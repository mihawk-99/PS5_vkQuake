/*
 * PS5 vkQuake - the SDL2 surface vkQuake's engine actually uses.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this file exists, and why it is a header called SDL.h.
 *
 * vkQuake has no build without SDL. `Quake/q_stdinc.h:240` includes "SDL.h",
 * and every one of its 96 engine translation units reaches that line through
 * `quakedef.h`. Its platform files (gl_vidsdl.c, sys_sdl.c, in_sdl2.c,
 * snd_sdl.c, pl_linux.c, main_sdl.c) are written against SDL's windowing,
 * event, audio and file APIs and cannot be compiled on a console that has none
 * of them - those are replaced outright, by the port layer beside this file.
 *
 * What is left is a much smaller problem: the engine proper - the server, the
 * client, the renderer's non-platform half, the console, the cvar system - uses
 * SDL for exactly four things. Threads. Mutexes, conditions and semaphores.
 * A monotonic clock and a sleep. And an allocator and a handful of odds and
 * ends. None of that is windowing, and all of it maps onto pthreads and the
 * PS5's POSIX surface without pretending to be a desktop.
 *
 * So this is not an SDL port. It is the ~30 functions the engine calls, declared
 * with SDL's names and types so that upstream's sources compile unmodified, and
 * implemented on pthreads by platform/ps5/sdl_ps5.c. The rule this file obeys:
 * a symbol belongs here only if some file the port compiles calls it. It is a
 * record of measured coupling, not a compatibility library.
 *
 * vkQuake expects SDL2's names, not SDL3's. That is not a choice made here:
 * `quakedef.h:44-59` aliases the SDL3 spellings onto the SDL2 ones whenever
 * USE_SDL3 is undefined (`SDL_Mutex` -> `SDL_mutex`, `SDL_WaitCondition` ->
 * `SDL_CondWait`, `SDL_Semaphore` -> `SDL_sem`, `SDL_GetNumLogicalCPUCores` ->
 * `SDL_GetCPUCount`). Defining USE_SDL3 would move that requirement rather than
 * remove it, so this header provides the SDL2 spelling and leaves upstream's
 * aliases to do their work.
 *
 * What is deliberately absent: windows, surfaces, events, gamepads, audio
 * devices, and the Vulkan surface helpers. Those are the port layer's job and
 * live in the files that replace vkQuake's platform sources. Anything added
 * here later needs the same justification - a caller that exists.
 */

#ifndef PS5_SDL_COMPAT_H
#define PS5_SDL_COMPAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* --------------------------------------------------------------------------
     * SDL_stdinc.h - the types and the allocator.
     *
     * The engine uses these spellings throughout (Uint8 in the demos, Uint32 in the
     * CRC and the palette), so they keep SDL's names and their exact widths. SDL_bool
     * is SDL2's enum, not C99 bool: upstream compares against SDL_TRUE, and the
     * width of the enum is part of what those comparisons were compiled against.
     * -------------------------------------------------------------------------- */

    typedef uint8_t Uint8;
    typedef int8_t Sint8;
    typedef uint16_t Uint16;
    typedef int16_t Sint16;
    typedef uint32_t Uint32;
    typedef int32_t Sint32;
    typedef uint64_t Uint64;
    typedef int64_t Sint64;

    typedef enum
    {
        SDL_FALSE = 0,
        SDL_TRUE = 1
    } SDL_bool;

/* The two format macros the engine's logging uses. The PS5 target is LP64 like
 * every other 64-bit platform vkQuake builds for, so these are the standard
 * ones and not a redefinition of what a uint32_t prints as. */
#ifndef SDL_PRIu32
#define SDL_PRIu32 "u"
#endif
#ifndef SDL_PRIu64
#define SDL_PRIu64 "llu"
#endif
#ifndef SDL_PRIs32
#define SDL_PRIs32 "d"
#endif

    void *SDL_malloc(size_t size);
    void *SDL_calloc(size_t nmemb, size_t size);
    void *SDL_realloc(void *mem, size_t size);
    void SDL_free(void *mem);

    /* --------------------------------------------------------------------------
     * SDL_error.h
     *
     * SDL_GetError is called 25 times in this tree, most of them formatting a
     * failure message into Sys_Error. The string is stored per thread because that
     * is what callers assume when they read it immediately after a failing call.
     * -------------------------------------------------------------------------- */

    void SDL_SetError(const char *fmt, ...);
    const char *SDL_GetError(void);
    void SDL_ClearError(void);

    /* --------------------------------------------------------------------------
     * Threads, mutexes, conditions, semaphores.
     *
     * These are the reason this header exists: 53 SDL_LockMutex and 56
     * SDL_UnlockMutex calls across the renderer, the sound mixer, the console and
     * the texture manager. They map one-to-one onto pthread primitives, with the
     * one addition SDL makes that pthreads does not - a recursive mutex, which is
     * what SDL_CreateMutex returns and what this must return too, because the
     * engine locks the same mutex twice on some paths.
     * -------------------------------------------------------------------------- */

    typedef struct SDL_mutex SDL_mutex;
    typedef struct SDL_cond SDL_cond;
    typedef struct SDL_sem SDL_sem;
    typedef struct SDL_Thread SDL_Thread;

/* The sentinel SDL_CondWaitTimeout takes to mean "wait forever". gl_model.c
 * passes it on three of its waits, so the value is part of the contract, not a
 * convenience: it is SDL2's, which is every bit set. */
#define SDL_MUTEX_MAXWAIT (~(Uint32)0)

    SDL_mutex *SDL_CreateMutex(void);
    int SDL_LockMutex(SDL_mutex *mutex);
    int SDL_UnlockMutex(SDL_mutex *mutex);
    void SDL_DestroyMutex(SDL_mutex *mutex);

    SDL_cond *SDL_CreateCond(void);
    int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex);
    int SDL_CondWaitTimeout(SDL_cond *cond, SDL_mutex *mutex, Uint32 ms);
    int SDL_CondBroadcast(SDL_cond *cond);
    int SDL_CondSignal(SDL_cond *cond);
    void SDL_DestroyCond(SDL_cond *cond);

    SDL_sem *SDL_CreateSemaphore(Uint32 initial_value);
    int SDL_SemWait(SDL_sem *sem);
    int SDL_SemTryWait(SDL_sem *sem);
    int SDL_SemPost(SDL_sem *sem);
    Uint32 SDL_SemValue(SDL_sem *sem);
    void SDL_DestroySemaphore(SDL_sem *sem);

    /* SDL's thread entry returns int; SDL_CreateThread takes a name for debuggers
     * and may ignore it. tasks.c passes one and detaches the thread, so both
     * lifecycle calls are needed. */
    typedef int (*SDL_ThreadFunction)(void *data);

    SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data);
    void SDL_WaitThread(SDL_Thread *thread, int *status);
    void SDL_DetachThread(SDL_Thread *thread);

    /* --------------------------------------------------------------------------
     * Timing and CPU count.
     *
     * Sys_DoubleTime is the engine's only clock and is built on the performance
     * counter; SDL_Delay is its only sleep and appears in the frame pacer, the
     * network loop and the sound path. Both are used every frame.
     * -------------------------------------------------------------------------- */

    void SDL_Delay(Uint32 ms);
    Uint64 SDL_GetPerformanceCounter(void);
    Uint64 SDL_GetPerformanceFrequency(void);
    int SDL_GetCPUCount(void);
    int SDL_GetSystemRAM(void);
    const char *SDL_GetPlatform(void);

    /* A monotonic wall clock in seconds. Upstream gets this from gettimeofday; the
     * engine uses it for the console's timestamps and the demo recorder. */
    Uint32 SDL_GetTicks(void);
    Uint64 SDL_GetTicks64(void);

    /* --------------------------------------------------------------------------
     * CPU feature queries.
     *
     * gl_rmisc.c gates the SIMD paths on these, and the gate is a runtime one:
     * reporting false turns the paths off and is always correct, reporting true on
     * a CPU without the instructions is not. The PS5's CPU is Zen 2, which has both,
     * so both report true - but they report it from the target's own capability
     * rather than from a constant, so a wrong answer here cannot be silent.
     * -------------------------------------------------------------------------- */

    SDL_bool SDL_HasSSE(void);
    SDL_bool SDL_HasSSE2(void);
    SDL_bool SDL_HasAVX(void);
    SDL_bool SDL_HasAVX2(void);

    /* --------------------------------------------------------------------------
     * Filesystem.
     *
     * SDL_GetPrefPath is how vkQuake finds a writable per-user directory, and on a
     * console there is no user and no home directory: the title's own folder is the
     * only writable place, and it is a constant. The port replaces all three of its
     * call sites rather than answering with a lie, so this is here only because
     * common.c and console.c reference the symbol; it returns /app0.
     *
     * SDL_RWops is SDL2's stream object. The engine uses it in exactly one place -
     * the miniz read callback that opens vkQuake's locale archive - and the port
     * replaces that call site with stdio. It is declared, and implemented for the
     * file case, so the tree links either way.
     * -------------------------------------------------------------------------- */

    char *SDL_GetPrefPath(const char *org, const char *app);
    char *SDL_GetBasePath(void);

    typedef struct SDL_RWops SDL_RWops;

    struct SDL_RWops
    {
        Sint64 (*size)(SDL_RWops *context);
        Sint64 (*seek)(SDL_RWops *context, Sint64 offset, int whence);
        size_t (*read)(SDL_RWops *context, void *ptr, size_t size, size_t maxnum);
        size_t (*write)(SDL_RWops *context, const void *ptr, size_t size, size_t num);
        int (*close)(SDL_RWops *context);
        Uint32 type;
        void *hidden;
    };

#define SDL_RWOPS_UNKNOWN 0u
#define SDL_RW_SEEK_SET 0
#define SDL_RW_SEEK_CUR 1
#define SDL_RW_SEEK_END 2

/* SDL2 spells the same three constants twice, and this tree uses both spellings:
 * the SDL_RW_* form in the frame-format code and the RW_SEEK_* form in common.c's
 * miniz callback. */
#define RW_SEEK_SET SDL_RW_SEEK_SET
#define RW_SEEK_CUR SDL_RW_SEEK_CUR
#define RW_SEEK_END SDL_RW_SEEK_END

    SDL_RWops *SDL_RWFromFile(const char *file, const char *mode);
    SDL_RWops *SDL_RWFromMem(void *mem, int size);
    SDL_RWops *SDL_RWFromConstMem(const void *mem, int size);
    Sint64 SDL_RWsize(SDL_RWops *context);
    Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence);
    size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum);
    size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t num);
    int SDL_RWclose(SDL_RWops *context);

    /* --------------------------------------------------------------------------
     * The remaining odds and ends, each because a caller exists.
     *
     *   Clipboard     console.c and cl_main.c copy text out of the console.
     *   MessageBox    common.c and pl_linux.c report a fatal error before exiting.
     *   LoadObject    steam.c resolves the Steam API at run time. Steam is not
     *                 present on this console, so these fail cleanly and the
     *                 engine's own Steam_Init handles that - which is the same path
     *                 a desktop without Steam takes.
     * -------------------------------------------------------------------------- */

    int SDL_SetClipboardText(const char *text);
    char *SDL_GetClipboardText(void);

    /* The mouse exists on this console only as an absence: no pointer, no buttons.
     * menu.c asks for its position and the port answers zero with no buttons held,
     * which is what a desktop with no mouse plugged in reports. The engine already
     * handles that case - it is the same state as a machine whose mouse never
     * moved - so the port does not need to remove the call, only to answer it
     * truthfully. */
    Uint32 SDL_GetMouseState(int *x, int *y);
    Uint32 SDL_GetGlobalMouseState(int *x, int *y);

    typedef struct SDL_MessageBoxButtonData
    {
        Uint32 flags;
        int buttonid;
        const char *text;
    } SDL_MessageBoxButtonData;

    typedef struct SDL_MessageBoxColor
    {
        Uint8 r, g, b;
    } SDL_MessageBoxColor;

    typedef struct SDL_MessageBoxColorScheme
    {
        SDL_MessageBoxColor colors[5];
    } SDL_MessageBoxColorScheme;

    typedef struct SDL_MessageBoxData
    {
        Uint32 flags;
        void *window;
        const char *title;
        const char *message;
        int numbuttons;
        const SDL_MessageBoxButtonData *buttons;
        const SDL_MessageBoxColorScheme *colorScheme;
    } SDL_MessageBoxData;

#define SDL_MESSAGEBOX_ERROR 0x00000010u
#define SDL_MESSAGEBOX_WARNING 0x00000020u
#define SDL_MESSAGEBOX_INFORMATION 0x00000040u

    int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message,
                                 void *window);
    int SDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid);

    void *SDL_LoadObject(const char *sofile);
    void *SDL_LoadFunction(void *handle, const char *name);
    void SDL_UnloadObject(void *handle);

    /* The engine prints through its own console, but SDL's log is called on the
     * startup path before that exists, so it has to go somewhere - the trace file. */
    typedef enum
    {
        SDL_LOG_PRIORITY_VERBOSE = 1,
        SDL_LOG_PRIORITY_DEBUG,
        SDL_LOG_PRIORITY_INFO,
        SDL_LOG_PRIORITY_WARN,
        SDL_LOG_PRIORITY_ERROR,
        SDL_LOG_PRIORITY_CRITICAL
    } SDL_LogPriority;

    void SDL_LogSetAllPriority(SDL_LogPriority priority);
    void SDL_LogSetPriority(int category, SDL_LogPriority priority);
    void SDL_Log(const char *fmt, ...);
    void SDL_LogError(int category, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* PS5_SDL_COMPAT_H */
