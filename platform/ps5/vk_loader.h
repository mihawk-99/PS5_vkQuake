/*
 * PS5 vkQuake - the part of a Vulkan loader this console does not have.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this exists. Vulkan's application-facing names for a promoted extension are
 * an alias pair, and the loader keeps that pair: an application that enabled
 * VK_KHR_get_physical_device_properties2 may ask for vkGetPhysicalDeviceProperties2
 * and receive the extension's entry point. This console has no loader - the driver
 * is linked into the title and its vkGetInstanceProcAddr answers only the names a
 * Vulkan 1.0 instance has, which is what it honestly is now - so vkQuake, which
 * enables the extension and then loads the *core* names, stopped on the first run
 * that reached them:
 *
 *   QUAKE ERROR: vkGetInstanceProcAddr failed to find vkGetPhysicalDeviceProperties2
 *
 * Measured on the console, build identity 10b174731bf50195. The two names are the
 * whole of what vkQuake resolves by a core name and are the pairs
 * VK_KHR_get_physical_device_properties2 promotes.
 *
 * What this is not. Not a dispatch table, not instance tracking, and not a
 * suffix rule: the table is named because a loader aliases exactly the pairs an
 * extension promotes, and a blanket "append KHR" would invent names that do not
 * exist. Retire the whole file when the driver claims Vulkan 1.1, at which point
 * the core names are the driver's own.
 */

#ifndef PS5_VK_LOADER_H
#define PS5_VK_LOADER_H

/* The driver's own lookup, as the shim holds it: this file does not include the
 * Vulkan header so that the host test can compile it against a fake one. */
typedef void *(*ps5_vk_proc_lookup)(void *handle, const char *name);

/* The driver's answer, with the loader's aliasing applied when it has none. */
void *ps5_vk_loader_proc_addr(ps5_vk_proc_lookup driver, void *handle, const char *name);

#endif
