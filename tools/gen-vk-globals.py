#!/usr/bin/env python3
"""Generate the Vulkan global entry points this console has no loader for.

    python3 tools/gen-vk-globals.py            write platform/ps5/vk_globals.c
    python3 tools/gen-vk-globals.py --check    fail if it is out of date

Why this file has to exist at all.

vkQuake links the Vulkan loader on the systems it was written for. The loader's
job is to export every *global* command - vkCreateInstance, vkCreateDevice, and
the roughly eighty others that take no dispatchable handle to start from - as an
ordinary symbol, so that an application can just call them. Everything else, the
per-instance and per-device commands, vkQuake resolves through
vkGetInstanceProcAddr, which is the driver's own exported symbol.

This console has no loader. ../PS5_Vulkan's libps5vk.ps5.a defines
vkGetInstanceProcAddr and vk_icdGetInstanceProcAddr and nothing else at global
scope, and its Mesa runtime archive is utility code rather than a loader. That is
not an oversight on the driver's part: RetroArch never noticed, because its Vulkan
code resolves every entry point through the dispatch pointer and so never
references a global symbol. vkQuake does reference them, so the port has to
provide what a loader would have.

What this generates is that loader's trampolines and nothing else: one function
per global command, each resolving the real one through vkGetInstanceProcAddr on
first call and forwarding to it. No dispatch, no layer support, no instance
tracking - a loader is a large program and this is the eighty lines of it that a
statically linked title actually needs.

Why generate rather than write. The signatures are not the interesting part and
transcribing seventy-nine of them by hand is how a parameter gets its types subtly
reordered in a way the compiler accepts because both sides are pointers. They are
read out of the Vulkan header the build already uses, so the generated file cannot
disagree with the header the rest of the engine was compiled against.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / 'vendor/vkQuake/Windows/misc/include/vulkan/vulkan_core.h'
OUTPUT = ROOT / 'platform/ps5/vk_globals.c'

# The global commands vkQuake references. Taken from the engine archive's
# undefined symbols, which is the list that matters: a command nothing calls does
# not need a trampoline, and one that is called cannot be missed. The generator
# re-derives it from the archive when the archive is there, and falls back to this
# list so the file can still be regenerated on a clean checkout.
FALLBACK = """vkAllocateCommandBuffers vkAllocateDescriptorSets vkAllocateMemory
vkBeginCommandBuffer vkBindBufferMemory vkBindImageMemory vkCmdBeginRenderPass
vkCmdBindDescriptorSets vkCmdBindIndexBuffer vkCmdBindVertexBuffers vkCmdBlitImage
vkCmdCopyBuffer vkCmdCopyBufferToImage vkCmdCopyImageToBuffer vkCmdDispatch vkCmdDraw
vkCmdDrawIndexed vkCmdEndRenderPass vkCmdExecuteCommands vkCmdNextSubpass
vkCmdPipelineBarrier vkCmdResetQueryPool vkCmdSetDepthBias vkCmdSetScissor
vkCmdSetViewport vkCmdWriteTimestamp vkCreateBuffer vkCreateBufferView
vkCreateCommandPool vkCreateComputePipelines vkCreateDescriptorPool
vkCreateDescriptorSetLayout vkCreateDevice vkCreateFence vkCreateFramebuffer
vkCreateGraphicsPipelines vkCreateImage vkCreateImageView vkCreateInstance
vkCreatePipelineLayout vkCreateQueryPool vkCreateRenderPass vkCreateSampler
vkCreateSemaphore vkCreateShaderModule vkDestroyBuffer vkDestroyFramebuffer
vkDestroyImage vkDestroyImageView vkDestroyPipeline vkDestroyRenderPass
vkDestroySampler vkDestroySemaphore vkDestroyShaderModule vkDeviceWaitIdle
vkEndCommandBuffer vkEnumerateDeviceExtensionProperties
vkEnumerateInstanceExtensionProperties vkEnumeratePhysicalDevices
vkFlushMappedMemoryRanges vkFreeDescriptorSets vkFreeMemory
vkGetBufferMemoryRequirements vkGetDeviceQueue vkGetImageMemoryRequirements
vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFormatProperties
vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceMemoryProperties
vkGetPhysicalDeviceProperties vkGetPhysicalDeviceQueueFamilyProperties
vkGetQueryPoolResults vkInvalidateMappedMemoryRanges vkMapMemory vkQueueSubmit
vkResetFences vkUnmapMemory vkUpdateDescriptorSets vkWaitForFences""".split()

# vkGetInstanceProcAddr is the one global the driver does export, so it is not
# forwarded - it is what everything else is forwarded through.
EXCLUDE = {'vkGetInstanceProcAddr'}

PFN = re.compile(r'typedef\s+(?P<ret>[^;\n]+?)\s*\(VKAPI_PTR\s*\*\s*(?P<pfn>PFN_vk\w+)\)\s*\((?P<params>[^;]*?)\)\s*;')


def split_params(text: str) -> list[str]:
    """Split a parameter list on top-level commas only."""
    text = ' '.join(text.split())
    if not text or text == 'void':
        return []
    parts, depth, current = [], 0, ''
    for character in text:
        if character == '(':
            depth += 1
        elif character == ')':
            depth -= 1
        if character == ',' and depth == 0:
            parts.append(current.strip())
            current = ''
        else:
            current += character
    if current.strip():
        parts.append(current.strip())
    return parts


def split_declaration(parameter: str) -> tuple[str, str]:
    """Split 'const VkAllocationCallbacks* pAllocator' into type and name.

    The name is the last identifier, and the type is everything before it. That
    is exactly the shape every Vulkan command parameter has, which is why this
    does not need to parse C.
    """
    match = re.search(r'([A-Za-z_]\w*)\s*$', parameter)
    if not match:
        raise SystemExit(f'cannot find a parameter name in {parameter!r}')
    name = match.group(1)
    return parameter[:match.start()].strip(), name


# The commands whose success is worth a line as much as their failure: they are the
# spine of M2, and "reached it, and it worked" is the answer a console run exists to
# give. Everything else reports only when it is refused.
ALWAYS = {'vkCreateInstance', 'vkCreateDevice', 'vkEnumeratePhysicalDevices'}


def wanted_commands() -> list[str]:
    """The globals the engine archive needs, or the fallback list."""
    import subprocess
    archive = ROOT / 'build/vkquake/libvkquake_engine.ps5.a'
    nm = Path('.deps/native/ps5-payload-sdk/bin/llvm-nm')
    if archive.is_file() and nm.is_file():
        try:
            out = subprocess.run([str(nm), '--undefined-only', str(archive)],
                                 capture_output=True, text=True, check=True).stdout
            names = sorted({n for n in re.findall(r'\bvk[A-Z]\w*', out)} - EXCLUDE)
            if names:
                return names
        except Exception:
            pass
    return sorted(set(FALLBACK) - EXCLUDE)


def main() -> int:
    check = '--check' in sys.argv[1:]
    if not HEADER.is_file():
        raise SystemExit(f'no Vulkan header at {HEADER}; run tools/fetch-vkquake.sh')

    header = HEADER.read_text()
    signatures = {}
    for match in PFN.finditer(header):
        command = match.group('pfn')[len('PFN_'):]
        signatures[command] = (match.group('ret').strip(), split_params(match.group('params')))

    commands = wanted_commands()
    missing = [c for c in commands if c not in signatures]
    if missing:
        raise SystemExit('the header has no prototype for: ' + ', '.join(missing))

    # A return type that swallowed a keyword means the pattern matched across a
    # declaration boundary, which is how the first version of this emitted
    # `VKAPI_ATTR typedef VkResult VKAPI_CALL vkCreateInstance(...)`. The compiler
    # catches that, but only for whichever command it happens to hit, so it is
    # checked here for all of them.
    for command in commands:
        ret = signatures[command][0]
        if 'typedef' in ret or '{' in ret or not ret:
            raise SystemExit(f'{command}: implausible return type {ret!r}')

    body = [
        '/*',
        ' * PS5 vkQuake - the Vulkan global commands, which this console has no loader to provide.',
        ' *',
        ' * Copyright (C) 2026 Mihawk',
        ' * SPDX-License-Identifier: GPL-3.0-or-later',
        ' *',
        ' * GENERATED by tools/gen-vk-globals.py. Do not edit: run the generator.',
        ' *',
        ' * See that script for why this exists. In one line: vkQuake calls the global Vulkan',
        ' * commands directly, a desktop links a loader that exports them, ../PS5_Vulkan ships no',
        ' * loader because RetroArch never needed one, so the port supplies the trampolines - each',
        ' * resolving the real command through the driver\'s vkGetInstanceProcAddr on first call.',
        ' *',
        f' * {len(commands)} commands, read out of the header the engine was compiled against.',
        ' */',
        '',
        '#include <stdio.h>',
        '#include <stdatomic.h>',
        '#include <string.h>',
        '#include <vulkan/vulkan_core.h>',
        '',
        '/* The driver\'s own exported entry point. ../PS5_Vulkan defines this one symbol at global',
        ' * scope, and everything below is resolved through it. */',
        'extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,',
        '                                                                     const char *name);',
        '',
        '/* The trace door, so a console run says what the driver answered instead of only',
        ' * how far it got. src/trace.cpp owns it, and it appends to the same file the',
        ' * title writes its build identity to. */',
        'extern void ps5_trace(const char *line);',
        '',
        '/* ---------------------------------------------------------------------------',
        ' * Resolving a command, which is the part the first version of this got wrong.',
        ' *',
        ' * It called vkGetInstanceProcAddr(NULL, name) for all seventy-nine. That is',
        ' * correct for the four true globals and wrong for everything else, and the driver',
        ' * says so plainly:',
        ' *',
        ' *   ps5vk_GetInstanceProcAddr(VkInstance _instance, const char *pName)',
        ' *   {  VK_FROM_HANDLE(ps5vk_instance, instance, _instance);',
        ' *      return vk_instance_get_proc_addr(instance ? &instance->vk : NULL, ...); }',
        ' *',
        ' * With a null instance it returns only vkCreateInstance,',
        ' * vkEnumerateInstanceExtensionProperties, vkEnumerateInstanceLayerProperties and',
        ' * vkEnumerateInstanceVersion. vkEnumeratePhysicalDevices is an instance-level',
        ' * command, so the first run of the title resolved it to NULL and called zero -',
        ' * and the console reported that as VID_Init, which is where the call was made,',
        ' * not where it went.',
        ' *',
        ' * So this is the part of a loader that a statically linked single-ICD title',
        ' * actually needs: remember the instance and the device as they are created, and',
        ' * resolve through whichever of them owns the command. The four globals keep the',
        ' * null instance, because that is what global means.',
        ' * ------------------------------------------------------------------------- */',
        '',
        'static VkInstance ps5_resolved_instance;',
        'static VkDevice ps5_resolved_device;',
        'static PFN_vkGetDeviceProcAddr ps5_get_device_proc_addr;',
        '',
        '/* Names that may only be asked of a null instance. Everything else is asked of',
        ' * the instance first and the device second. */',
        'static int ps5_is_global_command(const char *name)',
        '{',
        '    return strcmp(name, "vkCreateInstance") == 0 ||',
        '           strcmp(name, "vkEnumerateInstanceExtensionProperties") == 0 ||',
        '           strcmp(name, "vkEnumerateInstanceLayerProperties") == 0 ||',
        '           strcmp(name, "vkEnumerateInstanceVersion") == 0;',
        '}',
        '',
        'static PFN_vkVoidFunction ps5_resolve(const char *name)',
        '{',
        '    PFN_vkVoidFunction resolved;',
        '    if (ps5_is_global_command(name))',
        '        return vkGetInstanceProcAddr(NULL, name);',
        '    if (ps5_resolved_instance != VK_NULL_HANDLE)',
        '    {',
        '        resolved = vkGetInstanceProcAddr(ps5_resolved_instance, name);',
        '        if (resolved != NULL)',
        '            return resolved;',
        '    }',
        '    if (ps5_resolved_device != VK_NULL_HANDLE)',
        '    {',
        '        if (ps5_get_device_proc_addr == NULL)',
        '            ps5_get_device_proc_addr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(',
        '                ps5_resolved_instance, "vkGetDeviceProcAddr");',
        '        if (ps5_get_device_proc_addr != NULL)',
        '        {',
        '            resolved = (PFN_vkVoidFunction)ps5_get_device_proc_addr(ps5_resolved_device, name);',
        '            if (resolved != NULL)',
        '                return resolved;',
        '        }',
        '    }',
        '    return vkGetInstanceProcAddr(NULL, name);',
        '}',
        '',
        '/* Which calls report. Every command that returns VkResult logs a failure, because',
        ' * a refused call is what a run needs explained and a success is not - except the',
        ' * three that build the graphics stack, which log both ways, because for those the',
        ' * interesting question is whether they were reached at all. Those three are the',
        ' * spine of M2 and "reached it, and it worked" is the answer a run exists to give. */',
        'static VkResult traced_result(const char *name, VkResult result, int always)',
        '{',
        '    if (always || result != VK_SUCCESS)',
        '    {',
        '        char line[128];',
        '        snprintf(line, sizeof line, "%s -> %d", name, (int)result);',
        '        ps5_trace(line);',
        '    }',
        '    return result;',
        '}',
        '',
    ]

    for command in commands:
        ret, params = signatures[command]
        declarations = ', '.join(f'{t} {n}' if t else n for t, n in
                                 (split_declaration(p) for p in params)) or 'void'
        arguments = ', '.join(n for _, n in (split_declaration(p) for p in params))
        body.append(f'static PFN_{command} ps5_pfn_{command};')
        body.append(f'VKAPI_ATTR {ret} VKAPI_CALL {command}({declarations})')
        body.append('{')
        body.append(f'    if (!ps5_pfn_{command})')
        body.append(f'        ps5_pfn_{command} = (PFN_{command})ps5_resolve("{command}");')
        if ret == 'VkResult':
            always = '1' if command in ALWAYS else '0'
            # The two calls that produce the handles everything else is resolved
            # through. Capturing them here is what makes ps5_resolve able to answer.
            if command == 'vkCreateInstance':
                body.append('    const VkResult instance_result =')
                body.append(f'        traced_result("{command}", ps5_pfn_{command}({arguments}), {always});')
                body.append('    if (instance_result == VK_SUCCESS && pInstance != NULL)')
                body.append('        ps5_resolved_instance = *pInstance;')
                body.append('    return instance_result;')
            elif command == 'vkCreateDevice':
                body.append('    const VkResult device_result =')
                body.append(f'        traced_result("{command}", ps5_pfn_{command}({arguments}), {always});')
                body.append('    if (device_result == VK_SUCCESS && pDevice != NULL)')
                body.append('        ps5_resolved_device = *pDevice;')
                body.append('    return device_result;')
            elif command in {'vkEndCommandBuffer', 'vkQueueSubmit'}:
                body.append('    static atomic_flag reported = ATOMIC_FLAG_INIT;')
                body.append(f'    const VkResult result = ps5_pfn_{command}({arguments});')
                body.append(f'    return traced_result("{command}", result, !atomic_flag_test_and_set(&reported));')
            else:
                body.append(f'    return traced_result("{command}", ps5_pfn_{command}({arguments}), {always});')
        else:
            body.append(f'    ps5_pfn_{command}({arguments});')
        body.append('}')
        body.append('')

    text = '\n'.join(body)
    if check:
        if not OUTPUT.is_file() or OUTPUT.read_text() != text:
            raise SystemExit(f'{OUTPUT} is out of date; run tools/gen-vk-globals.py')
        print(f'{OUTPUT.relative_to(ROOT)} is up to date ({len(commands)} commands)')
        return 0

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(text)
    print(f'wrote {OUTPUT.relative_to(ROOT)}: {len(commands)} global commands')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
