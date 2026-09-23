"""Exercise the real loader hooks: first results and every error, no frame spam."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class FrameTrace(unittest.TestCase):
    def test_first_frame_and_errors(self):
        source = r'''
#include <assert.h>
#include "platform/ps5/ps5_window.c"
#include "platform/ps5/vk_loader.c"
#include "platform/ps5/vk_globals.c"
static int lines, calls;
static Uint64 ticks;
Uint64 SDL_GetTicks64(void) { return ticks; }
Uint64 SDL_GetPerformanceCounter(void) { return ticks * 1000000ull; }
int ps5_sdl_counts_frame(char *l, size_t n, int h) { return h ? snprintf(l, n, " frame") : 0; }
void ps5_memory_syscalls(unsigned long long *m, unsigned long long *u) { *m = 0; *u = 0; }
void ps5_startup_report(void) {}
void ps5_file_counts(unsigned long long *o, unsigned long long *r, unsigned long long *b, unsigned long long *t) { *o = *r = *b = *t = 0; }
int ps5_sdl_counts_format(char *l, size_t n, unsigned long long f) { (void)f; return snprintf(l, n, " counted"); }
static VkResult answer;
static char last[512];
void ps5_trace(const char *line) { ++lines; snprintf(last, sizeof last, "%s", line); }
static VkResult VKAPI_CALL present(VkQueue q, const VkPresentInfoKHR *p)
{ (void)q; (void)p; ++calls; return answer; }
static VkResult VKAPI_CALL end(VkCommandBuffer c)
{ (void)c; ++calls; return answer; }
static VkResult VKAPI_CALL submit(VkQueue q, uint32_t n, const VkSubmitInfo *p, VkFence f)
{ (void)q; (void)n; (void)p; (void)f; ++calls; return answer; }
static PFN_vkVoidFunction VKAPI_CALL device_lookup(VkDevice d, const char *name)
{ (void)d; return strcmp(name, "vkQueuePresentKHR") == 0 ? (PFN_vkVoidFunction)present : NULL; }
PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance i, const char *name)
{
    (void)i;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)device_lookup;
    if (!strcmp(name, "vkEndCommandBuffer")) return (PFN_vkVoidFunction)end;
    if (!strcmp(name, "vkQueueSubmit")) return (PFN_vkVoidFunction)submit;
    return NULL;
}
int main(void)
{
    PFN_vkGetInstanceProcAddr lookup = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    PFN_vkGetDeviceProcAddr device = (PFN_vkGetDeviceProcAddr)lookup(NULL, "vkGetDeviceProcAddr");
    PFN_vkQueuePresentKHR draw = (PFN_vkQueuePresentKHR)device(NULL, "vkQueuePresentKHR");
    assert(device(NULL, "missing") == NULL);
    for (int i = 0; i < 2; ++i) {
        assert(vkEndCommandBuffer(NULL) == VK_SUCCESS);
        assert(vkQueueSubmit(NULL, 0, NULL, NULL) == VK_SUCCESS);
        assert(draw(NULL, NULL) == VK_SUCCESS);
    }
    assert(calls == 6 && lines == 3);
    assert(!strcmp(last, "vkQueuePresentKHR -> 0"));
    answer = VK_ERROR_UNKNOWN;
    for (int i = 0; i < 2; ++i) {
        assert(vkEndCommandBuffer(NULL) == answer);
        assert(vkQueueSubmit(NULL, 0, NULL, NULL) == answer);
        assert(draw(NULL, NULL) == answer);
    }
    assert(calls == 12 && lines == 9);
    assert(!strcmp(last, "vkQueuePresentKHR -> -13"));
    answer = VK_SUCCESS;
    ticks = 9999;
    assert(draw(NULL, NULL) == VK_SUCCESS && lines == 9);
    ticks = 10000;
    assert(draw(NULL, NULL) == VK_SUCCESS && lines == 10);
    assert(strstr(last, "frames=4 interval=10000 ms fps=0.30 work_ms="));
    assert(strstr(last, " vblanks=") && strstr(last, " counted"));
    assert(draw(NULL, NULL) == VK_SUCCESS && lines == 10);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frame_trace.c"
            binary = Path(directory) / "frame_trace"
            path.write_text(source)
            subprocess.run([
                'cc', '-std=gnu11', '-O1', '-Wall', '-Wextra', '-Werror',
                '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                '-I.', '-Ivendor/vkQuake/Windows/misc/include', str(path), '-o', str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True, timeout=30)
