/*
 * PS5 vkQuake - DualSense mapping onto the engine's key and movement interfaces.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "quakedef.h"
#include "../../src/input_ps5.h"
#include <math.h>

static cvar_t joy_enable = {"joy_enable", "1", CVAR_ARCHIVE_GAME};
static cvar_t joy_deadzone_move = {"joy_deadzone_move", "0.175", CVAR_ARCHIVE_GAME};
static cvar_t joy_deadzone_look = {"joy_deadzone_look", "0.175", CVAR_ARCHIVE_GAME};
static cvar_t joy_deadzone_trigger = {"joy_deadzone_trigger", "0.2", CVAR_ARCHIVE_GAME};
static cvar_t joy_sensitivity_yaw = {"joy_sensitivity_yaw", "240", CVAR_ARCHIVE_GAME};
static cvar_t joy_sensitivity_pitch = {"joy_sensitivity_pitch", "130", CVAR_ARCHIVE_GAME};
static cvar_t joy_invert = {"joy_invert", "0", CVAR_ARCHIVE_GAME};
static const struct
{
    uint32_t button;
    int key;
} pad_keys[] = {
    {ps5_pad_cross, K_ABUTTON},
    {ps5_pad_circle, K_BBUTTON},
    {ps5_pad_square, K_XBUTTON},
    {ps5_pad_triangle, K_YBUTTON},
    {ps5_pad_l1, K_LSHOULDER},
    {ps5_pad_r1, K_RSHOULDER},
    {ps5_pad_l3, K_LTHUMB},
    {ps5_pad_r3, K_RTHUMB},
    {ps5_pad_options, K_ESCAPE},
    {ps5_pad_touch_pad, '`'},
    {ps5_pad_up, K_UPARROW},
    {ps5_pad_down, K_DOWNARROW},
    {ps5_pad_left, K_LEFTARROW},
    {ps5_pad_right, K_RIGHTARROW},
    {0, K_LTRIGGER},
    {0, K_RTRIGGER},
};
static qboolean held[sizeof pad_keys / sizeof pad_keys[0]];
static double repeat_at[sizeof pad_keys / sizeof pad_keys[0]];

static void pad_event(unsigned at, qboolean down)
{
    const double now = Sys_DoubleTime();
    if (down != held[at])
    {
        // Commit state before Key_Event: menu confirmation can poll reentrantly.
        held[at] = down;
        repeat_at[at] = now + 0.5;
        Key_Event(pad_keys[at].key, down);
    }
    else if (down && key_dest != key_game && now >= repeat_at[at])
    {
        repeat_at[at] = now + 0.1;
        Key_Event(pad_keys[at].key, true);
    }
}

void IN_Init(void)
{
    cvar_t *vars[] = {
        &joy_enable,          &joy_deadzone_move,     &joy_deadzone_look, &joy_deadzone_trigger,
        &joy_sensitivity_yaw, &joy_sensitivity_pitch, &joy_invert};
    for (unsigned i = 0; i < sizeof vars / sizeof vars[0]; ++i)
        Cvar_RegisterVariable(vars[i]);
    Con_Printf("PS5 controller: %s\n", ps5_pad_open() ? "ready" : "unavailable");
}

void IN_Shutdown(void)
{
    IN_ClearStates();
    ps5_pad_close();
}

void IN_SendKeyEvents(void)
{
    ps5_pad_poll();
}

void IN_Commands(void)
{
    const uint32_t buttons = joy_enable.value ? ps5_pad_buttons() : 0;
    const float x = joy_enable.value ? ps5_pad_axis(ps5_pad_axis_left_x) / 32768.0f : 0;
    const float y = joy_enable.value ? ps5_pad_axis(ps5_pad_axis_left_y) / 32768.0f : 0;
    for (unsigned i = 0; i < sizeof pad_keys / sizeof pad_keys[0]; ++i)
    {
        qboolean down = (buttons & pad_keys[i].button) != 0;
        if (joy_enable.value && i >= 14)
            down = ps5_pad_axis(i == 14 ? ps5_pad_axis_left_trigger : ps5_pad_axis_right_trigger) /
                       32768.0f >
                   joy_deadzone_trigger.value;
        if (key_dest != key_game)
        {
            if (pad_keys[i].key == K_UPARROW)
                down |= y < -0.7f;
            if (pad_keys[i].key == K_DOWNARROW)
                down |= y > 0.7f;
            if (pad_keys[i].key == K_LEFTARROW)
                down |= x < -0.7f;
            if (pad_keys[i].key == K_RIGHTARROW)
                down |= x > 0.7f;
        }
        pad_event(i, down);
    }
}

static void stick(unsigned axis, float deadzone, float *x, float *y)
{
    *x = ps5_pad_axis(axis) / 32768.0f;
    *y = ps5_pad_axis(axis + 1) / 32768.0f;
    const float length = sqrtf(*x * *x + *y * *y);
    deadzone = CLAMP(0.0f, deadzone, 0.95f);
    const float amount = CLAMP(0.0f, (length - deadzone) / (1.0f - deadzone), 1.0f);
    const float scale = length > 0 ? amount * amount / length : 0;
    *x *= scale;
    *y *= scale;
}

void IN_Move(usercmd_t *cmd)
{
    cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
    if (!joy_enable.value || cl.paused || key_dest != key_game)
        return;
    float mx, my, lx, ly;
    stick(ps5_pad_axis_left_x, joy_deadzone_move.value, &mx, &my);
    stick(ps5_pad_axis_right_x, joy_deadzone_look.value, &lx, &ly);
    extern cvar_t sv_maxspeed, cl_minpitch, cl_maxpitch;
    float speed = cl_forwardspeed.value;
    if ((in_speed.state & 1) ^ (cl_alwaysrun.value != 0 || speed >= sv_maxspeed.value))
        speed = sv_maxspeed.value;
    else if (speed >= sv_maxspeed.value)
        speed = q_min(sv_maxspeed.value, speed / cl_movespeedkey.value);
    cmd->sidemove = speed * mx;
    cmd->forwardmove = -speed * my;
    if (CL_AngleLocked())
        return;
    cl.viewangles[YAW] -= lx * joy_sensitivity_yaw.value * host_frametime;
    cl.viewangles[PITCH] +=
        ly * joy_sensitivity_pitch.value * (joy_invert.value ? -1.0f : 1.0f) * host_frametime;
    if (lx || ly)
        V_StopPitchDrift();
    cl.viewangles[PITCH] = CLAMP(cl_minpitch.value, cl.viewangles[PITCH], cl_maxpitch.value);
}

void IN_UpdateInputMode(void)
{
}

void IN_ClearStates(void)
{
    for (unsigned i = 0; i < sizeof pad_keys / sizeof pad_keys[0]; ++i)
        pad_event(i, false);
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
