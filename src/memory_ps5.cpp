/* Large application buffers must not exhaust SceLibcInternal's private heap.
 * Wrap only title/core references; native library allocations stay libc-owned.
 * A locked list identifies mappings without reading before foreign pointers. */
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <sys/mman.h>
#include "memory_diagnostics.hpp"

extern "C"
{
    void *__real_malloc(size_t);
    void *__real_calloc(size_t, size_t);
    void *__real_realloc(void *, size_t);
    void __real_free(void *);
    void *__wrap_malloc(size_t);
    void __wrap_free(void *);
}

namespace
{
/* Below this, an allocation goes to SceLibcInternal's own heap; at or above it, to
 * mmap. It was 1 MiB, and a console run measured why that is too high: the run
 * reached the first pipeline compile and died inside it, and the allocation report
 * (build identity 5cdf664e, evidence/m2-internal-heap/) shows the internal heap at
 * 13.7 MB in 1780 allocations when a 64 KiB malloc failed — the engine's own
 * start-up filling it before the compiler asked for anything (NET_Init 6.5 MB,
 * VID_Init 3.7 MB, the driver's objects and command buffers 1.2 MB), and the
 * compiler then having no room. The console's own sentence confirms it:
 * "[ScePthread/System] Internal Memory is running out."
 *
 * 32 KiB moves that traffic out while leaving small allocations where they are
 * cheap: a mapping is page-rounded, so routing an 8-byte request through it would
 * cost 16 KiB. What stays native is the engine's small churn and
 * the compiler's own small allocations, against a heap that is no longer full. */
constexpr size_t threshold = 32 * 1024;
constexpr size_t page = 0x4000;
struct alignas(std::max_align_t) Mapping
{
    Mapping *next;
    size_t requested;
    size_t span;
};
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
Mapping *mappings = nullptr;

/* Recently released spans, kept mapped for the next allocation of the same span.
 * A console run measured why: the Vulkan driver's secondary command buffers each
 * free and recreate Mesa's 64 KiB linear allocator on every vkBeginCommandBuffer,
 * 26 of them a frame, and with every such block an munmap and an mmap that reset
 * cost ~95 us a buffer (driver profile2 reset_common_ms, 2.45 ms a frame). The
 * cache is bounded in count, per-span size and total bytes, so what it holds back
 * from the system stays small next to the traffic it saves. */
constexpr size_t cache_slots = 32;
constexpr size_t cache_span_limit = 1024 * 1024;
constexpr size_t cache_bytes_limit = 8 * 1024 * 1024;
Mapping *cached[cache_slots];
size_t cached_count = 0, cached_bytes = 0;
/* System calls the allocator makes, for the hitch report (ps5_window.c). */
std::atomic<unsigned long long> maps{0}, unmaps{0};

Mapping **find(void *pointer)
{
    Mapping **entry = &mappings;
    while (*entry && static_cast<void *>(*entry + 1) != pointer)
        entry = &(*entry)->next;
    return entry;
}

// A fresh mapping is zero-filled and a reused one is not, so zero says whether
// the caller (calloc) needs the bytes cleared.
void *allocate(size_t size, bool zero = false)
{
    if (size < threshold)
        return __real_malloc(size ? size : 1);
    if (size > SIZE_MAX - sizeof(Mapping) - (page - 1))
    {
        errno = ENOMEM;
        return nullptr;
    }
    const size_t span = (sizeof(Mapping) + size + page - 1) & ~(page - 1);
    Mapping *entry = nullptr;
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < cached_count; ++i)
        if (cached[i]->span == span)
        {
            entry = cached[i];
            cached[i] = cached[--cached_count];
            cached_bytes -= span;
            break;
        }
    pthread_mutex_unlock(&lock);
    if (entry)
    {
        if (zero)
            std::memset(entry + 1, 0, size);
    }
    else
    {
        void *memory = mmap(nullptr, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        maps.fetch_add(1, std::memory_order_relaxed);
        if (memory == MAP_FAILED)
            return nullptr;
        entry = static_cast<Mapping *>(memory);
    }
    entry->requested = size;
    entry->span = span;
    pthread_mutex_lock(&lock);
    entry->next = mappings;
    mappings = entry;
    pthread_mutex_unlock(&lock);
    return entry + 1;
}

void release(void *pointer)
{
    if (!pointer)
        return;
    pthread_mutex_lock(&lock);
    Mapping **slot = find(pointer);
    Mapping *entry = *slot;
    bool kept = false;
    if (entry)
    {
        *slot = entry->next;
        if (entry->span <= cache_span_limit && cached_count < cache_slots &&
            cached_bytes + entry->span <= cache_bytes_limit)
        {
            cached[cached_count++] = entry;
            cached_bytes += entry->span;
            kept = true;
        }
    }
    pthread_mutex_unlock(&lock);
    if (kept)
        return;
    if (entry)
    {
        unmaps.fetch_add(1, std::memory_order_relaxed);
        munmap(entry, entry->span);
    }
    else
        __real_free(pointer);
}

void *resize(void *pointer, size_t size, const ps5::memory::Record &owned)
{
    if (!pointer)
        return allocate(size);
    if (!size)
    {
        release(pointer);
        return nullptr;
    }
    pthread_mutex_lock(&lock);
    Mapping *entry = *find(pointer);
    const size_t old_size = entry ? entry->requested : 0;
    pthread_mutex_unlock(&lock);
    // Grow our native buffers out of the private heap using the requested size
    // we recorded. Foreign/untracked pointers keep their native allocator.
    const bool migrate = !entry && owned.pointer == pointer &&
                         owned.route == ps5::memory::Route::Native && size >= threshold;
    if (!entry && !migrate)
        return __real_realloc(pointer, size);
    void *replacement = allocate(size);
    if (!replacement)
        return nullptr;
    const size_t copy_size = entry ? old_size : owned.bytes;
    std::memcpy(replacement, pointer, copy_size < size ? copy_size : size);
    release(pointer);
    return replacement;
}

} // namespace

/* The allocator's mmap and munmap calls so far. */
extern "C" void ps5_memory_syscalls(unsigned long long *mapped, unsigned long long *unmapped)
{
    *mapped = maps.load(std::memory_order_relaxed);
    *unmapped = unmaps.load(std::memory_order_relaxed);
}

/* The probe's second clock: see src/probe.c. PR_Init and Mod_Init run between the
 * last mutex and the crash and create none, but they allocate constantly, so this
 * is what puts a sample inside them. The probe logs only when the value changes.
 *
 * Weak, and checked, because the host test compiles this file on its own and does
 * not compile the probe: a strong reference would make the allocator untestable in
 * exchange for a diagnostic. A missing probe means no probe, not a link error. */
extern "C" void ps5_probe_watch(void) __attribute__((weak));

extern "C" void *__wrap_malloc(size_t size)
{
    if (ps5_probe_watch != nullptr)
        ps5_probe_watch();
    const auto caller = uintptr_t(__builtin_return_address(0));
    void *p = allocate(size);
    if (p)
        ps5::memory::add(
            {p, size, caller,
             size < threshold ? ps5::memory::Route::Native : ps5::memory::Route::Mapped});
    else
        ps5::memory::failure("malloc", size, 0, caller, errno);
    return p;
}
extern "C" void __wrap_free(void *pointer)
{
    ps5::memory::take(pointer);
    release(pointer);
}
extern "C" void *__wrap_calloc(size_t count, size_t size)
{
    const auto caller = uintptr_t(__builtin_return_address(0));
    if (size && count > SIZE_MAX / size)
    {
        errno = ENOMEM;
        ps5::memory::failure("calloc-overflow", size, count, caller, errno);
        return nullptr;
    }
    const size_t bytes = count * size;
    void *p = bytes < threshold ? (bytes ? __real_calloc(count, size) : __real_calloc(1, 1))
                                : allocate(bytes, true);
    if (p)
        ps5::memory::add(
            {p, bytes, caller,
             bytes < threshold ? ps5::memory::Route::Native : ps5::memory::Route::Mapped});
    else
        ps5::memory::failure("calloc", bytes, 0, caller, errno);
    return p;
}
extern "C" void *__wrap_realloc(void *pointer, size_t size)
{
    const auto caller = uintptr_t(__builtin_return_address(0));
    const auto old = ps5::memory::take(pointer, true);
    void *p = resize(pointer, size, old);
    if (p)
    {
        // The resulting route may change when a known buffer crosses the threshold.
        pthread_mutex_lock(&lock);
        const bool mapped = *find(p) != nullptr;
        pthread_mutex_unlock(&lock);
        ps5::memory::add(
            {p, size, caller, mapped ? ps5::memory::Route::Mapped : ps5::memory::Route::Native});
    }
    else if (size)
    {
        ps5::memory::add(old); // Failed realloc retains ownership and contents.
        ps5::memory::failure("realloc", size, 0, caller, errno);
    }
    return p;
}
#ifdef PS5_MEMORY_DIAGNOSTICS
extern "C" int __real_posix_memalign(void **, size_t, size_t);
extern "C" int __wrap_posix_memalign(void **out, size_t alignment, size_t size)
{
    const auto caller = uintptr_t(__builtin_return_address(0));
    const int saved = errno;
    const int result = __real_posix_memalign(out, alignment, size);
    if (!result && *out)
        ps5::memory::add({*out, size, caller, ps5::memory::Route::Aligned});
    else if (result)
        ps5::memory::failure("posix_memalign", size, alignment, caller, result);
    errno = saved; // POSIX reports its error through the return value.
    return result;
}
#endif
