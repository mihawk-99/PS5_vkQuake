/*
 * PS5 vkQuake - a runtime probe for the engine's static pointer tables.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this is here. The third console run died calling a null function pointer:
 *
 *   Datagram_Init +0xa1   call *0x10(%r12,%r13,1)   r12 = net_landrivers
 *   NET_Init +0x207
 *   Host_Init +0x94
 *
 * `net_landrivers` is a file-scope array in net_bsd.c whose entries are function
 * pointers, so the linker emits an R_X86_64_RELATIVE relocation for each one and
 * the contents are zero in the image until the loader applies them. The
 * relocation for net_landrivers[0].Init is in the ELF - `objdump -R` shows
 * `16a2100 R_X86_64_RELATIVE *ABS*+0x726950`, and 0x726950 is UDP4_Init - and the
 * module writer carries every relocation through to the FSELF unfiltered. Yet the
 * call went to zero.
 *
 * What makes that worth a probe rather than a conclusion: the table immediately
 * before it, `net_drivers`, has the same shape, is 0x100 bytes earlier in the same
 * section, and demonstrably worked - the backtrace shows Datagram_Init being
 * called through it. So "the loader does not apply .data relocations" is not true,
 * and neither is "the writer drops them". Something separates two adjacent tables.
 *
 * This writes the values it can see, once, before main, if /app0/probe.txt is
 * present. The file is the switch because a console run has no arguments and no
 * other way to ask for a diagnostic; deleting it turns the probe off. Values that
 * are correct here and zero later mean something zeroed them; values that are zero
 * here mean the loader did not apply the relocation, and the writer or the
 * converter is where to look.
 *
 * It reads the arrays through their symbols with a struct laid out by hand, which
 * is what makes it a probe rather than part of the port: it must not depend on the
 * engine's headers, because the question is whether the engine's own view of them
 * is what it should be.
 *
 * Why it lives in src/ and not in platform/ps5/. It was written there first and
 * did not run: platform/ps5/*.c is compiled into the engine archive, and the linker
 * only extracts an archive member that resolves an undefined symbol. A file whose
 * whole content is a constructor resolves nothing, so the member was never pulled
 * in and the probe silently did not exist - the archive held ps5_probe.o and the
 * linked image had no trace of it. src/ is compiled to objects and linked
 * directly, so a constructor there is always present. The same applies to
 * src/build_identity.cpp, which is why that one worked.
 */

#include <stdio.h>

/* The linkage only; the layout is read as raw pointers at the offsets the
 * disassembly showed, so nothing here has to match net_sys.h. */
extern char net_drivers[] __attribute__((weak));
extern char net_landrivers[] __attribute__((weak));
extern const int net_numdrivers __attribute__((weak));
extern const int net_numlandrivers __attribute__((weak));

extern void ps5_trace(const char *line);

/* Offsets into the two tables, from the code that walks them.
 *
 * net_driver_t is walked with a stride of 0x80 and its Init read at +0x10;
 * net_landriver_t with a stride of 0xb0 and the same +0x10. Both come from the
 * disassembly of NET_Init and Datagram_Init rather than from the headers, because
 * the headers are exactly what is in question. */
#define DRIVER_STRIDE 0x80
#define LANDRIVER_STRIDE 0xb0
#define INIT_OFFSET 0x10

static void probe(const char *name, char *table, int count, int stride)
{
    char line[192];
    if (table == NULL)
    {
        snprintf(line, sizeof line, "probe: %s symbol is absent", name);
        ps5_trace(line);
        return;
    }
    snprintf(line, sizeof line, "probe: %s at %p, count %d", name, (void *)table, count);
    ps5_trace(line);
    for (int index = 0; index < count && index < 4; ++index)
    {
        void *init = *(void **)(table + index * stride + INIT_OFFSET);
        snprintf(line, sizeof line, "probe:   [%d].Init = %p", index, init);
        ps5_trace(line);
    }
}

/* What the table held before main, kept so a later change can be named and undone.
 *
 * The two readings that would not reconcile - the probe reading b26bb0 at
 * [net_landrivers+0x10] and the call reading zero at the same address in the same
 * run - were checked to the instruction, and both are right. So the memory does
 * change in between and the only question left is when. Recording the value this
 * early is what makes that answerable: if it is wrong later, the difference says
 * so at the first allocation afterwards, which is inside the step that did it.
 */
static void *saved_init[4];
static int saved_count;

__attribute__((constructor)) static void ps5_probe_tables(void)
{
    FILE *control = fopen("/app0/probe.txt", "rb");
    if (control == NULL)
        return;
    fclose(control);

    for (int index = 0; index < net_numlandrivers && index < 2; ++index)
        saved_init[saved_count++] =
            *(void **)(net_landrivers + index * LANDRIVER_STRIDE + INIT_OFFSET);

    ps5_trace("probe: static pointer tables, read before main");
    probe("net_drivers", net_drivers, net_numdrivers, DRIVER_STRIDE);
    probe("net_landrivers", net_landrivers, net_numlandrivers, LANDRIVER_STRIDE);
    ps5_trace("probe: end");
}

/* --- watching the value, because it changes ---
 *
 * The probe above answers "is it right before main" and the answer was yes. The
 * crash answers "is it right at NET_Init" and the answer was no, at the same
 * address, in the same run. So it is written between the two and the only question
 * left is when.
 *
 * There is no hook in Host_Init that this project owns - upstream's host.c is not
 * ours to edit - but the engine calls SDL throughout that sequence, and SDL is
 * this project's shim. platform/ps5/sdl_ps5.c calls this on every SDL_CreateMutex,
 * which happens in Sys_FileInit, Con_Init, Key_Init and elsewhere, so the trace
 * gets a handful of samples between main and the crash. The last sample before the
 * fault names the step that did it, near enough to bisect by hand if it is not
 * obvious.
 *
 * Off unless /app0/probe.txt exists, like the probe itself.
 */

void ps5_probe_watch(void)
{
    static int enabled = -1;
    static void *last = NULL;
    static int reported = 0;
    if (enabled < 0)
    {
        FILE *control = fopen("/app0/probe.txt", "rb");
        enabled = control != NULL;
        if (control)
            fclose(control);
    }
    if (!enabled || net_landrivers == NULL)
        return;

    void *init = *(void **)(net_landrivers + INIT_OFFSET);
    /* Only on change, and only once for the first value. The allocator calls this
     * on every allocation, which through Host_Init is thousands of times; logging
     * each one would bury the one line that matters and fill the console's folder
     * with a megabyte of "still correct". */
    if (reported && init == last)
        return;

    char line[160];
    if (reported && saved_count > 0 && init != saved_init[0])
    {
        /* It changed, which is the answer the whole probe was built to get. Say
         * what it was and what it is, and put the correct value back: nothing in
         * the engine writes this field - Datagram_Init writes `initialized` at +8
         * and `controlSock` at +0xc and never Init - so a change here is damage,
         * and restoring it is what a caller would have to do anyway. */
        snprintf(line, sizeof line, "probe: net_landrivers[0].Init changed %p -> %p; restored",
                 last, init);
        *(void **)(net_landrivers + INIT_OFFSET) = saved_init[0];
        init = saved_init[0];
    }
    else
    {
        snprintf(line, sizeof line, "probe: watch net_landrivers[0].Init = %p", init);
    }
    last = init;
    reported = 1;
    ps5_trace(line);
}
