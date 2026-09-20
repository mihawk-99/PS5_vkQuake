/*
 * PS5 vkQuake - the engine's input interface, on a console.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What this is, and what it is not. vkQuake's engine calls fourteen IN_*
 * functions; upstream implements them in in_sdl.c and in_sdl2.c, which are the
 * SDL event loop and the SDL gamepad. This file implements the interface so the
 * title links and boots, and every function below reports that nothing happened.
 * It is a placeholder with a deadline: M4 fills it in.
 *
 * Saying so plainly matters, because the interesting half already exists and it
 * would be easy to mistake this for it. src/input_ps5.cpp reads the DualSense -
 * it opens the pad, batches samples through scePadRead, and reports the held
 * buttons and six axes behind the small C surface in src/input_ps5.h. What is
 * missing is the mapping: which pad button is which Quake key, how the sticks
 * become movement and view angles, and how the console's key_dest decides whether
 * a press goes to the game or the menu. That mapping is the work M4 does, and it
 * belongs here rather than in the driver.
 *
 * Until then the honest behaviour is silence. A stub that invented a keystroke
 * would be worse than one that does nothing: a title that moves on its own is
 * harder to diagnose than one that does not move at all.
 */

#include "quakedef.h"

void IN_Init(void)
{
    /* Nothing to register: no pad is opened until there is something to do with
     * what it reports. */
}

void IN_Shutdown(void)
{
}

/* Called once a frame from Sys_SendKeyEvents. This is where the pad will be
 * polled and its transitions turned into Key_Event calls. */
void IN_SendKeyEvents(void)
{
}

/* Called once a frame from Host_Frame, after the key events. Where held buttons
 * that repeat - the pad's own key-repeat handling - will live. */
void IN_Commands(void)
{
}

/* Called from IN_Move, which the engine calls once a frame with the command being
 * built. Sticks and triggers become forward/side/up movement and view angles
 * here. */
void IN_Move(usercmd_t *cmd)
{
    (void)cmd;
}

void IN_UpdateInputMode(void)
{
}

void IN_ClearStates(void)
{
}

void IN_Activate(void)
{
}

void IN_Deactivate(qboolean free_cursor)
{
    (void)free_cursor;
}

void IN_DeactivateForConsole(void)
{
}

void IN_HideCursor(void)
{
}

/* The mouse does not exist on this console. The engine asks for its position in
 * the console overlay and the menu; answering the origin is the same answer a
 * desktop gives with no mouse attached, which is a state the engine handles. */
void IN_GetMousePos(int *outx, int *outy)
{
    if (outx)
        *outx = 0;
    if (outy)
        *outy = 0;
}

void IN_MouseMotion(float dx, float dy)
{
    (void)dx;
    (void)dy;
}

void IN_ScaleMouseCoords(float x, float y, int *outx, int *outy)
{
    (void)x;
    (void)y;
    if (outx)
        *outx = 0;
    if (outy)
        *outy = 0;
}
