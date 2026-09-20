/*
 * PS5 vkQuake - the seven libc functions the payload SDK declares but does not carry.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this file exists. The payload SDK's libc is a clean-room runtime, and it is
 * not a complete one: it declares a function in its headers that it does not
 * define, so the compiler accepts the call and the linker refuses it. Seven of
 * those are reachable from vkQuake, and this file supplies them.
 *
 * It is the same shape as src/locale_shims.c, which does this for the thirty-four
 * xlocale functions glslang and SPIRV-Cross are written against, and it exists
 * for the same reason: the alternative is patching upstream's sources to avoid
 * standard C, which would be a divergence to maintain forever to work around a
 * gap that is one file wide.
 *
 * Each one answers as truthfully as the console allows, and where the answer is
 * "there is nothing here" it says so rather than inventing something plausible:
 *
 *   backtrace        There is no unwinder. Reporting zero frames is what a
 *                    platform without one does, and vkQuake's only caller is a
 *                    debugger check that treats an empty trace as "not in one".
 *   getpwuid         There are no users on a console and no /etc/passwd to read.
 *                    NULL is what upstream's caller expects and checks for - it
 *                    prints the failure and falls back.
 *   gethostbyaddr    Reverse DNS. The console's network is for LAN play, and a
 *                    failed lookup is the ordinary case upstream handles.
 *   hstrerror        A string for an h_errno code, which this file also owns
 *                    because the resolver that would set it is not here.
 *
 * getline is the one that is not an absence. Upstream uses it to read a Steam
 * configuration file, and it has to behave, so it is implemented properly over
 * the libc's own FILE rather than stubbed.
 */

#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/* --- the unwinder ---------------------------------------------------------
 *
 * The SDK's execinfo.h declares both and defines neither. vkQuake calls them
 * from Sys_IsInDebugger's neighbourhood, where an empty trace is the answer that
 * means "no debugger", which is also the true answer here.
 */

int backtrace(void **buffer, int size)
{
    (void)buffer;
    (void)size;
    return 0;
}

char **backtrace_symbols(void *const *buffer, int size)
{
    (void)buffer;
    (void)size;
    return NULL;
}

/* --- users ----------------------------------------------------------------
 *
 * Sys_GetUserdir calls getpwuid(getuid()) and checks for NULL, printing the
 * failure and falling back to $HOME. On this console there is no user database
 * to consult, so that fallback is the path taken.
 */

struct passwd *getpwuid(uid_t uid)
{
    (void)uid;
    return NULL;
}

/* --- getline --------------------------------------------------------------
 *
 * POSIX getline, over the libc's own FILE. It grows the caller's buffer with
 * realloc, which upstream frees with free() and not Mem_Free - its own comment
 * says so - so the allocator has to be the libc's, which it is.
 */

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    if (!lineptr || !n || !stream)
    {
        errno = EINVAL;
        return -1;
    }

    if (!*lineptr || *n == 0)
    {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr)
            return -1;
    }

    size_t used = 0;
    for (;;)
    {
        const int character = fgetc(stream);
        if (character == EOF)
        {
            /* Nothing read and end of file: the caller sees -1, which is how it
             * knows the file is done. A partial line is still a line. */
            if (used == 0)
                return -1;
            break;
        }
        if (used + 2 > *n)
        {
            const size_t grown = *n * 2;
            char *larger = realloc(*lineptr, grown);
            if (!larger)
                return -1;
            *lineptr = larger;
            *n = grown;
        }
        (*lineptr)[used++] = (char)character;
        if (character == '\n')
            break;
    }

    (*lineptr)[used] = '\0';
    return (ssize_t)used;
}

/* --- the resolver's error channel -----------------------------------------
 *
 * net_sys.h takes h_errno from netdb.h, and the SDK's netdb.h defines it as
 * `(*__h_errno())` - a function returning a pointer, so that the value can be
 * per thread. This is that function. There is no resolver here to set it, so it
 * stays zero, which is the value upstream reads when a lookup succeeds.
 */

static _Thread_local int resolver_error;

int *__h_errno(void)
{
    return &resolver_error;
}

const char *hstrerror(int err)
{
    switch (err)
    {
    case 0:
        return "no resolver error";
    case HOST_NOT_FOUND:
        return "host not found";
    case TRY_AGAIN:
        return "try again";
    case NO_RECOVERY:
        return "non-recoverable resolver error";
    case NO_DATA:
        return "no address associated with name";
    default:
        return "unknown resolver error";
    }
}

/* --- reverse lookup -------------------------------------------------------
 *
 * UDP_GetNameFromAddr resolves a peer's address to a name for the scoreboard and
 * the server list. A console has no resolver, so the answer is that there is no
 * name - which is the same answer a desktop gives when the lookup fails, and the
 * caller already treats it as "show the address instead".
 */

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type)
{
    (void)addr;
    (void)len;
    (void)type;
    resolver_error = HOST_NOT_FOUND;
    return NULL;
}

/* --- the working directory and the environment ----------------------------
 *
 * These are the functions a console libc omits because a console has neither:
 * there is no process working directory to ask about and no environment to read.
 * They are here rather than absent because vkQuake calls them on its startup
 * path, and because the link does not fail when they are missing - the SDK's
 * libc_stub_weak.so supplies a placeholder for each, so the call compiles, links,
 * and then jumps through an unfilled GOT slot.
 *
 * getcwd is how this was found. The first console run died with SIGSEGV, an
 * instruction-fetch fault at 0x10002627e6, and the backtrace symbolized to
 * Sys_Init+0x31: the indirect call to getcwd three instructions in. getcwd is not
 * in the SDK's libc.a, it is in libc_stub_weak.so, and the console's libc.prx does
 * not provide it - so the GOT slot held nothing and the call went nowhere.
 *
 * The answer getcwd gives is the true one for a title: /app0 is the folder the
 * console mounts this application at, and it is where the game data, the config
 * and the trace all live. Upstream uses it to set host_parms->basedir, so this is
 * what makes `id1/pak0.pak` resolve under /app0 without patching anything.
 *
 * The previous project on this console hit the same absence and answered it by
 * patching fill_pathname_application_path to a constant rather than by shimming
 * the call. This does the shim, because vkQuake is not ours to patch and because
 * the function has a correct answer here anyway.
 */

#define PS5_TITLE_FOLDER "/app0"

char *getcwd(char *buffer, size_t size)
{
    const size_t needed = sizeof(PS5_TITLE_FOLDER); /* includes the NUL */
    if (!buffer)
    {
        /* POSIX allows a NULL buffer with size 0 and lets the implementation
         * allocate. Nothing here calls it that way, but returning NULL for it is
         * better than writing through a null pointer. */
        errno = EINVAL;
        return NULL;
    }
    if (size < needed)
    {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buffer, PS5_TITLE_FOLDER, needed);
    return buffer;
}

/* The environment, in process. A console has no environment to inherit, so this
 * is the whole of it: setenv and putenv add, getenv reads, unsetenv removes. That
 * is enough for what a title does with one - vkQuake sets SDL_VIDEO_CENTERED on
 * its way into VID_Init, and reads HOME only under a build option this port does
 * not enable. */
#define PS5_ENV_MAX 32
#define PS5_ENV_VALUE_MAX 256

static char environment[PS5_ENV_MAX][PS5_ENV_VALUE_MAX];
static int environment_count;

static int environment_find(const char *name)
{
    const size_t length = strlen(name);
    for (int index = 0; index < environment_count; ++index)
    {
        if (strncmp(environment[index], name, length) == 0 && environment[index][length] == '=')
            return index;
    }
    return -1;
}

char *getenv(const char *name)
{
    if (!name)
        return NULL;
    const int index = environment_find(name);
    return index < 0 ? NULL : environment[index] + strlen(name) + 1;
}

int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !value || !*name || strchr(name, '='))
    {
        errno = EINVAL;
        return -1;
    }
    const int existing = environment_find(name);
    if (existing >= 0 && !overwrite)
        return 0;

    char entry[PS5_ENV_VALUE_MAX];
    const int written = snprintf(entry, sizeof entry, "%s=%s", name, value);
    if (written < 0 || (size_t)written >= sizeof entry)
    {
        errno = ENOMEM;
        return -1;
    }

    if (existing >= 0)
    {
        memcpy(environment[existing], entry, (size_t)written + 1);
        return 0;
    }
    if (environment_count >= PS5_ENV_MAX)
    {
        errno = ENOMEM;
        return -1;
    }
    memcpy(environment[environment_count], entry, (size_t)written + 1);
    ++environment_count;
    return 0;
}

int unsetenv(const char *name)
{
    if (!name || !*name || strchr(name, '='))
    {
        errno = EINVAL;
        return -1;
    }
    const int index = environment_find(name);
    if (index < 0)
        return 0;
    /* The last entry takes the hole, so the array stays dense and the order of
     * the others is not disturbed. */
    --environment_count;
    if (index != environment_count)
        memcpy(environment[index], environment[environment_count], PS5_ENV_VALUE_MAX);
    return 0;
}

/* putenv takes the string itself rather than copying it, which is the one way it
 * differs from setenv. Copying is what this does, so the caller is free to reuse
 * its buffer - which upstream is, because it passes a string literal. */
int putenv(char *string)
{
    if (!string)
    {
        errno = EINVAL;
        return -1;
    }
    const char *equals = strchr(string, '=');
    if (!equals)
        return unsetenv(string);

    char name[PS5_ENV_VALUE_MAX];
    const size_t length = (size_t)(equals - string);
    if (length == 0 || length >= sizeof name)
    {
        errno = EINVAL;
        return -1;
    }
    memcpy(name, string, length);
    name[length] = '\0';
    return setenv(name, equals + 1, 1);
}

/* --- the process and the host ---------------------------------------------
 *
 * getprogname is called by upstream's error and log paths to name the program,
 * and every title's process on this console is eboot.bin. gethostname is called
 * once while bringing the network up; the console reports the name the system
 * gave it, which this port does not read, so the console's own product name is
 * the answer rather than an invented hostname.
 */

const char *getprogname(void)
{
    return "eboot.bin";
}

int gethostname(char *name, size_t length)
{
    static const char host[] = "ps5";
    if (!name || length < sizeof host)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(name, host, sizeof host);
    return 0;
}

/* --- forward lookup, which is the one that stopped the title ---
 *
 * `gethostbyname` is the eighth function of this class and the one that cost the most
 * to find. The port shimmed seven of its neighbours - `getcwd`, `getenv`, `putenv`,
 * `setenv`, `unsetenv`, `getline`, `getpwuid`, `backtrace`, `gethostbyaddr`,
 * `gethostname` - and missed this one, and `UDP4_Init` calls it through a GOT slot
 * that nothing fills:
 *
 *   Datagram_Init -> UDP4_Init -> call *GOT(gethostbyname) -> 0
 *
 * The console reported that as `Datagram_Init +0xa1`, because a null call pushes no
 * frame and the frame-pointer walk therefore found UDP4_Init's return address - the
 * call site in its caller - as the innermost frame. Four rounds went into proving the
 * landriver table correct, which it always was.
 *
 * Returning NULL is what upstream expects and handles: `UDP4_Init` prints
 * "gethostbyname failed" with the resolver's error and carries on with the loopback
 * address, which is what a console wants anyway. The h_errno this sets is the same
 * one `gethostbyaddr` sets, for the same reason: there is no resolver here, and a
 * failed lookup is a state the engine already knows how to survive.
 */
struct hostent *gethostbyname(const char *name)
{
    (void)name;
    resolver_error = HOST_NOT_FOUND;
    return NULL;
}
