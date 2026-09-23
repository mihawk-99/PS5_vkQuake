#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

extern "C"
{
    void *__wrap_malloc(size_t);
    void *__wrap_calloc(size_t, size_t);
    void *__wrap_realloc(void *, size_t);
    void __wrap_free(void *);
    // Host build binds these to libc, preserving genuinely foreign pointers.
    void *__real_malloc(size_t n)
    {
        return std::malloc(n);
    }
    void *__real_calloc(size_t n, size_t s)
    {
        return std::calloc(n, s);
    }
    void *__real_realloc(void *p, size_t n)
    {
        // Model the console's small private heap: growth must migrate before
        // asking its realloc for a large allocation.
        if (n >= 32 * 1024)
        {
            errno = ENOMEM;
            return nullptr;
        }
        return std::realloc(p, n);
    }
    void __real_free(void *p)
    {
        std::free(p);
    }
}
int main()
{
    constexpr size_t large = 16 * 1024 * 1024;
    auto *p = static_cast<unsigned char *>(__wrap_malloc(large));
    assert(p && uintptr_t(p) % alignof(std::max_align_t) == 0);
    std::memset(p, 0xA5, large);
    assert(!__wrap_realloc(p, SIZE_MAX));
    assert(p[0] == 0xA5 && p[large - 1] == 0xA5);
    p = static_cast<unsigned char *>(__wrap_realloc(p, large * 2));
    assert(p && p[0] == 0xA5 && p[large - 1] == 0xA5);
    p = static_cast<unsigned char *>(__wrap_realloc(p, 13));
    assert(p && p[12] == 0xA5);
    __wrap_free(p);
    p = static_cast<unsigned char *>(__wrap_calloc(2, large));
    assert(p);
    for (size_t i = 0; i < 2 * large; ++i)
        assert(p[i] == 0);
    __wrap_free(p);
    assert(!__wrap_calloc(SIZE_MAX, 2) && errno == ENOMEM);
    assert(!__wrap_malloc(SIZE_MAX) && errno == ENOMEM);
    // Native malloc/calloc buffers preserve their content when growth crosses
    // the mapped threshold, and failed growth leaves ownership/content intact.
    p = static_cast<unsigned char *>(__wrap_malloc(31));
    assert(p);
    std::memset(p, 0x6a, 31);
    p = static_cast<unsigned char *>(__wrap_realloc(p, 100));
    assert(p && p[30] == 0x6a);
    assert(!__wrap_realloc(p, SIZE_MAX) && p[30] == 0x6a);
    p = static_cast<unsigned char *>(__wrap_realloc(p, large));
    assert(p && p[0] == 0x6a && p[30] == 0x6a);
    __wrap_free(p);
    p = static_cast<unsigned char *>(__wrap_calloc(100, 10));
    assert(p);
    p = static_cast<unsigned char *>(__wrap_realloc(p, large));
    assert(p);
    for (size_t i = 0; i < 1000; ++i)
        assert(p[i] == 0);
    __wrap_free(p);
    // strdup/native library buffers must still be freed by libc.
    char *foreign = strdup("native allocation");
    foreign = static_cast<char *>(__wrap_realloc(foreign, 128));
    assert(foreign && !strcmp(foreign, "native allocation"));
    __wrap_free(foreign);
    foreign = strdup("native allocation");
    // Untracked native pointers must never be copied using a guessed size.
    assert(!__wrap_realloc(foreign, large));
    assert(!strcmp(foreign, "native allocation"));
    __wrap_free(foreign);
    __wrap_free(nullptr);
    void *empty = __wrap_calloc(0, SIZE_MAX);
    assert(empty);
    __wrap_free(empty);
    assert(!__wrap_realloc(__wrap_malloc(large), 0));
    // A released span is reused for the next allocation of the same span, and
    // calloc still returns zeroed memory when it is handed a reused one.
    constexpr size_t linear = 64 * 1024;
    auto *first = static_cast<unsigned char *>(__wrap_malloc(linear));
    assert(first);
    std::memset(first, 0x5c, linear);
    __wrap_free(first);
    auto *again = static_cast<unsigned char *>(__wrap_calloc(1, linear));
    assert(again == first);
    for (size_t i = 0; i < linear; ++i)
        assert(again[i] == 0);
    std::memset(again, 0x33, linear);
    __wrap_free(again);
    again = static_cast<unsigned char *>(__wrap_malloc(linear));
    assert(again == first && again[linear - 1] == 0x33);
    __wrap_free(again);
    // Spans above the cache's per-span limit are still returned to the system,
    // and a full cache does not keep more.
    std::vector<void *> many;
    for (int i = 0; i < 40; ++i)
        many.push_back(__wrap_malloc(linear));
    for (void *block : many)
        __wrap_free(block);
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t)
        workers.emplace_back(
            []
            {
                for (int i = 0; i < 50; ++i)
                {
                    void *buffer = __wrap_malloc(1024 * 1024);
                    assert(buffer);
                    memset(buffer, i, 1024 * 1024);
                    __wrap_free(buffer);
                }
            });
    for (auto &worker : workers)
        worker.join();
}
