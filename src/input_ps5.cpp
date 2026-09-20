/*
 * PS5 vkQuake - the console's gamepad.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file owns the native pad and reports its state. It used to be RetroArch's
 * joypad and input driver pair, polling for binding discovery and translating the
 * pad into RetroArch's button numbering and axis encoding; both of those are gone
 * with the frontend, and what is left is the console's own state behind the small
 * surface in input_ps5.h.
 *
 * The ABI is not derivable and is not guessed. `scePadInit`, `scePadOpen`,
 * `scePadRead` and the 120-byte sample layout below were verified on hardware by
 * ../ProsperoLight, a native PS5 title whose controls work: its src/radio_input.cpp
 * and src/moonlight_stream.cpp carry the same calls, the same button bits and the
 * same offsets, including `connected` at 0x4c and the timestamp at 0x50. The static
 * assertions are what keeps a wrong layout from compiling quietly.
 */

#include "input_ps5.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

extern "C"
{
    /* The console's pad service. Declarations rather than the SDK's headers: this
     * payload SDK ships no header for these, and ../ProsperoLight declares the same
     * shapes, which the console accepted. */
    std::int32_t scePadInit();
    std::int32_t scePadOpen(std::int32_t user_id, std::int32_t port_type, std::int32_t index,
                            const void *params);
    std::int32_t scePadRead(std::int32_t handle, void *samples, std::int32_t capacity);
    std::int32_t scePadClose(std::int32_t handle);
    std::int32_t sceUserServiceInitialize(const void *params);
    std::int32_t sceUserServiceGetInitialUser(std::int32_t *user_id);
    std::int32_t sceUserServiceTerminate();
    std::int32_t sceKernelUsleep(std::uint32_t microseconds);
}

namespace
{
/* The console's pad words are named in input_ps5.h, because the port layer reads
 * them too: the button bits are the console's own numbering and the mapping onto
 * Quake's keys happens on the far side of that header. Keeping a second copy of
 * the values here is how the two would drift. */

/* One sample, as the console writes it. */
struct PadSample
{
    std::uint32_t buttons;
    std::uint8_t left_x;
    std::uint8_t left_y;
    std::uint8_t right_x;
    std::uint8_t right_y;
    std::uint8_t left_trigger;
    std::uint8_t right_trigger;
    std::uint8_t reserved_to_connected[66];
    std::int32_t connected;
    std::uint64_t timestamp_us;
    std::uint8_t extension[16];
    std::uint8_t connected_count;
    std::uint8_t remaining[15];
};

static_assert(sizeof(PadSample) == 120, "the console's pad samples are 120 bytes");
static_assert(offsetof(PadSample, left_x) == 0x04, "the stick bytes follow the button word");
static_assert(offsetof(PadSample, connected) == 0x4c, "connection state sits at 0x4c");
static_assert(offsetof(PadSample, timestamp_us) == 0x50, "the timestamp sits at 0x50");

/* One read returns a batch of the samples taken since the last one. */
constexpr int sample_capacity = 64;
constexpr std::int32_t pad_open_attempts = 10;
constexpr std::uint32_t pad_open_retry_microseconds = 100000;
/* The stick bytes run 0..255 with 128 centred. */
constexpr int stick_centre = 128;

/* The trace is a development aid: one line goes to the title's own file, which is
 * the only output this project has that survives a run. It lives in src/trace.cpp,
 * whose signature is C++ and therefore not reachable from an `extern "C"`
 * declaration - the driver calls through this small C-linkage door instead. */
} // namespace

extern "C" void ps5_input_trace(const char *line) noexcept;

namespace
{
struct PadState
{
    std::int32_t handle = -1;
    PadSample samples[sample_capacity];
    /* How many entries of `samples` the last read actually filled. A read returns
     * only what happened since the previous one, so the rest of the buffer still
     * holds older frames and must not be reported as current. */
    std::int32_t sample_count = 0;
    std::uint32_t buttons = 0;
    bool owns_user_service = false;
    bool announced = false;
};

PadState *active_pad = nullptr;

PadState *state_of(void *data) noexcept
{
    return static_cast<PadState *>(data);
}

/* The newest of the samples the last read filled, or null when the pad is not
 * there or the shell is intercepting it. */
const PadSample *newest_sample(const PadState &state) noexcept
{
    const PadSample *newest = nullptr;
    for (std::int32_t index = 0; index < state.sample_count; ++index)
    {
        const PadSample &sample = state.samples[index];
        if (newest == nullptr || sample.timestamp_us > newest->timestamp_us)
            newest = &sample;
    }
    if (newest == nullptr || !newest->connected || (newest->buttons & ps5_pad_intercepted) != 0)
        return nullptr;
    return newest;
}

/* A stick byte as a signed 16-bit axis, about the centre the pad reports. */
std::int16_t stick_axis(std::uint8_t value) noexcept
{
    const int offset = static_cast<int>(value) - stick_centre;
    int scaled = offset * 32767 / (offset < 0 ? 128 : 127);
    if (scaled > 32767)
        scaled = 32767;
    if (scaled < -32768)
        scaled = -32768;
    return static_cast<std::int16_t>(scaled);
}

void *open_pad() noexcept
{
    auto *state = new (std::nothrow) PadState();
    if (state == nullptr)
    {
        ps5_input_trace("input: the driver state could not be allocated");
        return nullptr;
    }

    state->owns_user_service = sceUserServiceInitialize(nullptr) == 0;

    std::int32_t user_id = -1;
    if (sceUserServiceGetInitialUser(&user_id) < 0)
    {
        ps5_input_trace("input: sceUserServiceGetInitialUser found no user; no pad will be read");
        return state; /* the driver stays alive and reports nothing */
    }
    if (scePadInit() < 0)
    {
        ps5_input_trace("input: scePadInit failed; no pad will be read");
        return state;
    }
    /* The pad is not always there the first time it is asked for - a title started
     * from the shell can arrive before the pad service has published the device -
     * so the open is retried, as ../ProsperoLight retries it. */
    for (std::int32_t attempt = 0; attempt < pad_open_attempts; ++attempt)
    {
        state->handle = scePadOpen(user_id, 0, 0, nullptr);
        if (state->handle >= 0)
            break;
        (void)sceKernelUsleep(pad_open_retry_microseconds);
    }
    if (state->handle < 0)
    {
        char line[176];
        std::snprintf(line, sizeof(line),
                      "input: scePadOpen failed after %d attempts, handle=%d; no input this run",
                      static_cast<int>(pad_open_attempts), state->handle);
        ps5_input_trace(line);
        return state;
    }
    {
        char line[176];
        std::snprintf(line, sizeof(line), "input: pad opened, user=%d handle=%d",
                      static_cast<int>(user_id), state->handle);
        ps5_input_trace(line);
    }
    return state;
}

void poll_pad(void *data) noexcept
{
    PadState *state = state_of(data);
    if (state == nullptr || state->handle < 0)
        return;
    const std::int32_t count = scePadRead(state->handle, state->samples, sample_capacity);
    if (count == 0)
        return; // No new samples: preserve the last state across a second binding poll.
    if (count < 0 || count > sample_capacity)
    {
        /* The service refused the read or the pad went away. Nothing is reported
         * rather than the last state being repeated, so a button cannot stick
         * down. */
        state->sample_count = 0;
        state->buttons = 0;
        return;
    }
    state->sample_count = count;
    const PadSample *newest = newest_sample(*state);
    state->buttons = newest != nullptr ? newest->buttons : 0;

    /* Successful button transitions are routine, not diagnostics. Keep pad-open
     * failures and lifecycle logs, without a synchronous file write per press. */
}

void close_pad(void *data) noexcept
{
    PadState *state = state_of(data);
    if (state == nullptr)
        return;
    if (state->handle >= 0)
    {
        (void)scePadClose(state->handle);
        state->handle = -1;
    }
    if (state->owns_user_service)
    {
        (void)sceUserServiceTerminate();
        state->owns_user_service = false;
    }
    delete state;
}
} // namespace

/* --- the port's input surface ---------------------------------------------
 *
 * These were RetroArch's input_driver_t and input_device_driver_t, plus the
 * translation onto RetroArch's button numbering and axis encoding that went with
 * them. What replaces them is the console's own state, reported as it arrives:
 * the button mask the pad sets, and the six axes at the widths the hardware uses.
 * The mapping onto Quake's keys belongs to whoever drives the engine, and putting
 * it here would be this file deciding what a circle button means to a game it
 * does not know.
 */
extern "C"
{
    bool ps5_pad_open(void)
    {
        if (!active_pad)
            active_pad = state_of(open_pad());
        if (active_pad)
            ps5_input_trace("input: ps5 pad opened (16 buttons, 6 axes)");
        return active_pad != nullptr;
    }

    void ps5_pad_poll(void)
    {
        poll_pad(active_pad);
    }

    void ps5_pad_close(void)
    {
        close_pad(active_pad);
        active_pad = nullptr;
    }

    std::uint32_t ps5_pad_buttons(void)
    {
        return active_pad != nullptr ? active_pad->buttons : 0;
    }

    std::int16_t ps5_pad_axis(std::uint32_t index)
    {
        if (active_pad == nullptr)
            return 0;
        const PadSample *sample = newest_sample(*active_pad);
        if (sample == nullptr)
            return 0;
        switch (index)
        {
        case ps5_pad_axis_left_x:
            return stick_axis(sample->left_x);
        case ps5_pad_axis_left_y:
            return stick_axis(sample->left_y);
        case ps5_pad_axis_right_x:
            return stick_axis(sample->right_x);
        case ps5_pad_axis_right_y:
            return stick_axis(sample->right_y);
        case ps5_pad_axis_left_trigger:
            return static_cast<std::int16_t>(sample->left_trigger * 32767 / 255);
        case ps5_pad_axis_right_trigger:
            return static_cast<std::int16_t>(sample->right_trigger * 32767 / 255);
        default:
            return 0;
        }
    }
} // extern "C"
