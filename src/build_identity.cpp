/*
 * PS5 vkQuake - the build identity, in the binary and in the trace.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this file exists. A console run leaves two records: the kernel's log, which
 * says a title started and stopped, and /app0/trace.txt, which says what the title
 * did. Neither says *which build* did it. That matters more here than on a
 * desktop, because a title is deployed once and run many times: two runs an hour
 * apart can be two different binaries, and the only way to tell is to have the
 * binary say so.
 *
 * tools/build-title.sh hashes the sources, the port layer, the build scripts and
 * the driver archives into one 64-character identity and writes it to
 * build/title_build_identity.h. This file puts that string into the program - once
 * as a constructor that appends it to the trace before main runs, and once simply
 * by being referenced, which is what keeps it in the image at all.
 *
 * The header is generated during the title build rather than the engine build, and
 * this file is compiled with src/ for that reason: the identity covers the engine,
 * so the engine cannot also contain the identity without the two depending on each
 * other. src/ is compiled afterwards, from sources the identity has already been
 * computed over.
 *
 * tools/deploy-title.py reads the string back out of the deployed eboot.bin and
 * refuses to publish a title whose identity cannot be found, once, NUL-terminated
 * - so a run on the console can be tied to the sources that produced it.
 */

#include "trace.hpp"

#include "../build/title_build_identity.h"

namespace
{
/* A constructor, so the line is written before main and therefore before anything
 * that could fail. The trace facility opens, writes and closes on every call and
 * ignores every error, so this cannot stop the title from starting. */
class BuildIdentity
{
  public:
    BuildIdentity() noexcept
    {
        ps5::debug::mark(PS5_VKQUAKE_BUILD_ID);
    }
};

const BuildIdentity build_identity;
} // namespace
