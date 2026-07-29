#pragma once
/* ================================================================================================
 * File: timing.h
 * Brief: Tiny wall-clock helpers for the on-screen PS2 renderer profiler.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include <ctime>

namespace ps2::timing {

using Stamp = std::clock_t;

inline Stamp Now()
{
    return std::clock();
}

inline int ElapsedMicros(Stamp start)
{
    const Stamp elapsed = std::clock() - start;
    return static_cast<int>(
        (static_cast<long long>(elapsed) * 1000000LL) / CLOCKS_PER_SEC);
}

} // namespace ps2::timing
