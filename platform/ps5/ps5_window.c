/*
 * PS5 vkQuake - the one screen, and the Vulkan surface that draws into it.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The declarations and the reasoning are in platform/ps5/SDL.h and
 * platform/ps5/SDL_vulkan.h; what is here is the behaviour, and two things about
 * it are worth stating because they are the difference between this and a desktop.
 *
 * There is one display mode and it is 3840x2160. That is not a default this file
 * chose: ../PS5_Vulkan drives VideoOut at that size and
 * ps5vk_CreateDisplayPlaneSurfaceKHR asserts pCreateInfo->imageExtent equals it,
 * so a surface at any other size is refused rather than scaled. Every mode query
 * below therefore answers with that one mode, and SDL_Vulkan_GetDrawableSize
 * returns it whatever size a window was asked to be - because the honest answer
 * to "how big is the drawable" on this console is "as big as the screen", and
 * because vkQuake compares the drawable against the surface's currentExtent in
 * GL_CreateSwapChain and would otherwise build a swapchain the driver rejects.
 *
 * The surface is a VkDisplayPlaneSurfaceKHR, which is what a machine with no
 * window system uses. The sequence is the one RetroArch's khr_display context
 * driver proved on this hardware - enumerate displays, planes and modes, pick the
 * plane whose capabilities allow the mode, create the surface - collapsed to the
 * shape the console actually has, which is exactly one of each. The plural forms
 * are still asked rather than assumed, so that a driver which grows a second
 * display is described correctly by this code instead of silently mis-picked.
 *
 * Every Vulkan entry point here is resolved through vkGetInstanceProcAddr, which
 * is the driver's own exported symbol rather than a loaded loader: ../PS5_Vulkan
 * measured that a title cannot dlopen a repository-built .so, so the driver is
 * linked into the title and this is a direct call.
 */

#include "SDL_vulkan.h"
#include "vk_loader.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* sdl_ps5.c: the shim's kernel-entry counts, as per-frame fields. */
int ps5_sdl_counts_format(char *line, size_t bytes, unsigned long long frames);

/* The console's mode, matching ../PS5_Vulkan's driver/ps5vk_wsi.c. Named here as
 * constants rather than taken from the driver because the driver's copies are
 * private to it, and the assert that ties the two together is in the driver. If
 * they ever disagree, surface creation fails loudly on the first run. */
#define PS5_SCREEN_WIDTH 3840
#define PS5_SCREEN_HEIGHT 2160
#define PS5_SCREEN_REFRESH_HZ 60

/* The one entry point the driver exports by name. Declared rather than included
 * from a Vulkan loader header, because there is no loader here. */
extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                                      const char *name);

struct SDL_Window
{
    Uint32 flags;
    int width;
    int height;
};

struct SDL_Cursor
{
    int id;
};

/* A subsystem mask, so SDL_QuitSubSystem can be honest about what it stops. The
 * video subsystem on this console has no state to tear down, but
 * SDL_GetCurrentVideoDriver only answers while it is up. */
static Uint32 initialised_subsystems;

/* SDL's own names for the driver and the display, so that a log line or the video
 * menu says something a person recognises rather than something invented here. */
static const char video_driver_name[] = "PS5 VideoOut";

static void fill_mode(SDL_DisplayMode *mode)
{
    mode->format = SDL_PIXELFORMAT_ARGB8888;
    mode->w = PS5_SCREEN_WIDTH;
    mode->h = PS5_SCREEN_HEIGHT;
    mode->refresh_rate = PS5_SCREEN_REFRESH_HZ;
    mode->driverdata = NULL;
}

int SDL_InitSubSystem(Uint32 flags)
{
    initialised_subsystems |= flags;
    return 0;
}

void SDL_QuitSubSystem(Uint32 flags)
{
    initialised_subsystems &= ~flags;
}

const char *SDL_GetCurrentVideoDriver(void)
{
    return (initialised_subsystems & SDL_INIT_VIDEO) ? video_driver_name : NULL;
}

int SDL_GetNumVideoDisplays(void)
{
    return 1;
}

/* --- windows -------------------------------------------------------------
 *
 * A window is the flags and the size, and nothing else. Upstream reads the flags
 * back - VID_GetFullscreen is this masked with SDL_WINDOW_FULLSCREEN, and
 * VID_IsMinimized the minimized bit - so they are tracked rather than ignored.
 * The size is tracked because SDL_GetWindowSize has a caller in the input layer,
 * and because discarding a request silently is how a caller ends up debugging a
 * number it set itself.
 */

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    (void)title;
    (void)x;
    (void)y;
    SDL_Window *window = calloc(1, sizeof *window);
    if (!window)
    {
        SDL_SetError("out of memory creating a window");
        return NULL;
    }
    window->flags = flags;
    /* A window that was never asked to be hidden is shown, and on this console it
     * always has the focus: there is nothing else to focus. Both bits matter -
     * the input layer unblocks sound when the focus is gained and blocks it when
     * lost, and a title that thinks it is unfocused is a title that never plays
     * audio. */
    if (!(flags & SDL_WINDOW_HIDDEN))
        window->flags |= SDL_WINDOW_SHOWN;
    window->flags |= SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
    window->width = w;
    window->height = h;
    return window;
}

void SDL_DestroyWindow(SDL_Window *window)
{
    free(window);
}

void SDL_ShowWindow(SDL_Window *window)
{
    if (window)
        window->flags = (window->flags | SDL_WINDOW_SHOWN) & ~SDL_WINDOW_HIDDEN;
}

void SDL_RaiseWindow(SDL_Window *window)
{
    (void)window; /* Nothing to raise above. */
}

void SDL_SetWindowSize(SDL_Window *window, int w, int h)
{
    if (!window)
        return;
    window->width = w;
    window->height = h;
}

void SDL_SetWindowPosition(SDL_Window *window, int x, int y)
{
    (void)window;
    (void)x;
    (void)y; /* One screen, one position. */
}

/* SDL2's contract: the argument is 0 to leave fullscreen, SDL_WINDOW_FULLSCREEN
 * or SDL_WINDOW_FULLSCREEN_DESKTOP to enter it, and the return is 0 on success.
 * The fullscreen bits are tracked because VID_GetFullscreen reads them back and
 * the answer decides modestate - that is, whether the engine believes it is
 * fullscreen, which on this console it always is. */
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags)
{
    if (!window)
        return -1;
    window->flags &= ~(SDL_WINDOW_FULLSCREEN | 0x00001000u);
    window->flags |= flags & (SDL_WINDOW_FULLSCREEN | 0x00001000u);
    return 0;
}

void SDL_SetWindowDisplayMode(SDL_Window *window, const SDL_DisplayMode *mode)
{
    (void)window;
    (void)mode; /* The screen's mode is not the window's to choose. */
}

void SDL_SetWindowBordered(SDL_Window *window, SDL_bool bordered)
{
    if (!window)
        return;
    if (bordered)
        window->flags &= ~SDL_WINDOW_BORDERLESS;
    else
        window->flags |= SDL_WINDOW_BORDERLESS;
}

void SDL_SetWindowTitle(SDL_Window *window, const char *title)
{
    (void)window;
    (void)title;
}

Uint32 SDL_GetWindowFlags(SDL_Window *window)
{
    return window ? window->flags : 0;
}

int SDL_GetWindowDisplayIndex(SDL_Window *window)
{
    (void)window;
    return 0;
}

Uint32 SDL_GetWindowPixelFormat(SDL_Window *window)
{
    (void)window;
    return SDL_PIXELFORMAT_ARGB8888;
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h)
{
    if (w)
        *w = window ? window->width : 0;
    if (h)
        *h = window ? window->height : 0;
}

/* --- displays and modes ---------------------------------------------------
 *
 * One display, one mode, answered for index 0 and refused for anything else. The
 * refusals are the point: a caller that asks about a second display on this
 * console has a wrong idea of the machine, and an error it can read is better
 * than a plausible answer that hides it.
 */

int SDL_GetNumDisplayModes(int displayIndex)
{
    return displayIndex == 0 ? 1 : -1;
}

int SDL_GetDisplayMode(int displayIndex, int modeIndex, SDL_DisplayMode *mode)
{
    if (displayIndex != 0 || modeIndex != 0 || !mode)
    {
        SDL_SetError("this console has one display with one mode");
        return -1;
    }
    fill_mode(mode);
    return 0;
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode)
{
    return SDL_GetDisplayMode(displayIndex, 0, mode);
}

int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode)
{
    return SDL_GetDisplayMode(displayIndex, 0, mode);
}

/* --- cursors -------------------------------------------------------------- */

SDL_Cursor *SDL_CreateSystemCursor(int id)
{
    SDL_Cursor *cursor = calloc(1, sizeof *cursor);
    if (cursor)
        cursor->id = id;
    return cursor;
}

void SDL_FreeCursor(SDL_Cursor *cursor)
{
    free(cursor);
}

void SDL_SetCursor(SDL_Cursor *cursor)
{
    (void)cursor;
}

void SDL_ShowCursor(int toggle)
{
    (void)toggle;
}

int SDL_SetRelativeMouseMode(SDL_bool enabled)
{
    (void)enabled;
    return 0;
}

SDL_bool SDL_GetRelativeMouseMode(void)
{
    return SDL_FALSE;
}

void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y)
{
    (void)window;
    (void)x;
    (void)y;
}

void SDL_StartTextInput(void)
{
}

void SDL_StopTextInput(void)
{
}

/* --- the Vulkan surface ---------------------------------------------------
 *
 * See the file header: five calls and one create, in the order RetroArch's
 * khr_display context driver proved on this hardware.
 */

/* What SDL hands the engine is a loader's lookup, not the driver's raw one: the
 * engine asks for the core spelling of a promoted extension's entry point and
 * expects the loader's alias (platform/ps5/vk_loader.c, and the console run that
 * made the difference visible). */
extern void ps5_trace(const char *line);
static PFN_vkGetDeviceProcAddr device_proc_addr;
static PFN_vkQueuePresentKHR queue_present;

static VKAPI_ATTR VkResult VKAPI_CALL traced_present(VkQueue queue, const VkPresentInfoKHR *info)
{
    /* Queue presentation is externally synchronized. One witness per boot,
     * plus every failure, keeps the trace useful without per-frame writes. */
    static int reported;
    const VkResult result = queue_present(queue, info);
    if (!reported || result != VK_SUCCESS)
    {
        char line[80];
        snprintf(line, sizeof line, "vkQueuePresentKHR -> %d", (int)result);
        ps5_trace(line);
        reported = 1;
    }
    // Periodic counts distinguish a living process from continued presentation.
    static Uint64 frames, last_frames, last_tick;
    if (result == VK_SUCCESS)
    {
        const Uint64 now = SDL_GetTicks64();
        ++frames;
        if (frames == 1)
        {
            last_tick = now;
            last_frames = frames;
        }
        else if (now - last_tick >= 10000)
        {
            char line[384];
            const int used =
                snprintf(line, sizeof line, "PS5 present: frames=%llu interval=%llu ms fps=%.2f",
                         (unsigned long long)frames, (unsigned long long)(now - last_tick),
                         (double)(frames - last_frames) * 1000.0 / (double)(now - last_tick));
            // The shim's per-frame kernel-entry counts, in the same single write.
            if (used > 0 && (size_t)used < sizeof line)
                ps5_sdl_counts_format(line + used, sizeof line - (size_t)used,
                                      (unsigned long long)(frames - last_frames));
            ps5_trace(line);
            last_tick = now;
            last_frames = frames;
        }
    }
    return result;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL traced_device_proc(VkDevice device,
                                                                   const char *name)
{
    PFN_vkVoidFunction proc = device_proc_addr(device, name);
    if (proc && strcmp(name, "vkQueuePresentKHR") == 0)
    {
        queue_present = (PFN_vkQueuePresentKHR)proc;
        return (PFN_vkVoidFunction)traced_present;
    }
    return proc;
}

static void *ps5_window_proc_addr(void *instance, const char *name)
{
    void *proc =
        ps5_vk_loader_proc_addr((ps5_vk_proc_lookup)(void *)vkGetInstanceProcAddr, instance, name);
    if (proc && strcmp(name, "vkGetDeviceProcAddr") == 0)
    {
        device_proc_addr = (PFN_vkGetDeviceProcAddr)proc;
        return (void *)traced_device_proc;
    }
    return proc;
}

void *SDL_Vulkan_GetVkGetInstanceProcAddr(void)
{
    return (void *)ps5_window_proc_addr;
}

int SDL_Vulkan_LoadLibrary(const char *path)
{
    (void)path; /* The driver is linked, not loaded. */
    return 0;
}

void SDL_Vulkan_UnloadLibrary(void)
{
}

/* The two extensions a surface here needs. SDL's contract is the two-call form:
 * pNames NULL reports the count, otherwise it writes that many names. */
int SDL_Vulkan_GetInstanceExtensions(SDL_Window *window, unsigned int *pCount, const char **pNames)
{
    (void)window;
    static const char *const extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                                             VK_KHR_DISPLAY_EXTENSION_NAME};
    const unsigned int count = (unsigned int)(sizeof extensions / sizeof extensions[0]);

    if (!pCount)
    {
        SDL_SetError("SDL_Vulkan_GetInstanceExtensions needs a count");
        return 0;
    }
    if (!pNames)
    {
        *pCount = count;
        return 1;
    }
    if (*pCount < count)
    {
        SDL_SetError("room for %u extension names, %u are needed", *pCount, count);
        return 0;
    }
    for (unsigned int index = 0; index < count; ++index)
        pNames[index] = extensions[index];
    *pCount = count;
    return 1;
}

/* What the drawable actually is. Not the window's size - see the file header. */
void SDL_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h)
{
    (void)window;
    if (w)
        *w = PS5_SCREEN_WIDTH;
    if (h)
        *h = PS5_SCREEN_HEIGHT;
}

/* The whole of the console's Vulkan surface, and the reason M2 is a milestone.
 *
 * Every failure below reports through SDL_SetError, because upstream turns that
 * straight into Sys_Error with the count it got, and a run that says "no
 * displays" is worth more than one that says the surface was NULL. */
int SDL_Vulkan_CreateSurface(SDL_Window *window, VkInstance instance, VkSurfaceKHR *surface)
{
    (void)window;
    if (!instance || !surface)
    {
        SDL_SetError("SDL_Vulkan_CreateSurface needs an instance and a surface");
        return 0;
    }

    PFN_vkGetInstanceProcAddr get_proc = vkGetInstanceProcAddr;
    PFN_vkEnumeratePhysicalDevices enumerate_devices =
        (PFN_vkEnumeratePhysicalDevices)get_proc(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR get_displays =
        (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)get_proc(
            instance, "vkGetPhysicalDeviceDisplayPropertiesKHR");
    PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR get_planes =
        (PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)get_proc(
            instance, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
    PFN_vkGetDisplayModePropertiesKHR get_modes =
        (PFN_vkGetDisplayModePropertiesKHR)get_proc(instance, "vkGetDisplayModePropertiesKHR");
    PFN_vkCreateDisplayPlaneSurfaceKHR create_surface =
        (PFN_vkCreateDisplayPlaneSurfaceKHR)get_proc(instance, "vkCreateDisplayPlaneSurfaceKHR");

    if (!enumerate_devices || !get_displays || !get_planes || !get_modes || !create_surface)
    {
        SDL_SetError("the driver does not implement VK_KHR_display");
        return 0;
    }

    uint32_t device_count = 0;
    if (enumerate_devices(instance, &device_count, NULL) != VK_SUCCESS || device_count == 0)
    {
        SDL_SetError("the instance has no physical device");
        return 0;
    }
    VkPhysicalDevice device = VK_NULL_HANDLE;
    if (enumerate_devices(instance, &device_count, &device) != VK_SUCCESS)
    {
        SDL_SetError("could not enumerate the physical device");
        return 0;
    }

    uint32_t display_count = 0;
    if (get_displays(device, &display_count, NULL) != VK_SUCCESS || display_count == 0)
    {
        SDL_SetError("the device reports no displays");
        return 0;
    }
    VkDisplayPropertiesKHR *displays = calloc(display_count, sizeof *displays);
    if (!displays || get_displays(device, &display_count, displays) != VK_SUCCESS)
    {
        free(displays);
        SDL_SetError("could not read the display properties");
        return 0;
    }

    uint32_t plane_count = 0;
    if (get_planes(device, &plane_count, NULL) != VK_SUCCESS || plane_count == 0)
    {
        free(displays);
        SDL_SetError("the device reports no display planes");
        return 0;
    }
    VkDisplayPlanePropertiesKHR *planes = calloc(plane_count, sizeof *planes);
    if (!planes || get_planes(device, &plane_count, planes) != VK_SUCCESS)
    {
        free(displays);
        free(planes);
        SDL_SetError("could not read the display plane properties");
        return 0;
    }

    /* The first display that offers a mode at the size the driver will accept.
     * The console has one, but the loop is written for the general case so that a
     * driver with more is described correctly rather than assumed away. */
    VkDisplayKHR chosen_display = VK_NULL_HANDLE;
    VkDisplayModeKHR chosen_mode = VK_NULL_HANDLE;
    for (uint32_t index = 0; index < display_count && chosen_mode == VK_NULL_HANDLE; ++index)
    {
        uint32_t mode_count = 0;
        if (get_modes(device, displays[index].display, &mode_count, NULL) != VK_SUCCESS ||
            mode_count == 0)
            continue;
        VkDisplayModePropertiesKHR *modes = calloc(mode_count, sizeof *modes);
        if (!modes || get_modes(device, displays[index].display, &mode_count, modes) != VK_SUCCESS)
        {
            free(modes);
            continue;
        }
        for (uint32_t mode_index = 0; mode_index < mode_count; ++mode_index)
        {
            if (modes[mode_index].parameters.visibleRegion.width == PS5_SCREEN_WIDTH &&
                modes[mode_index].parameters.visibleRegion.height == PS5_SCREEN_HEIGHT)
            {
                chosen_display = displays[index].display;
                chosen_mode = modes[mode_index].displayMode;
                break;
            }
        }
        free(modes);
    }

    if (chosen_mode == VK_NULL_HANDLE)
    {
        free(displays);
        free(planes);
        SDL_SetError("no display offers a %dx%d mode", PS5_SCREEN_WIDTH, PS5_SCREEN_HEIGHT);
        return 0;
    }

    /* The plane that can drive that display. Only one can on this console, and
     * the check is kept because picking a plane by index rather than by capability
     * is the mistake this sequence exists to avoid. */
    uint32_t chosen_plane = UINT32_MAX;
    for (uint32_t index = 0; index < plane_count; ++index)
    {
        if (planes[index].currentDisplay == VK_NULL_HANDLE ||
            planes[index].currentDisplay == chosen_display)
        {
            chosen_plane = index;
            break;
        }
    }
    if (chosen_plane == UINT32_MAX)
    {
        free(displays);
        free(planes);
        SDL_SetError("no display plane can drive the chosen display");
        return 0;
    }

    const VkDisplaySurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .displayMode = chosen_mode,
        .planeIndex = chosen_plane,
        .planeStackIndex = planes[chosen_plane].currentStackIndex,
        .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .globalAlpha = 1.0f,
        .alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,
        .imageExtent = {PS5_SCREEN_WIDTH, PS5_SCREEN_HEIGHT},
    };

    const VkResult result = create_surface(instance, &create_info, NULL, surface);

    free(displays);
    free(planes);

    if (result != VK_SUCCESS)
    {
        SDL_SetError("vkCreateDisplayPlaneSurfaceKHR failed with %d", (int)result);
        return 0;
    }
    return 1;
}
