/* Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 * DMA cursor, stereo byte ordering and failure cleanup using real engine types. */
#include <assert.h>
#include "../platform/ps5/ps5_audio.c"
volatile dma_t *shm;
static ps5_audio_fill callback;
static void *callback_context;
static bool fail_open, locked, paused;
static unsigned closed, allocations;
void *Mem_Alloc(size_t bytes)
{
    ++allocations;
    return malloc(bytes);
}
void Mem_Free(const void *ptr)
{
    --allocations;
    free((void *)ptr);
}
void Con_Printf(const char *fmt, ...)
{
    (void)fmt;
}
void *ps5_audio_open(ps5_audio_fill fill, void *context)
{
    callback = fill;
    callback_context = context;
    return fail_open ? NULL : context;
}
void ps5_audio_close(void *device)
{
    assert(device == callback_context);
    ++closed;
}
void ps5_audio_lock(void *device)
{
    assert(device && !locked);
    locked = true;
}
void ps5_audio_unlock(void *device)
{
    assert(device && locked);
    locked = false;
}
bool ps5_audio_pause(void *device, bool pause)
{
    assert(device);
    paused = pause;
    return true;
}

int main(void)
{
    dma_t dma;
    fail_open = true;
    assert(!SNDDMA_Init(&dma) && !shm && !dma.buffer && allocations == 0);
    fail_open = false;
    assert(SNDDMA_Init(&dma));
    assert(dma.speed == 48000 && dma.channels == 2 && dma.samplebits == 16);
    int16_t *ring = (int16_t *)dma.buffer;
    for (int i = 0; i < dma.samples; ++i)
    {
        assert(ring[i] == 0);
        ring[i] = (int16_t)i;
    }
    dma.samplepos = dma.samples - 6;
    int16_t result[16];
    callback(callback_context, result, 8);
    for (unsigned i = 0; i < 16; ++i)
        assert(result[i] == (int16_t)((dma.samples - 6 + i) % dma.samples));
    SNDDMA_LockBuffer();
    assert(SNDDMA_GetDMAPos() == 10 && locked);
    SNDDMA_Submit();
    assert(!locked);
    SNDDMA_BlockSound();
    assert(paused);
    SNDDMA_UnblockSound();
    assert(!paused);
    SNDDMA_Shutdown();
    assert(!shm && allocations == 0 && closed == 1);
    SNDDMA_Shutdown();
    assert(closed == 1);
    puts("audio adapter: native format, stereo ring wrap, cursor, pause and cleanup PASS");
}
