/* PS5 vkQuake - feed the engine's DMA ring to native AudioOut.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "quakedef.h"
#include "../../src/audio_ps5.h"

static void *audio;

static void paint_audio(void *context, int16_t *out, size_t frames)
{
    dma_t *dma = context;
    size_t samples = frames * 2;
    const int16_t *ring = (const int16_t *)dma->buffer;
    while (samples)
    {
        const size_t count = q_min(samples, (size_t)(dma->samples - dma->samplepos));
        memcpy(out, ring + dma->samplepos, count * sizeof *out);
        dma->samplepos = (dma->samplepos + count) % dma->samples;
        out += count;
        samples -= count;
    }
}

qboolean SNDDMA_Init(dma_t *dma)
{
    memset(dma, 0, sizeof *dma);
    dma->samplebits = 16;
    dma->speed = 48000;
    dma->channels = 2;
    dma->samples = 32768;
    dma->submission_chunk = 1;
    dma->buffer = Mem_Alloc(dma->samples * sizeof(int16_t));
    if (!dma->buffer)
        return false;
    memset(dma->buffer, 0, dma->samples * sizeof(int16_t));
    shm = dma;
    audio = ps5_audio_open(paint_audio, dma);
    if (!audio)
    {
        Mem_Free(dma->buffer);
        dma->buffer = NULL;
        shm = NULL;
        Con_Printf("PS5 audio: AudioOut unavailable\n");
        return false;
    }
    Con_Printf("PS5 audio: engine mixer ready, 48000 Hz stereo S16\n");
    return true;
}

int SNDDMA_GetDMAPos(void)
{
    // The engine holds SNDDMA_LockBuffer while reading this cursor.
    return shm ? shm->samplepos : 0;
}

void SNDDMA_Shutdown(void)
{
    if (audio)
        ps5_audio_close(audio);
    audio = NULL;
    if (shm)
    {
        Mem_Free(shm->buffer);
        shm->buffer = NULL;
        shm = NULL;
    }
}

void SNDDMA_LockBuffer(void)
{
    if (audio)
        ps5_audio_lock(audio);
}

void SNDDMA_Submit(void)
{
    if (audio)
        ps5_audio_unlock(audio);
}

void SNDDMA_BlockSound(void)
{
    if (audio && !ps5_audio_pause(audio, true))
        Con_Printf("PS5 audio: pause failed\n");
}

void SNDDMA_UnblockSound(void)
{
    if (audio && !ps5_audio_pause(audio, false))
        Con_Printf("PS5 audio: resume failed\n");
}
