/*
 * PS5 vkQuake - the startup trace. See src/trace.hpp for why it exists.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "trace.hpp"

#include "memory_diagnostics.hpp"
#include "../build/title_build_identity.h"

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

namespace ps5::debug
{
namespace
{
/* Inside the title's own folder, which the console mounts at /app0 and which the
 * trace files written by an earlier build proved is writable. */
constexpr const char *trace_path = "/app0/trace.txt";

/* Where the allocation report goes when the build carries the diagnostics
 * (PS5_MEMORY_DIAGNOSTICS=1, and the flag is part of the build identity, so a
 * diagnostics run is distinguishable from a normal one). Its own file rather than
 * the trace, because a failure line is written the moment an allocation fails -
 * which is what a run that dies inside a library leaves behind. Without the define
 * the header's other half makes all of this a no-op. */
constexpr const char *memory_path = "/app0/memory.txt";

pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;

void write(const char *line) noexcept
{
    // Audio and presentation report from different threads. Keep each complete
    // append together, including the newline, on the console's mounted file.
    pthread_mutex_lock(&trace_mutex);
    std::FILE *file = std::fopen(trace_path, "a");
    if (file != nullptr)
    {
        std::fputs(line, file);
        std::fputc('\n', file);
        std::fflush(file);
        std::fclose(file);
    }
    pthread_mutex_unlock(&trace_mutex);
}

/* Point the C streams at the trace file, before main.
 *
 * Why a constructor. vkQuake reports through stdout and stderr, and on this
 * console neither reaches anything: not the kernel log, not the title's folder,
 * not FTP. The second console run died inside Sys_Error - the engine had failed
 * to load its game data - and the console's report said which function called
 * Sys_Error and nothing about why, because Sys_Error's own message went to a
 * stream with no reader. The backtrace was readable and the reason was not.
 *
 * Appending rather than truncating, so the sequence of a run survives. Exit does
 * not flush on this console, so the title's exit path flushes explicitly
 * (src/title_exit.cpp); see the constructor for why the streams are buffered.
 *
 * An earlier project on this console solved the same problem the same way. It is
 * a development aid and it is removed when the reason for it is gone;
 * docs/ACTIVE.md says when that is.
 */
/* The streams' buffers, and the thread that empties them four times a second. */
char out_buffer[64 * 1024];
char err_buffer[64 * 1024];

void *flush_streams(void *) noexcept
{
    for (;;)
    {
        usleep(250000);
        std::fflush(stdout);
        std::fflush(stderr);
    }
    return nullptr;
}

struct ConsoleStreams
{
    ConsoleStreams() noexcept
    {
        /* Buffered, and written out by a thread, not by the caller. These
         * streams were unbuffered, and the console's libc then hands each print
         * to the file in pieces: a console run measured the start map's
         * centre-print log -- a few hundred characters through Con_Printf -- at
         * 84-148 ms of one frame, the owner's New Game stutter (port evidence
         * m6-r43-newgame-svc), and one 1.9 KB driver line at 1.6-5.8 s. One
         * write of the same bytes costs about a millisecond. What the
         * unbuffered streams protected -- an error printed just before exit --
         * is kept by ps5_title_exit's fflush(nullptr), which every exit path
         * takes; a hard crash can lose what the last flush interval held. */
        const bool out = std::freopen(trace_path, "a", stdout) != nullptr;
        const bool err = std::freopen(trace_path, "a", stderr) != nullptr;
        if (out)
            std::setvbuf(stdout, out_buffer, _IOFBF, sizeof out_buffer);
        if (err)
            std::setvbuf(stderr, err_buffer, _IOFBF, sizeof err_buffer);
        pthread_t flusher;
        if ((out || err) && pthread_create(&flusher, nullptr, flush_streams, nullptr) == 0)
            pthread_detach(flusher);
        else
        {
            /* No thread to write them out: a line at a time, still one write
             * each rather than pieces. */
            if (out)
                std::setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);
            if (err)
                std::setvbuf(stderr, nullptr, _IOLBF, BUFSIZ);
        }
        /* Opened here, before main, so the report covers the whole title: the
         * engine's start-up allocations are half of what a memory question is
         * about. finish() runs at exit when the title gets that far; when it does
         * not, the failure lines are already on disk. */
        ps5::memory::init(memory_path, PS5_VKQUAKE_BUILD_ID);
        std::atexit(ps5::memory::finish);
    }
};

const ConsoleStreams console_streams;
} // namespace

void mark(const char *step) noexcept
{
    write(step);
}

void mark_value(const char *step, long long value) noexcept
{
    char line[192];
    std::snprintf(line, sizeof(line), "%s = %lld", step, value);
    write(line);
}

void mark_init(const char *step, bool have_video, int width, int height) noexcept
{
    char line[192];
    std::snprintf(line, sizeof(line), "%s (video=%s width=%d height=%d)", step,
                  have_video ? "present" : "NULL", width, height);
    write(line);
}
} // namespace ps5::debug

/* The C door src/input_ps5.cpp writes through; see the note in trace.hpp. */
extern "C" void ps5_input_trace(const char *line) noexcept
{
    ps5::debug::mark(line);
}

/* The same door for the port layer's own C, which cannot name a C++ namespace.
 * platform/ps5/vk_globals.c is generated and uses this to report what the driver
 * answered, so a console run says how far the Vulkan path got rather than only
 * how far it did not. */
extern "C" void ps5_trace(const char *line) noexcept
{
    ps5::debug::mark(line);
}
