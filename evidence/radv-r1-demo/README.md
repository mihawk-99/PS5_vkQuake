# radv-r1-demo: vkQuake on RADV, the demo loop

The title built with `PS5_VULKAN_DRIVER=radv` and RADV_ARCHIVE set to the PS5
Mesa fork's release archive at 7e30f3e (ps5-port with the VideoOut swapchain),
deployed over PPSA99010 on 2026-09-27 and run for 120 s from launch; the ps5vk
eboot.bin was restored byte for byte afterwards. `trace.txt` is the title's
trace from its build identity on.

- The device: "PlayStation 5 GPU (RADV NAVI21)", radv Mesa 26.2.0
  (git-7e30f3e7e9), VK_KHR_display on VideoOut, FIFO, ray queries on.
- First present 3.28 s after start; the pipelines took 0.69 s.
- The demo loop from demo1: 119.88 fps in every steady window, 8.3 ms of work
  a frame (7.7 to 9.0 ms), one vblank a frame; the high-frame-rate mode was
  taken because the title declares it.
- Hitches: the level loads (903 ms, 107 ms), as with ps5vk.
