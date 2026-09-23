/* Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Exercise the real adapter with engine headers and controlled pad samples. */
#include <assert.h>
#include "../platform/ps5/ps5_input.c"

client_state_t cl;
kbutton_t in_speed;
keydest_t key_dest;
double host_frametime;
cvar_t cl_forwardspeed, cl_alwaysrun, cl_movespeedkey, sv_maxspeed, cl_minpitch, cl_maxpitch;
static uint32_t buttons;
static int16_t axes[6];
static double now;
static int event_key[128], event_down[128], events, polls, closes;

bool ps5_pad_open(void)
{
    return true;
}
void ps5_pad_poll(void)
{
    ++polls;
}
void ps5_pad_close(void)
{
    ++closes;
}
uint32_t ps5_pad_buttons(void)
{
    return buttons;
}
int16_t ps5_pad_axis(uint32_t axis)
{
    return axes[axis];
}
double Sys_DoubleTime(void)
{
    return now;
}
void Cvar_RegisterVariable(cvar_t *var)
{
    var->value = atof(var->string);
}
void Con_Printf(const char *fmt, ...)
{
    (void)fmt;
}
qboolean CL_AngleLocked(void)
{
    return false;
}
void V_StopPitchDrift(void)
{
}
void Key_Event(int key, qboolean down)
{
    assert(events < 128);
    event_key[events] = key;
    event_down[events++] = down;
}

int main(void)
{
    IN_Init();
    key_dest = key_game;
    buttons = ps5_pad_cross;
    IN_SendKeyEvents();
    IN_Commands();
    IN_Commands();
    assert(polls == 1 && events == 1 && event_key[0] == K_ABUTTON && event_down[0]);
    buttons = 0;
    IN_Commands();
    assert(events == 2 && !event_down[1]);
    axes[5] = 32767;
    IN_Commands();
    assert(event_key[2] == K_RTRIGGER && event_down[2]);
    joy_enable.value = 0;
    IN_Commands();
    assert(events == 4 && !event_down[3]);
    joy_enable.value = 1;
    axes[5] = 0;
    key_dest = key_menu;
    axes[1] = -32768;
    IN_Commands();
    assert(event_key[4] == K_UPARROW && event_down[4]);
    now = 0.49;
    IN_Commands();
    assert(events == 5);
    now = 0.51;
    IN_Commands();
    assert(events == 6 && event_key[5] == K_UPARROW);
    IN_ClearStates();
    assert(events == 7 && !event_down[6]);
    memset(axes, 0, sizeof axes);
    key_dest = key_game;
    cl_forwardspeed.value = 200;
    sv_maxspeed.value = 320;
    cl_movespeedkey.value = 2;
    cl_minpitch.value = -70;
    cl_maxpitch.value = 80;
    host_frametime = 0.1;
    usercmd_t cmd = {0};
    axes[0] = 1000;
    axes[2] = -1000;
    IN_Move(&cmd);
    assert(cmd.sidemove == 0 && cl.viewangles[YAW] == 0);
    axes[0] = 32767;
    axes[1] = -32768;
    axes[2] = 32767;
    axes[3] = 32767;
    IN_Move(&cmd);
    assert(cmd.sidemove > 140 && cmd.forwardmove > 140);
    assert(sqrtf(cmd.sidemove * cmd.sidemove + cmd.forwardmove * cmd.forwardmove) < 200.1f);
    assert(cl.viewangles[YAW] < 0 && cl.viewangles[PITCH] > 0);
    joy_invert.value = 1;
    cl.viewangles[PITCH] = -69;
    host_frametime = 10;
    IN_Move(&cmd);
    assert(cl.viewangles[PITCH] == -70);
    key_dest = key_menu;
    IN_Move(&cmd);
    assert(cmd.forwardmove == 0 && cmd.sidemove == 0);
    key_dest = key_game;
    cl.paused = true;
    IN_Move(&cmd);
    assert(cmd.forwardmove == 0);
    IN_Shutdown();
    assert(closes == 1);
    puts(
        "input adapter: transitions, release, repeat, triggers, deadzone, movement and pitch PASS");
}
