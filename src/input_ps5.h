/*
 * PS5 vkQuake - the console's pad, as the port layer consumes it.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What this is. src/input_ps5.cpp reads the DualSense through scePadRead and
 * reports the newest sample of each batch. This header is the surface it offers,
 * and it exists because that surface used to be RetroArch's: the file implemented
 * an input_driver_t and an input_device_driver_t, and translated the pad into
 * RetroArch's own button numbering and axis encoding. None of that survives the
 * frontend, and what is left is the console's own numbering, which is what the
 * port maps onto Quake's keys.
 *
 * The button bits are the console's, not a port's invention. They are the masks
 * scePadRead sets in the sample's `buttons` word, and they are named after the
 * labels printed on the shell. A caller that wants Quake's K_* keys maps these;
 * a caller that wants something else maps them differently, which is the point of
 * exposing the console's own numbering rather than a translation of it.
 *
 * The axes keep the encoding the pad reports: sticks are unsigned bytes about 128
 * and arrive here scaled to the signed 16-bit range, and the triggers are unsigned
 * bytes rising from 0. That asymmetry is the hardware's, and hiding it would mean
 * inventing a convention that the next reader has to discover anyway.
 */

#ifndef PS5_INPUT_H
#define PS5_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Held buttons, as the console reports them. `intercepted` is set by the shell
     * when it has taken the pad for itself - the home screen, a system dialog - and a
     * sample carrying it is not input this title may act on. */
    enum
    {
        ps5_pad_l3 = 0x000002u,
        ps5_pad_r3 = 0x000004u,
        ps5_pad_options = 0x000008u,
        ps5_pad_up = 0x000010u,
        ps5_pad_right = 0x000020u,
        ps5_pad_down = 0x000040u,
        ps5_pad_left = 0x000080u,
        ps5_pad_l1 = 0x000400u,
        ps5_pad_r1 = 0x000800u,
        ps5_pad_triangle = 0x001000u,
        ps5_pad_circle = 0x002000u,
        ps5_pad_cross = 0x004000u,
        ps5_pad_square = 0x008000u,
        ps5_pad_touch_pad = 0x100000u,
        ps5_pad_intercepted = 0x80000000u
    };

    /* Axis indices for ps5_pad_axis. The first four are centred sticks; the last two
     * rise from zero, because that is what a trigger does. */
    enum
    {
        ps5_pad_axis_left_x = 0,
        ps5_pad_axis_left_y = 1,
        ps5_pad_axis_right_x = 2,
        ps5_pad_axis_right_y = 3,
        ps5_pad_axis_left_trigger = 4,
        ps5_pad_axis_right_trigger = 5,
        ps5_pad_axis_count = 6
    };

    /* Opens the pad. Returns false when there is none - a title started from the
     * shell can arrive before the pad service publishes the device, and the open is
     * retried for a second before giving up. A false return is not fatal: the title
     * runs without input and every read below reports nothing held. */
    bool ps5_pad_open(void);

    /* Reads whatever the pad has produced since the last call. Cheap and safe to call
     * every frame; a read that reports nothing new leaves the previous state alone
     * rather than clearing it, so a button cannot flicker between polls. */
    void ps5_pad_poll(void);

    /* Releases the pad and the user service the open took. */
    void ps5_pad_close(void);

    /* The buttons held as of the last poll, as a mask of the ps5_pad_* bits. Zero
     * when there is no pad, when it disconnected, or when the shell has intercepted
     * it. */
    uint32_t ps5_pad_buttons(void);

    /* One axis as of the last poll: -32768..32767 for the four sticks, 0..32767 for
     * the two triggers, and 0 for an index that is not one of the six. */
    int16_t ps5_pad_axis(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* PS5_INPUT_H */
