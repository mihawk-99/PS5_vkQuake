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
