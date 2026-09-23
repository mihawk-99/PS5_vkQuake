/*
 * PS5 vkQuake - the SDL2 surface vkQuake's engine uses, on pthreads.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The declarations and the reasoning are in platform/ps5/SDL.h beside this file;
 * what belongs here is the behaviour, and three of these functions have
 * behaviour that is easy to get subtly wrong.
 *
 * SDL_CreateMutex returns a RECURSIVE mutex. This is not an implementation
 * detail: SDL2 documents its mutex as recursive, and vkQuake relies on it - the
 * texture manager locks texmgr_mutex inside a function that its own caller has
 * already locked, and so does the console. A pthread mutex left at its default
 * would deadlock there, on the console, in the middle of a frame, with no error
 * to read. So the mutex here is created with PTHREAD_MUTEX_RECURSIVE and the
 * host test beside this file locks one twice to keep it that way.
 *
 * SDL_CreateThread's thread function returns int, and SDL_WaitThread hands that
 * value back through a pointer. A detached thread has nobody to hand it to, and
 * a joined one has somebody who may not have arrived yet, so the handle can be
 * freed by either side and not by both. A reference count answers that: the
 * handle holds one, the thread holds one, and whoever lets go last frees it.
 *
 * SDL_GetPerformanceFrequency is a constant here rather than a measured value
 * because the clock underneath it is CLOCK_MONOTONIC in nanoseconds - the same
 * clock the engine's Sys_DoubleTime divides, and the one the kernel guarantees
 * does not go backwards while the title runs.
 *
 * Everything in this file is deliberately small. It is not an SDL port and it
 * must never grow into one: the windowing, the events, the gamepad and the audio
 * device are the port layer's own files, written against the PS5's APIs, and
 * they do not come through here.
 */

#include "SDL.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Memory and error state.
 * ------------------------------------------------------------------------- */

void *SDL_malloc(size_t size)
{
    return malloc(size);
}

void *SDL_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *SDL_realloc(void *mem, size_t size)
{
    return realloc(mem, size);
}

void SDL_free(void *mem)
{
    free(mem);
}

/* The error string is per thread because that is how callers read it: they call
 * something, and if it fails they format SDL_GetError() into their own message
 * immediately. A shared buffer would let one thread overwrite another's reason
 * between the two. */
static _Thread_local char sdl_error[256];

void SDL_SetError(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(sdl_error, sizeof sdl_error, fmt, args);
    va_end(args);
}

const char *SDL_GetError(void)
{
    return sdl_error[0] ? sdl_error : "no error";
}

void SDL_ClearError(void)
{
    sdl_error[0] = '\0';
}

/* ---------------------------------------------------------------------------
 * Mutexes, conditions, semaphores.
 * ------------------------------------------------------------------------- */

/* What the engine asks of the kernel, counted. A console run measured a system
 * call at ~20 us on this console (getpid 20.1 us, clock_gettime 20.3 us) against
 * 12 ns for a user-mode TSC read and 16 ns for an uncontended mutex pair, so how
 * often these primitives can enter the kernel is a cost worth knowing per frame.
 * Relaxed atomic adds, no system call; reported on the present line
 * (ps5_window.c), which is one write every ten seconds. */
enum
{
    COUNT_CLOCK,
    COUNT_SEM_POST,
    COUNT_SEM_BLOCK,
    COUNT_SEM_TRY,
    COUNT_SEM_TRY_EMPTY,
    COUNT_CONTENDED,
    COUNT_COND_WAIT,
    COUNT_COND_WAKE,
    COUNT_DELAY,
    COUNT_KINDS
};
static _Atomic unsigned long long counts[COUNT_KINDS];
static void count(int kind)
{
    atomic_fetch_add_explicit(&counts[kind], 1, memory_order_relaxed);
}
/* A lock that says whether it had to wait: trylock first, then the real lock. */
static int lock_counted(pthread_mutex_t *mutex)
{
    if (pthread_mutex_trylock(mutex) == 0)
        return 0;
    count(COUNT_CONTENDED);
    return pthread_mutex_lock(mutex);
}

/* The means per frame since the previous call, as " name=value" fields. */
int ps5_sdl_counts_format(char *line, size_t bytes, unsigned long long frames)
{
    static const char *const names[COUNT_KINDS] = {"clock",     "sem_post",      "sem_block",
                                                   "sem_try",   "sem_try_empty", "contended",
                                                   "cond_wait", "cond_wake",     "delay"};
    static unsigned long long previous[COUNT_KINDS];
    size_t used = 0;
    for (int kind = 0; kind < COUNT_KINDS && used < bytes; ++kind)
    {
        const unsigned long long now = atomic_load_explicit(&counts[kind], memory_order_relaxed);
        const double per_frame = frames ? (double)(now - previous[kind]) / (double)frames : 0.0;
        previous[kind] = now;
        const int wrote =
            snprintf(line + used, bytes - used, " %s/frame=%.1f", names[kind], per_frame);
        if (wrote < 0)
            break;
        used += (size_t)wrote;
    }
    return (int)used;
}

struct SDL_mutex
{
    pthread_mutex_t handle;
};

struct SDL_cond
{
    pthread_cond_t handle;
};

struct SDL_sem
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    Uint32 count;
};

/* The probe's clock: see src/probe.c. The engine creates mutexes at several points
 * through Host_Init and this is the only one of them this project owns.
 *
 * Weak and checked for the same reason as the allocator's: the host test compiles
 * this file on its own, and a missing probe should mean no probe. */
extern void ps5_probe_watch(void) __attribute__((weak));

SDL_mutex *SDL_CreateMutex(void)
{
    if (ps5_probe_watch != NULL)
        ps5_probe_watch();
    SDL_mutex *mutex = malloc(sizeof *mutex);
    if (!mutex)
    {
        SDL_SetError("out of memory creating a mutex");
        return NULL;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    /* Recursive, because SDL's is and the engine depends on it. See the file
     * header: this is the one line that would turn a working frame into a
     * deadlock. */
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    const int created = pthread_mutex_init(&mutex->handle, &attr);
    pthread_mutexattr_destroy(&attr);

    if (created != 0)
    {
        free(mutex);
        SDL_SetError("pthread_mutex_init failed: %d", created);
        return NULL;
    }
    return mutex;
}

int SDL_LockMutex(SDL_mutex *mutex)
{
    if (!mutex)
        return -1;
    return lock_counted(&mutex->handle);
}

int SDL_UnlockMutex(SDL_mutex *mutex)
{
    if (!mutex)
        return -1;
    return pthread_mutex_unlock(&mutex->handle);
}

void SDL_DestroyMutex(SDL_mutex *mutex)
{
    if (!mutex)
        return;
    pthread_mutex_destroy(&mutex->handle);
    free(mutex);
}

SDL_cond *SDL_CreateCond(void)
{
    SDL_cond *cond = malloc(sizeof *cond);
    if (!cond)
    {
        SDL_SetError("out of memory creating a condition");
        return NULL;
    }
    if (pthread_cond_init(&cond->handle, NULL) != 0)
    {
        free(cond);
        SDL_SetError("pthread_cond_init failed");
        return NULL;
    }
    return cond;
}

int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex)
{
    if (!cond || !mutex)
        return -1;
    count(COUNT_COND_WAIT);
    return pthread_cond_wait(&cond->handle, &mutex->handle);
}

/* SDL's contract, and the reason vkQuake's own macro wraps this in `== 0`:
 * zero means the condition was signalled, non-zero means the wait timed out.
 * The caller cannot tell those apart from the return value alone otherwise, and
 * gl_model.c waits on a worker with a timeout and treats the two differently. */
int SDL_CondWaitTimeout(SDL_cond *cond, SDL_mutex *mutex, Uint32 ms)
{
    if (!cond || !mutex)
        return -1;
    count(COUNT_COND_WAIT);
    if (ms == SDL_MUTEX_MAXWAIT)
        return pthread_cond_wait(&cond->handle, &mutex->handle);

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)(ms / 1000u);
    deadline.tv_nsec += (long)(ms % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    const int result = pthread_cond_timedwait(&cond->handle, &mutex->handle, &deadline);
    return result == ETIMEDOUT ? 1 : result;
}

int SDL_CondBroadcast(SDL_cond *cond)
{
    if (!cond)
        return -1;
    count(COUNT_COND_WAKE);
    return pthread_cond_broadcast(&cond->handle);
}

int SDL_CondSignal(SDL_cond *cond)
{
    if (!cond)
        return -1;
    count(COUNT_COND_WAKE);
    return pthread_cond_signal(&cond->handle);
}

void SDL_DestroyCond(SDL_cond *cond)
{
    if (!cond)
        return;
    pthread_cond_destroy(&cond->handle);
    free(cond);
}

/* The semaphore is a mutex and a condition rather than sem_t. The payload SDK's
 * libc is a clean-room runtime whose POSIX surface is documented in
 * tooling/native/runtime/api-surface.txt, and building this from primitives that
 * are certainly there costs less than discovering that sem_timedwait is not. */
SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_sem *sem = malloc(sizeof *sem);
    if (!sem)
    {
        SDL_SetError("out of memory creating a semaphore");
        return NULL;
    }
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = initial_value;
    return sem;
}

int SDL_SemWait(SDL_sem *sem)
{
    if (!sem)
        return -1;
    lock_counted(&sem->mutex);
    while (sem->count == 0)
    {
        count(COUNT_SEM_BLOCK);
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int SDL_SemTryWait(SDL_sem *sem)
{
    if (!sem)
        return -1;
    count(COUNT_SEM_TRY);
    lock_counted(&sem->mutex);
    /* SDL returns SDL_MUTEX_TIMEDOUT rather than blocking, and vkQuake's macro
     * turns a zero return into true. Non-zero is the answer either way, but the
     * value is kept as SDL's. */
    const int result = sem->count == 0 ? 1 : 0;
    if (result == 0)
        sem->count--;
    else
        count(COUNT_SEM_TRY_EMPTY);
    pthread_mutex_unlock(&sem->mutex);
    return result;
}

int SDL_SemPost(SDL_sem *sem)
{
    if (!sem)
        return -1;
    count(COUNT_SEM_POST);
    lock_counted(&sem->mutex);
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

Uint32 SDL_SemValue(SDL_sem *sem)
{
    if (!sem)
        return 0;
    pthread_mutex_lock(&sem->mutex);
    const Uint32 value = sem->count;
    pthread_mutex_unlock(&sem->mutex);
    return value;
}

void SDL_DestroySemaphore(SDL_sem *sem)
{
    if (!sem)
        return;
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
    free(sem);
}

/* ---------------------------------------------------------------------------
 * Threads, and the reference count that lets a handle be freed once.
 * ------------------------------------------------------------------------- */

struct SDL_Thread
{
    pthread_t handle;
    SDL_ThreadFunction fn;
    void *data;
    int status;
    atomic_int references;
};

static void thread_release(SDL_Thread *thread)
{
    if (atomic_fetch_sub_explicit(&thread->references, 1, memory_order_acq_rel) == 1)
        free(thread);
}

static void *thread_trampoline(void *arg)
{
    SDL_Thread *thread = arg;
    thread->status = thread->fn(thread->data);
    thread_release(thread);
    return NULL;
}

SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data)
{
    (void)name; /* SDL uses this to name the thread for a debugger; the console
                 * has no debugger attached and the field does not exist. */
    if (!fn)
    {
        SDL_SetError("SDL_CreateThread needs a function");
        return NULL;
    }

    SDL_Thread *thread = malloc(sizeof *thread);
    if (!thread)
    {
        SDL_SetError("out of memory creating a thread");
        return NULL;
    }
    thread->fn = fn;
    thread->data = data;
    thread->status = 0;
    /* Two owners from the start: the caller's handle and the running thread. */
    atomic_init(&thread->references, 2);

    if (pthread_create(&thread->handle, NULL, thread_trampoline, thread) != 0)
    {
        free(thread);
        SDL_SetError("pthread_create failed");
        return NULL;
    }
    return thread;
}

void SDL_WaitThread(SDL_Thread *thread, int *status)
{
    if (!thread)
        return;
    /* pthread_join returns after the thread function has returned, so the status
     * written by the trampoline is visible here without further synchronisation. */
    pthread_join(thread->handle, NULL);
    if (status)
        *status = thread->status;
    thread_release(thread);
}

void SDL_DetachThread(SDL_Thread *thread)
{
    if (!thread)
        return;
    pthread_detach(thread->handle);
    /* The thread keeps its own reference and frees the handle when it finishes;
     * the caller has just given up the other one. */
    thread_release(thread);
}

/* ---------------------------------------------------------------------------
 * Time and the machine's shape.
 * ------------------------------------------------------------------------- */

void SDL_Delay(Uint32 ms)
{
    count(COUNT_DELAY);
    struct timespec remaining;
    remaining.tv_sec = (time_t)(ms / 1000u);
    remaining.tv_nsec = (long)(ms % 1000u) * 1000000L;
    /* nanosleep is interrupted by a signal and reports the remainder; the
     * engine's frame pacer asks for a millisecond at a time and needs the sleep
     * to actually happen, so it is resumed rather than abandoned. */
    while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR)
        ;
}

Uint64 SDL_GetPerformanceCounter(void)
{
    struct timespec now;
    count(COUNT_CLOCK);
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (Uint64)now.tv_sec * 1000000000ull + (Uint64)now.tv_nsec;
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return 1000000000ull; /* CLOCK_MONOTONIC counts nanoseconds. */
}

Uint32 SDL_GetTicks(void)
{
    return (Uint32)(SDL_GetPerformanceCounter() / 1000000ull);
}

Uint64 SDL_GetTicks64(void)
{
    return SDL_GetPerformanceCounter() / 1000000ull;
}

int SDL_GetCPUCount(void)
{
    /* The console reports the cores the title may use, which is fewer than the
     * hardware has: the system reserves some. Reading it rather than assuming a
     * number keeps the engine's worker count honest - tasks.c clamps this to 32
     * and creates that many render workers. */
    const long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores < 1)
        return 1;
    return (int)cores;
}

int SDL_GetSystemRAM(void)
{
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGESIZE);
    if (pages < 1 || page_size < 1)
        return 0;
    return (int)((pages * page_size) / (1024 * 1024));
}

const char *SDL_GetPlatform(void)
{
    return "PS5";
}

/* ---------------------------------------------------------------------------
 * CPU features.
 *
 * The engine gates its SIMD paths on these at run time: false is always a
 * correct answer and only costs speed, while true on a CPU without the
 * instructions is a crash. The PS5's CPU is a Zen 2 and has all four, so all
 * four report true - but they report it from the target's own capability, read
 * once, so the answer cannot drift from the machine underneath it.
 * ------------------------------------------------------------------------- */

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>

/* Read one feature bit, by leaf, subleaf, register and bit.
 *
 * __cpuid_count is used rather than __get_cpuid, and that is not a style
 * preference: __get_cpuid(7, ...) returns zeroes for every register on this
 * toolchain, so AVX2 - leaf 7, EBX bit 5 - reads as absent on a CPU that has it.
 * The host test beside this file compares these answers against the compiler's
 * own __builtin_cpu_supports and failed on exactly that, which is how the bug was
 * found rather than inferred. A wrong answer here does not crash: it silently
 * turns the engine's AVX2 paths off, which is why it needed a test that compares
 * two independent sources instead of one that merely checks the implications
 * between them. */
static unsigned feature_bit(unsigned leaf, unsigned subleaf, unsigned reg, unsigned bit)
{
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
    const unsigned value = reg == 0 ? eax : reg == 1 ? ebx : reg == 2 ? ecx : edx;
    return (value >> bit) & 1u;
}
#else
static unsigned feature_bit(unsigned leaf, unsigned subleaf, unsigned reg, unsigned bit)
{
    (void)leaf;
    (void)subleaf;
    (void)reg;
    (void)bit;
    return 0;
}
#endif

/* The leaves and bits are the architecture's, not this port's: leaf 1 EDX for
 * SSE and SSE2, leaf 1 ECX for AVX (which also needs OSXSAVE, but the engine's
 * use of these is a hint and the PS5 has no OS to withhold it), and leaf 7
 * subleaf 0 EBX for AVX2. */
SDL_bool SDL_HasSSE(void)
{
    return feature_bit(1, 0, 3, 25) ? SDL_TRUE : SDL_FALSE;
}
SDL_bool SDL_HasSSE2(void)
{
    return feature_bit(1, 0, 3, 26) ? SDL_TRUE : SDL_FALSE;
}
SDL_bool SDL_HasAVX(void)
{
    return feature_bit(1, 0, 2, 28) ? SDL_TRUE : SDL_FALSE;
}
SDL_bool SDL_HasAVX2(void)
{
    return feature_bit(7, 0, 1, 5) ? SDL_TRUE : SDL_FALSE;
}

/* ---------------------------------------------------------------------------
 * Filesystem.
 *
 * The console has no user, no home directory and no per-application data area:
 * the title's own folder is the only writable place, and it is a constant. These
 * exist because common.c and console.c name the symbols; the port replaces their
 * call sites, and these answer with the truth rather than a plausible-looking
 * path on a machine that has no such directory.
 * ------------------------------------------------------------------------- */

char *SDL_GetPrefPath(const char *org, const char *app)
{
    (void)org;
    (void)app;
    const char *path = "/app0/";
    char *copy = malloc(strlen(path) + 1);
    if (copy)
        strcpy(copy, path);
    return copy;
}

char *SDL_GetBasePath(void)
{
    const char *path = "/app0/";
    char *copy = malloc(strlen(path) + 1);
    if (copy)
        strcpy(copy, path);
    return copy;
}

/* The one stream object the engine uses. It is stdio underneath, and it exists
 * so that the miniz callback in common.c has something to call. */
struct SDL_RWopsStdio
{
    FILE *file;
};

typedef struct SDL_RWopsStdio SDL_RWopsStdio;

static Sint64 rw_stdio_size(SDL_RWops *context)
{
    SDL_RWopsStdio *io = context->hidden;
    const long here = ftell(io->file);
    if (here < 0)
        return -1;
    if (fseek(io->file, 0, SEEK_END) != 0)
        return -1;
    const long end = ftell(io->file);
    fseek(io->file, here, SEEK_SET);
    return end < 0 ? -1 : (Sint64)end;
}

static Sint64 rw_stdio_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    SDL_RWopsStdio *io = context->hidden;
    const int base = whence == SDL_RW_SEEK_SET   ? SEEK_SET
                     : whence == SDL_RW_SEEK_CUR ? SEEK_CUR
                                                 : SEEK_END;
    if (fseek(io->file, (long)offset, base) != 0)
        return -1;
    return (Sint64)ftell(io->file);
}

static size_t rw_stdio_read(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    SDL_RWopsStdio *io = context->hidden;
    return fread(ptr, size, maxnum, io->file);
}

static size_t rw_stdio_write(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    SDL_RWopsStdio *io = context->hidden;
    return fwrite(ptr, size, num, io->file);
}

static int rw_stdio_close(SDL_RWops *context)
{
    SDL_RWopsStdio *io = context->hidden;
    const int result = io->file ? fclose(io->file) : 0;
    free(io);
    free(context);
    return result;
}

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode)
{
    if (!file || !mode)
    {
        SDL_SetError("SDL_RWFromFile needs a path and a mode");
        return NULL;
    }

    FILE *handle = fopen(file, mode);
    if (!handle)
    {
        SDL_SetError("could not open %s: %s", file, strerror(errno));
        return NULL;
    }

    SDL_RWops *context = calloc(1, sizeof *context);
    SDL_RWopsStdio *io = calloc(1, sizeof *io);
    if (!context || !io)
    {
        free(context);
        free(io);
        fclose(handle);
        SDL_SetError("out of memory opening %s", file);
        return NULL;
    }

    io->file = handle;
    context->size = rw_stdio_size;
    context->seek = rw_stdio_seek;
    context->read = rw_stdio_read;
    context->write = rw_stdio_write;
    context->close = rw_stdio_close;
    context->type = SDL_RWOPS_UNKNOWN;
    context->hidden = io;
    return context;
}

/* The in-memory variants are declared because SDL's header has them; nothing the
 * port compiles calls them, and a caller that appeared would need a real
 * implementation rather than this refusal. */
SDL_RWops *SDL_RWFromMem(void *mem, int size)
{
    (void)mem;
    (void)size;
    SDL_SetError("SDL_RWFromMem is not implemented on this port");
    return NULL;
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, int size)
{
    (void)mem;
    (void)size;
    SDL_SetError("SDL_RWFromConstMem is not implemented on this port");
    return NULL;
}

Sint64 SDL_RWsize(SDL_RWops *context)
{
    return context && context->size ? context->size(context) : -1;
}

Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence)
{
    return context && context->seek ? context->seek(context, offset, whence) : -1;
}

size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    return context && context->read ? context->read(context, ptr, size, maxnum) : 0;
}

size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    return context && context->write ? context->write(context, ptr, size, num) : 0;
}

int SDL_RWclose(SDL_RWops *context)
{
    return context && context->close ? context->close(context) : -1;
}

/* ---------------------------------------------------------------------------
 * The odds and ends.
 * ------------------------------------------------------------------------- */

/* There is no clipboard on this console. Answering with an empty string is what
 * a desktop with an empty clipboard reports, which is a state the engine already
 * handles; answering NULL is not. */
int SDL_SetClipboardText(const char *text)
{
    (void)text;
    return 0;
}

char *SDL_GetClipboardText(void)
{
    char *empty = malloc(1);
    if (empty)
        empty[0] = '\0';
    return empty;
}

/* A modal message box is a desktop idea. The engine uses it for failures it
 * cannot recover from, and this port's equivalent is the trace file - which is
 * where stderr already points. So the message is written there, in the same
 * shape, and the call returns rather than pretending a user dismissed it. */
int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message, void *window)
{
    (void)flags;
    (void)window;
    fprintf(stderr, "messagebox: %s: %s\n", title ? title : "", message ? message : "");
    fflush(stderr);
    return 0;
}

int SDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    if (messageboxdata)
        fprintf(stderr, "messagebox: %s: %s\n", messageboxdata->title ? messageboxdata->title : "",
                messageboxdata->message ? messageboxdata->message : "");
    fflush(stderr);
    if (buttonid)
        *buttonid = 0;
    return 0;
}

/* Steam is not on this console and the title cannot load a module at run time -
 * ../PS5_Vulkan established that every dlopen and sceKernelLoadStartModule of a
 * repository-built .so is refused. So these fail, and they fail the way a desktop
 * without Steam fails: the caller checks for NULL and disables the Steam path.
 * Reporting success here would be worse than useless, because the next call would
 * be a null function pointer. */
void *SDL_LoadObject(const char *sofile)
{
    SDL_SetError("this title cannot load %s at run time", sofile ? sofile : "(null)");
    return NULL;
}

void *SDL_LoadFunction(void *handle, const char *name)
{
    (void)handle;
    SDL_SetError("no loaded object provides %s", name ? name : "(null)");
    return NULL;
}

void SDL_UnloadObject(void *handle)
{
    (void)handle;
}

/* SDL's log goes to stderr, and src/main.cpp has already pointed stderr at
 * /app0/trace.txt before the engine starts. That is the only record a console run
 * leaves, so nothing written here is dropped. */
void SDL_LogSetAllPriority(SDL_LogPriority priority)
{
    (void)priority;
}

void SDL_LogSetPriority(int category, SDL_LogPriority priority)
{
    (void)category;
    (void)priority;
}

void SDL_Log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

void SDL_LogError(int category, const char *fmt, ...)
{
    (void)category;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

/* ---------------------------------------------------------------------------
 * The mouse, which this console does not have.
 *
 * menu.c asks for its position every frame. Reporting the origin with no buttons
 * held is the state a desktop reports when no mouse is attached, and the engine
 * handles that state already - the port does not need to remove the call, only to
 * answer it truthfully.
 * ------------------------------------------------------------------------- */

Uint32 SDL_GetMouseState(int *x, int *y)
{
    if (x)
        *x = 0;
    if (y)
        *y = 0;
    return 0;
}

Uint32 SDL_GetGlobalMouseState(int *x, int *y)
{
    return SDL_GetMouseState(x, y);
}

/* ---------------------------------------------------------------------------
 * Lifecycle, the version, and the browser.
 *
 * The header says why these are here and why the version is 2.0.0. What is worth
 * repeating is that SDL_Init on this console has nothing to bring up: the display
 * is opened by whoever needs it, the pad by whoever needs that, and the audio
 * device by the sound backend. This only records that the engine asked.
 * ------------------------------------------------------------------------- */

static Uint32 sdl_initialised;

int SDL_Init(Uint32 flags)
{
    sdl_initialised |= flags;
    return 0; /* SDL2's contract is 0 for success, and main_sdl.c tests >= 0. */
}

void SDL_Quit(void)
{
    sdl_initialised = 0;
}

void SDL_GetVersion(SDL_version *ver)
{
    if (!ver)
        return;
    ver->major = SDL_MAJOR_VERSION;
    ver->minor = SDL_MINOR_VERSION;
    ver->patch = SDL_PATCHLEVEL;
}

/* There is no browser, no shell and no URL handler on this console. Reporting
 * failure is what a desktop with no browser reports, which is a case the caller
 * already handles; reporting success would leave it waiting for something that
 * will never happen. */
int SDL_OpenURL(const char *url)
{
    SDL_SetError("this console has nothing to open %s with", url ? url : "(null)");
    return -1;
}

/* ---------------------------------------------------------------------------
 * Surfaces, for the window icon that a console does not have.
 *
 * SDL_RWFromConstMem above is what makes this path unreachable: it refuses, and
 * PL_SetWindowIcon returns on the NULL before it decodes anything. These are
 * defined so the reference resolves and the link succeeds.
 * ------------------------------------------------------------------------- */

SDL_Surface *SDL_LoadBMP_RW(SDL_RWops *src, int freesrc)
{
    if (src && freesrc)
        SDL_RWclose(src);
    SDL_SetError("this console sets no window icon");
    return NULL;
}

/* Packing an RGB triple into the surface's format. The console has one format and
 * the window-icon path never asks, but the arithmetic is the honest one rather
 * than a constant. */
Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
    (void)format;
    return ((Uint32)0xFFu << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}

int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key)
{
    (void)surface;
    (void)flag;
    (void)key;
    return 0;
}

void SDL_FreeSurface(SDL_Surface *surface)
{
    free(surface);
}

void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
    (void)window;
    (void)icon;
}
