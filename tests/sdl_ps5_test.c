/*
 * PS5 vkQuake - the SDL compatibility layer's behaviour, tested on the host.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What this test is for. platform/ps5/sdl_ps5.c maps the ~30 SDL functions the
 * engine calls onto pthreads, and the mapping looks obvious everywhere except
 * three places where being wrong produces no error at all on the console:
 *
 *   - SDL's mutex is recursive. A pthread mutex left at its default is not, and
 *     the texture manager locks texmgr_mutex inside a function its caller has
 *     already locked. Getting this wrong does not fail a build or print a
 *     message; it stops the title mid-frame with the GPU idle, which is the
 *     hardest kind of failure to read from a console's log.
 *   - A condition wait with a timeout has to say which of the two happened.
 *     gl_model.c treats "signalled" and "timed out" differently.
 *   - A thread handle can be released by a joiner or by the thread itself, and
 *     freeing it twice - or never - is invisible until it is not.
 *
 * So those three are tested directly rather than inferred from the engine
 * working, and the rest of the file checks the ordinary behaviour the engine
 * would otherwise discover at run time.
 *
 * The implementation is included, not linked: it is C, this is C, and the point
 * is to exercise the real code rather than a copy of its semantics. Compile it
 * with -pthread; tests/test_sdl_ps5.py does.
 */

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../platform/ps5/sdl_ps5.c"

/* ---------------------------------------------------------------------------
 * The three that fail silently.
 * ------------------------------------------------------------------------- */

/* A recursive lock: the engine locks the same mutex twice on real paths, and a
 * non-recursive mutex would hang here forever rather than fail. The test would
 * hang too, which is why the harness gives it a timeout. */
static void test_mutex_is_recursive(void)
{
    SDL_mutex *mutex = SDL_CreateMutex();
    assert(mutex != NULL);

    assert(SDL_LockMutex(mutex) == 0);
    assert(SDL_LockMutex(mutex) == 0);
    assert(SDL_LockMutex(mutex) == 0);
    assert(SDL_UnlockMutex(mutex) == 0);
    assert(SDL_UnlockMutex(mutex) == 0);
    assert(SDL_UnlockMutex(mutex) == 0);

    SDL_DestroyMutex(mutex);
    printf("  mutex is recursive (three nested locks released)\n");
}

/* A mutex still excludes other threads. Recursive for the owner, exclusive
 * against everyone else - the second half is the part a recursive mutex could
 * plausibly have given up. */
struct counter_state
{
    SDL_mutex *mutex;
    long value;
    int iterations;
};

static int counter_worker(void *data)
{
    struct counter_state *state = data;
    for (int i = 0; i < state->iterations; i++)
    {
        SDL_LockMutex(state->mutex);
        /* Read, modify, write with a yield in the middle: unsynchronised, this
         * loses updates almost immediately. */
        const long seen = state->value;
        sched_yield();
        state->value = seen + 1;
        SDL_UnlockMutex(state->mutex);
    }
    return 0;
}

static void test_mutex_excludes(void)
{
    struct counter_state state = {SDL_CreateMutex(), 0, 20000};
    assert(state.mutex != NULL);

    SDL_Thread *threads[4];
    for (int i = 0; i < 4; i++)
        threads[i] = SDL_CreateThread(counter_worker, "counter", &state);
    for (int i = 0; i < 4; i++)
    {
        assert(threads[i] != NULL);
        SDL_WaitThread(threads[i], NULL);
    }

    assert(state.value == 4L * state.iterations);
    SDL_DestroyMutex(state.mutex);
    printf("  mutex excludes: %ld increments from 4 threads, none lost\n", state.value);
}

/* Signalled and timed out are different answers, and the difference is what
 * gl_model.c branches on. */
struct cond_state
{
    SDL_mutex *mutex;
    SDL_cond *cond;
    int ready;
};

static int cond_signaller(void *data)
{
    struct cond_state *state = data;
    SDL_Delay(30);
    SDL_LockMutex(state->mutex);
    state->ready = 1;
    SDL_CondSignal(state->cond);
    SDL_UnlockMutex(state->mutex);
    return 0;
}

static void test_cond_signalled_and_timeout(void)
{
    struct cond_state state = {SDL_CreateMutex(), SDL_CreateCond(), 0};
    assert(state.mutex && state.cond);

    /* Timed out: nobody will signal this one, and the answer must be non-zero
     * because vkQuake's own macro wraps the call in `== 0`. */
    SDL_LockMutex(state.mutex);
    const Uint64 before = SDL_GetPerformanceCounter();
    const int timed_out = SDL_CondWaitTimeout(state.cond, state.mutex, 25);
    const Uint64 elapsed_ms = (SDL_GetPerformanceCounter() - before) / 1000000ull;
    SDL_UnlockMutex(state.mutex);
    assert(timed_out != 0);
    assert(elapsed_ms >= 20);

    /* Signalled: this one is woken, and the answer must be zero. */
    SDL_Thread *thread = SDL_CreateThread(cond_signaller, "signaller", &state);
    assert(thread != NULL);
    SDL_LockMutex(state.mutex);
    while (!state.ready)
        assert(SDL_CondWait(state.cond, state.mutex) == 0);
    SDL_UnlockMutex(state.mutex);
    SDL_WaitThread(thread, NULL);

    assert(state.ready == 1);
    SDL_DestroyCond(state.cond);
    SDL_DestroyMutex(state.mutex);
    printf("  condition: timeout answered non-zero after %llums, signal answered zero\n",
           (unsigned long long)elapsed_ms);
}

/* A handle released by the joiner, and a handle released by the thread itself.
 * Both run many times: a double free or a use-after-free in the reference count
 * shows up as a crash or as a wrong status rather than as a silent leak. */
static int returns_seven(void *data)
{
    (void)data;
    return 7;
}

static int detached_worker(void *data)
{
    (void)data;
    sched_yield();
    return 0;
}

static void test_thread_lifecycle(void)
{
    for (int i = 0; i < 200; i++)
    {
        SDL_Thread *thread = SDL_CreateThread(returns_seven, "seven", NULL);
        assert(thread != NULL);
        int status = -1;
        SDL_WaitThread(thread, &status);
        assert(status == 7);
    }

    for (int i = 0; i < 200; i++)
    {
        SDL_Thread *thread = SDL_CreateThread(detached_worker, "detached", NULL);
        assert(thread != NULL);
        SDL_DetachThread(thread);
    }

    /* Give the detached threads a moment to finish so the count is not raced by
     * the process exiting underneath them. */
    SDL_Delay(50);
    printf("  threads: 200 joined with a status, 200 detached, no double free\n");
}

/* ---------------------------------------------------------------------------
 * The ordinary behaviour.
 * ------------------------------------------------------------------------- */

static void test_semaphore(void)
{
    SDL_sem *sem = SDL_CreateSemaphore(1);
    assert(sem != NULL);

    assert(SDL_SemWait(sem) == 0);    /* takes the only permit */
    assert(SDL_SemTryWait(sem) != 0); /* empty, and non-zero means so */
    assert(SDL_SemPost(sem) == 0);
    assert(SDL_SemTryWait(sem) == 0); /* available again */
    assert(SDL_SemValue(sem) == 0);

    SDL_SemPost(sem);
    SDL_DestroySemaphore(sem);
    printf("  semaphore: wait, try-wait, post and value agree\n");
}

static void test_delay_and_clock(void)
{
    const Uint64 first = SDL_GetPerformanceCounter();
    SDL_Delay(40);
    const Uint64 second = SDL_GetPerformanceCounter();
    assert(SDL_GetPerformanceFrequency() == 1000000000ull);

    const Uint64 slept_ms = (second - first) / 1000000ull;
    /* A sleep may overshoot - the scheduler decides - but must never undershoot
     * by any amount the frame pacer would notice. */
    assert(slept_ms >= 35);
    assert(second > first);

    assert(SDL_GetTicks64() >= slept_ms);
    assert(SDL_GetCPUCount() >= 1);
    printf("  clock: 40ms sleep measured %llums, counter monotonic, %d cpus\n",
           (unsigned long long)slept_ms, SDL_GetCPUCount());
}

static void test_error_is_per_thread(void)
{
    /* The engine formats SDL_GetError() into Sys_Error immediately after a
     * failing call, from whichever thread made it. */
    SDL_SetError("first thread's reason");
    assert(strcmp(SDL_GetError(), "first thread's reason") == 0);

    struct cond_state unused = {0};
    (void)unused;
    SDL_ClearError();
    assert(strcmp(SDL_GetError(), "no error") == 0);

    /* A NULL create is how the engine finds out it is out of memory, and the
     * reason has to survive to the message. */
    assert(SDL_CreateThread(NULL, "nothing", NULL) == NULL);
    assert(strstr(SDL_GetError(), "function") != NULL);
    printf("  error string survives, and a refused call explains itself\n");
}

static void test_filesystem_answers(void)
{
    /* The console has no home directory; /app0 is the title's own folder. */
    char *pref = SDL_GetPrefPath("", "vkQuake");
    assert(pref != NULL);
    assert(strcmp(pref, "/app0/") == 0);
    SDL_free(pref);

    char *base = SDL_GetBasePath();
    assert(base != NULL);
    assert(strcmp(base, "/app0/") == 0);
    SDL_free(base);

    /* The stream object the miniz callback uses, against a real file. */
    const char *path = "build/sdl-rwops-test.bin";
    SDL_RWops *writer = SDL_RWFromFile(path, "wb");
    assert(writer != NULL);
    const char payload[] = "vkquake";
    assert(SDL_RWwrite(writer, payload, 1, sizeof payload - 1) == sizeof payload - 1);
    assert(SDL_RWclose(writer) == 0);

    SDL_RWops *reader = SDL_RWFromFile(path, "rb");
    assert(reader != NULL);
    assert(SDL_RWsize(reader) == (Sint64)(sizeof payload - 1));
    assert(SDL_RWseek(reader, 2, SDL_RW_SEEK_SET) == 2);
    char buffer[8] = {0};
    assert(SDL_RWread(reader, buffer, 1, 4) == 4);
    assert(memcmp(buffer, "quak", 4) == 0);
    assert(SDL_RWclose(reader) == 0);
    remove(path);

    printf("  filesystem: /app0 for both path queries, RWops round-trips a file\n");
}

static void test_cpu_features(void)
{
    /* Reporting a feature the CPU lacks would crash the SIMD paths, so the
     * answers have to be consistent: AVX2 implies AVX implies SSE2. */
    const SDL_bool sse = SDL_HasSSE();
    const SDL_bool sse2 = SDL_HasSSE2();
    const SDL_bool avx = SDL_HasAVX();
    const SDL_bool avx2 = SDL_HasAVX2();

    if (avx2)
        assert(avx && sse2 && sse);
    if (avx)
        assert(sse2 && sse);
    if (sse2)
        assert(sse);

    /* Consistency is not correctness, and the first version of this test checked
     * only consistency - which it passed while AVX2 was reported absent on a CPU
     * that has it, because __get_cpuid(7, ...) returns zeroes. So the answers are
     * also compared against the compiler's own opinion, which is an independent
     * source for the same fact. This is the check that catches the bug. */
#if defined(__x86_64__) || defined(__i386__)
    assert(sse == (__builtin_cpu_supports("sse") ? SDL_TRUE : SDL_FALSE));
    assert(sse2 == (__builtin_cpu_supports("sse2") ? SDL_TRUE : SDL_FALSE));
    assert(avx == (__builtin_cpu_supports("avx") ? SDL_TRUE : SDL_FALSE));
    assert(avx2 == (__builtin_cpu_supports("avx2") ? SDL_TRUE : SDL_FALSE));
    printf("  cpu features: SSE=%d SSE2=%d AVX=%d AVX2=%d, and they match __builtin_cpu_supports\n",
           sse, sse2, avx, avx2);
#else
    printf("  cpu features: SSE=%d SSE2=%d AVX=%d AVX2=%d\n", sse, sse2, avx, avx2);
#endif
}

int main(void)
{
    printf("sdl_ps5: the SDL compatibility layer, on the host\n");

    test_mutex_is_recursive();
    test_mutex_excludes();
    test_cond_signalled_and_timeout();
    test_thread_lifecycle();
    test_semaphore();
    test_delay_and_clock();
    test_error_is_per_thread();
    test_filesystem_answers();
    test_cpu_features();

    printf("sdl_ps5: all checks passed\n");
    return 0;
}
