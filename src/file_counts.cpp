/* PS5 vkQuake - count and time the engine's file access, for the hitch report.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A console run found frames of 92 and 155 ms after Single Player > New Game in
 * which the driver created, compiled and allocated nothing (port evidence
 * m6-r43-newgame). A system call costs ~20 us on this console and the engine
 * reads its data through stdio, so fopen, fread, fseek and fclose are wrapped at
 * link time (tools/build-title.sh) and timed with the TSC; ps5_window.c reports a
 * hitch frame's share. Relaxed atomics and two TSC reads a call: nothing here
 * enters the kernel that the call itself did not. */
#include <atomic>
#include <cstdint>
#include <cstdio>

extern "C"
{
    std::FILE *__real_fopen(const char *, const char *);
    std::size_t __real_fread(void *, std::size_t, std::size_t, std::FILE *);
    int __real_fseek(std::FILE *, long, int);
    int __real_fclose(std::FILE *);
    std::uint64_t sceKernelReadTsc(void) __attribute__((weak));
}

namespace
{
std::atomic<unsigned long long> opens{0}, reads{0}, bytes{0}, ticks{0};

std::uint64_t now()
{
    return sceKernelReadTsc != nullptr ? sceKernelReadTsc() : 0;
}

void spent(std::uint64_t from)
{
    if (from != 0)
        ticks.fetch_add(now() - from, std::memory_order_relaxed);
}
} // namespace

extern "C" std::FILE *__wrap_fopen(const char *path, const char *mode)
{
    const std::uint64_t from = now();
    std::FILE *const file = __real_fopen(path, mode);
    opens.fetch_add(1, std::memory_order_relaxed);
    spent(from);
    return file;
}

extern "C" std::size_t __wrap_fread(void *into, std::size_t size, std::size_t count,
                                    std::FILE *file)
{
    const std::uint64_t from = now();
    const std::size_t got = __real_fread(into, size, count, file);
    reads.fetch_add(1, std::memory_order_relaxed);
    bytes.fetch_add(got * size, std::memory_order_relaxed);
    spent(from);
    return got;
}

extern "C" int __wrap_fseek(std::FILE *file, long offset, int whence)
{
    const std::uint64_t from = now();
    const int result = __real_fseek(file, offset, whence);
    spent(from);
    return result;
}

extern "C" int __wrap_fclose(std::FILE *file)
{
    const std::uint64_t from = now();
    const int result = __real_fclose(file);
    spent(from);
    return result;
}

/* Totals so far: opens, reads, bytes read, and TSC ticks spent in the four. */
extern "C" void ps5_file_counts(unsigned long long *open_count, unsigned long long *read_count,
                                unsigned long long *byte_count, unsigned long long *tick_count)
{
    *open_count = opens.load(std::memory_order_relaxed);
    *read_count = reads.load(std::memory_order_relaxed);
    *byte_count = bytes.load(std::memory_order_relaxed);
    *tick_count = ticks.load(std::memory_order_relaxed);
}
