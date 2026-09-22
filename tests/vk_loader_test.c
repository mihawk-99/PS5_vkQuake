/*
 * PS5 vkQuake - the loader's name aliasing, checked on the host.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What this is for. platform/ps5/vk_loader.c decides whether a lookup the engine
 * makes through SDL_Vulkan_GetVkGetInstanceProcAddr answers, and getting it wrong
 * produces no warning: the engine calls Sys_Error and the title stops after
 * vkCreateInstance. The console that produced this file failed exactly that way -
 * "vkGetInstanceProcAddr failed to find vkGetPhysicalDeviceProperties2" - so the
 * behaviour is checked here against a driver of the console's shape: a Vulkan 1.0
 * instance that advertises VK_KHR_get_physical_device_properties2 and therefore
 * answers the extension's spelling and not the core one.
 *
 * The implementation is included, not linked: it is C, this is C, and the point is
 * to exercise the real code rather than a restatement of it. tests/test_vk_loader.py
 * compiles and runs it.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../platform/ps5/vk_loader.c"

/* The console's shape: the extension's names answer, the core names do not, and
 * anything else is missing. The pointers are sentinels - nothing is called
 * through them - so a lookup's identity is what the check compares. */
static void *const properties2_khr = (void *)(uintptr_t)0x1001;
static void *const features2_khr = (void *)(uintptr_t)0x1002;
static void *const direct = (void *)(uintptr_t)0x1003;
static int lookups;

static void *fake_driver(void *handle, const char *name)
{
    (void)handle;
    ++lookups;
    if (strcmp(name, "vkGetPhysicalDeviceProperties2KHR") == 0)
        return properties2_khr;
    if (strcmp(name, "vkGetPhysicalDeviceFeatures2KHR") == 0)
        return features2_khr;
    if (strcmp(name, "vkGetDeviceProcAddr") == 0)
        return direct;
    return NULL;
}

int main(void)
{
    int checks = 0;

    /* The pair the console refused: the core spelling resolves to the
     * extension's entry point, which is what a loader does for the engine. */
    if (ps5_vk_loader_proc_addr(fake_driver, NULL, "vkGetPhysicalDeviceProperties2") !=
        properties2_khr)
    {
        printf("FAIL: the core properties2 name did not resolve through the extension\n");
        return 1;
    }
    printf("core vkGetPhysicalDeviceProperties2 resolves to the extension's entry point\n");
    ++checks;

    if (ps5_vk_loader_proc_addr(fake_driver, NULL, "vkGetPhysicalDeviceFeatures2") != features2_khr)
    {
        printf("FAIL: the core features2 name did not resolve through the extension\n");
        return 1;
    }
    printf("core vkGetPhysicalDeviceFeatures2 resolves to the extension's entry point\n");
    ++checks;

    /* A name the driver answers is not aliased and costs one lookup: an alias
     * applied first would ask for a name that does not exist. */
    lookups = 0;
    if (ps5_vk_loader_proc_addr(fake_driver, NULL, "vkGetDeviceProcAddr") != direct || lookups != 1)
    {
        printf("FAIL: a name the driver answers was aliased (%d lookups)\n", lookups);
        return 1;
    }
    printf("a name the driver answers is returned as it is, in one lookup\n");
    ++checks;

    /* A missing name stays missing, rather than resolving to something else. */
    if (ps5_vk_loader_proc_addr(fake_driver, NULL, "vkGetPhysicalDeviceMemoryProperties2") != NULL)
    {
        printf("FAIL: a name with no extension twin resolved anyway\n");
        return 1;
    }
    printf("a name with no extension twin is reported missing\n");
    ++checks;

    /* Neither a driver nor a name is a lookup. */
    lookups = 0;
    if (ps5_vk_loader_proc_addr(NULL, NULL, "vkGetPhysicalDeviceProperties2") != NULL ||
        ps5_vk_loader_proc_addr(fake_driver, NULL, NULL) != NULL || lookups != 0)
    {
        printf("FAIL: a null driver or name was passed through (%d lookups)\n", lookups);
        return 1;
    }
    printf("a null driver and a null name resolve to nothing without a lookup\n");
    ++checks;

    /* The table is exactly the promoted pair: a third name appearing in it would
     * be an alias this console has no evidence for. */
    if (sizeof(ps5_vk_aliases) / sizeof(ps5_vk_aliases[0]) != 2)
    {
        printf("FAIL: the alias table holds %zu names, not the promoted pair\n",
               sizeof(ps5_vk_aliases) / sizeof(ps5_vk_aliases[0]));
        return 1;
    }
    printf("the alias table is the promoted pair and nothing else\n");
    ++checks;

    printf("%d of %d checks passed\n", checks, checks);
    printf("all checks passed\n");
    return 0;
}
