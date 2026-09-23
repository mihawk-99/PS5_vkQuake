/* PS5 vkQuake - hand termination to the shell after engine shutdown.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "memory_diagnostics.hpp"
#include <cstdio>
#include <unistd.h>

extern "C" int sceSystemServiceLoadExec(const char *path, const char *const *argv);

// The sibling template proved this exit path. Kernel exit() faults for titles;
// the shell closes asynchronously, so even a successful request must not return.
extern "C" [[noreturn]] void ps5_title_exit(int status)
{
    std::printf("PS5 exit: status=%d; requesting shell close\n", status);
    ps5::memory::finish();
    std::fflush(nullptr);
    const int result = sceSystemServiceLoadExec("exit", nullptr);
    std::printf("PS5 exit: shell request result=0x%08x\n", static_cast<unsigned>(result));
    std::fflush(nullptr);
    for (;;)
        usleep(100000);
}
