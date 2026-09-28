/* PS5 vkQuake - the RADV archive's entry point under the name the port uses.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * RADV exports the ICD's vk_icdGetInstanceProcAddr; the port resolves every
 * Vulkan command through vkGetInstanceProcAddr, as ps5vk's archive names it. */
#if defined(PS5_VKQUAKE_RADV)
#include <vulkan/vulkan.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance,
                                                                   const char *name);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                               const char *name)
{
    return vk_icdGetInstanceProcAddr(instance, name);
}
#endif
