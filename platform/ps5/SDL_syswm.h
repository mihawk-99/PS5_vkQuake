/*
 * PS5 vkQuake - SDL_syswm.h, which this console has nothing to put in.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * vkQuake's pl_linux.c includes this header and, in the SDL2 path, uses nothing
 * from it: the only thing it would ask for is the platform window handle, and
 * that is inside the `#ifdef USE_SDL3` branch, which is not the branch this port
 * compiles. So the header exists to be included, and says so rather than defining
 * an SDL_SysWMinfo that would describe a window manager this console does not
 * have.
 *
 * If a caller ever needs a field from here, that is the signal that the port is
 * trying to reach a platform layer through SDL rather than through the console's
 * own APIs - which is the thing this whole directory exists to avoid.
 */

#ifndef PS5_SDL_SYSWM_COMPAT_H
#define PS5_SDL_SYSWM_COMPAT_H

#include "SDL.h"

#endif /* PS5_SDL_SYSWM_COMPAT_H */
