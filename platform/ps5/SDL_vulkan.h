/*
 * PS5 vkQuake - the SDL Vulkan-surface helpers, on the console's own surface.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this file is the whole of M2's platform work. vkQuake has no windowing of
 * its own: gl_vidsdl.c asks SDL for five things and then does everything else
 * itself in plain Vulkan. It asks SDL which instance extensions it needs
 * (SDL_Vulkan_GetInstanceExtensions), where the loader's entry point is
 * (SDL_Vulkan_GetVkGetInstanceProcAddr), for a VkSurfaceKHR to draw into
 * (SDL_Vulkan_CreateSurface), how big the drawable is, and for a library handle.
 * Replace those five and the other five thousand lines of instance creation,
 * device selection, feature negotiation, swapchain building, frame recording and
 * present run unmodified - which is the entire point of doing it here rather than
 * by editing upstream.
 *
 * What the console answers with, and why each answer is the true one rather than
 * a convenient one:
 *
 *   Instance extensions  VK_KHR_surface and VK_KHR_display. There is no window
 *                        system on this console, and ../PS5_Vulkan's instance
 *                        extension table declares exactly those two (plus the
 *                        Mesa runtime's own), so a list that named a platform
 *                        surface extension would name something no driver here
 *                        implements.
 *   Entry point          the linked driver's vkGetInstanceProcAddr. ../PS5_Vulkan
 *                        measured that a PS5 title cannot dlopen a repository
 *                        built .so, so the driver is linked rather than loaded,
 *                        and the symbol is defined rather than searched for.
 *   Surface              a VkDisplayPlaneSurfaceKHR through VK_KHR_display,
 *                        which is the same surface RetroArch's khr_display
 *                        context driver built and the one ../PS5_Vulkan's
 *                        driver/ps5vk_wsi.c presents to VideoOut.
 *   Drawable size        the mode the display was opened at, fixed at 1920x1080.
 *
 * The window handle SDL would carry is not needed and not faked into something
 * meaningful: it is a non-null token so that upstream's `if (!draw_context)`
 * checks behave as they do on a desktop, and nothing dereferences it.
 */

#ifndef PS5_SDL_VULKAN_COMPAT_H
#define PS5_SDL_VULKAN_COMPAT_H

#include <vulkan/vulkan_core.h>

#include "SDL.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Loading and unloading the loader. The driver is linked into the title, so
     * there is nothing to load; these report success because the thing they promise
     * - that the Vulkan entry point below is callable - is already true. */
    int SDL_Vulkan_LoadLibrary(const char *path);
    void SDL_Vulkan_UnloadLibrary(void);
    void *SDL_Vulkan_GetVkGetInstanceProcAddr(void);

    /* The instance extensions a surface on this console needs. SDL's contract is the
     * two-call form: with pNames NULL it reports the count, otherwise it writes that
     * many names and reports the count again. */
    int SDL_Vulkan_GetInstanceExtensions(SDL_Window *window, unsigned int *pCount,
                                         const char **pNames);

    /* The surface itself. */
    int SDL_Vulkan_CreateSurface(SDL_Window *window, VkInstance instance, VkSurfaceKHR *surface);

    /* The drawable, in pixels. Fixed, because the display is opened at one mode. */
    void SDL_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* PS5_SDL_VULKAN_COMPAT_H */
