/*
 * PS5 vkQuake - the loader's name aliasing, which is all of the loader vkQuake
 * needs beyond the global trampolines in platform/ps5/vk_globals.c.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The reasoning is in platform/ps5/vk_loader.h; what is here is the behaviour and
 * the table. The table is the two extension/core pairs
 * VK_KHR_get_physical_device_properties2 promotes, which is every name vkQuake
 * loads by its core spelling (Quake/gl_vidsdl.c, the instance-proc block): the
 * surface, swapchain and debug-utils lookups beside them are either Vulkan 1.0
 * names or already spelled with their extension's suffix.
 */

#include "vk_loader.h"

#include <stddef.h>
#include <string.h>

static const struct ps5_vk_alias
{
    const char *core;
    const char *extension;
} ps5_vk_aliases[] = {
    {"vkGetPhysicalDeviceProperties2", "vkGetPhysicalDeviceProperties2KHR"},
    {"vkGetPhysicalDeviceFeatures2", "vkGetPhysicalDeviceFeatures2KHR"},
};

void *ps5_vk_loader_proc_addr(ps5_vk_proc_lookup driver, void *handle, const char *name)
{
    if (driver == NULL || name == NULL)
        return NULL;

    void *const found = driver(handle, name);
    if (found != NULL)
        return found;

    for (size_t i = 0; i < sizeof(ps5_vk_aliases) / sizeof(ps5_vk_aliases[0]); ++i)
    {
        if (strcmp(name, ps5_vk_aliases[i].core) == 0)
            return driver(handle, ps5_vk_aliases[i].extension);
    }

    /* The driver answers what it has; a name it does not answer and this table
     * does not alias is missing, which is what a loader reports as NULL too. */
    return NULL;
}
