/* Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PS5_AUDIO_H
#define PS5_AUDIO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* The callback runs under the same lock the engine takes while mixing.
     * Native output consumes 256 stereo S16 frames at 48000 Hz per call. */
    typedef void (*ps5_audio_fill)(void *context, int16_t *samples, size_t frames);
    void *ps5_audio_open(ps5_audio_fill fill, void *context);
    void ps5_audio_close(void *audio);
    void ps5_audio_lock(void *audio);
    void ps5_audio_unlock(void *audio);
    bool ps5_audio_pause(void *audio, bool paused);
#ifdef __cplusplus
}
#endif
#endif
