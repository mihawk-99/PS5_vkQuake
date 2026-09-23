/* PS5 vkQuake - report a slow host frame's phases.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Single Player > New Game shows frames of 92 and 155 ms in which the driver
 * created, compiled and allocated nothing and no file was read (port evidence
 * m6-r43-newgame). platform/ps5/vkquake-edits.py marks seven points of
 * _Host_Frame; a frame over 40 ms gets one line with where its time went. */
#include <cstdio>
#include <cstring>

extern "C" void ps5_trace(const char *line);

namespace
{
/* This host frame's parse time and count per server command (cl_parse.c); a
 * fast update is 128 and up. Written and read on the main thread only. */
double svc_seconds[256];
unsigned svc_count[256];
} // namespace

extern "C" void ps5_svc_time(int cmd, double seconds)
{
    const unsigned index = static_cast<unsigned>(cmd) & 255u;
    svc_seconds[index] += seconds;
    ++svc_count[index];
}

extern "C" void ps5_host_frame_split(const double *mark, int count)
{
    if (count < 7)
        return;
    const double total = (mark[6] - mark[0]) * 1000.0;
    static unsigned lines;
    if (total < 40.0 || lines >= 200)
    {
        std::memset(svc_seconds, 0, sizeof svc_seconds);
        std::memset(svc_count, 0, sizeof svc_count);
        return;
    }
    ++lines;
    char line[512];
    int used = std::snprintf(
        line, sizeof line,
        "PS5 slow host frame: total_ms=%.3f input_ms=%.3f server_ms=%.3f client_ms=%.3f "
        "screen_ms=%.3f particles_ms=%.3f sound_ms=%.3f",
        total, (mark[1] - mark[0]) * 1000.0, (mark[2] - mark[1]) * 1000.0,
        (mark[3] - mark[2]) * 1000.0, (mark[4] - mark[3]) * 1000.0, (mark[5] - mark[4]) * 1000.0,
        (mark[6] - mark[5]) * 1000.0);
    /* The three costliest server commands of the frame, as cmd#count:ms. */
    for (int rank = 0; rank < 3 && used > 0 && static_cast<size_t>(used) < sizeof line; ++rank)
    {
        int best = -1;
        for (int cmd = 0; cmd < 256; ++cmd)
            if (svc_count[cmd] != 0 && (best < 0 || svc_seconds[cmd] > svc_seconds[best]))
                best = cmd;
        if (best < 0)
            break;
        used +=
            std::snprintf(line + used, sizeof line - static_cast<size_t>(used), " svc%d#%u:%.3fms",
                          best, svc_count[best], svc_seconds[best] * 1000.0);
        svc_count[best] = 0;
    }
    ps5_trace(line);
    std::memset(svc_seconds, 0, sizeof svc_seconds);
    std::memset(svc_count, 0, sizeof svc_count);
}
