/*
 * PS5 vkQuake - the engine's sound interface, on a console.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What this is, and what it is not. vkQuake's sound mixer calls seven SNDDMA_*
 * functions; upstream implements them in snd_sdl.c, which opens an SDL audio
 * device and lets SDL pull from the engine's ring. This file implements the
 * interface so the title links and boots, and SNDDMA_Init reports failure - which
 * the engine handles by running silent rather than by refusing to start:
 * S_Startup sets sound_started to false and everything downstream checks it.
 *
 * The real half already exists, as with input. src/audio_ps5.cpp opens the
 * console's AudioOut, allocates the ring, and runs the worker thread that blocks
 * in sceAudioOutOutput; it is tested on the host against a clocked mock. What is
 * missing is the adapter between that backend and the engine's expectations - a
 * `dma_t` whose `buffer` the mixer paints into, `samplepos` advanced as the
 * device consumes, and the lock/submit pair wrapped around the mix. That adapter
 * is M5, and it belongs here.
 *
 * Returning false rather than pretending is deliberate. A backend that accepted
 * the engine's samples and discarded them would make the mixer's pacing depend on
 * a device that never drains, which shows up as a stalled frame rather than as
 * silence - a much harder thing to read from a console log than no sound.
 */

#include "quakedef.h"

qboolean SNDDMA_Init(dma_t *dma)
{
    /* No audio device is opened yet; see the file header. The engine's own
     * message for this is the one to keep: it says sound is off, which is true. */
    (void)dma;
    return false;
}

int SNDDMA_GetDMAPos(void)
{
    return 0;
}

void SNDDMA_Shutdown(void)
{
}

void SNDDMA_LockBuffer(void)
{
}

void SNDDMA_Submit(void)
{
}

void SNDDMA_BlockSound(void)
{
}

void SNDDMA_UnblockSound(void)
{
}
