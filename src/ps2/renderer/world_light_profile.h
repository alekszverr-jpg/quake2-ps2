#pragma once
#include "ps2/renderer/timing.h"

namespace ps2::view {
// Diagnostic only: accumulate clock ticks before conversion, so short calls
// do not each lose their fractional microseconds. Disabled scopes read no clock.
struct WorldLightProfile {
    enum Phase { Select, Split, Color, PhaseCount };
    bool enabled = false;
    long long ticks[PhaseCount] = {};
    int surfaces = 0, surfaceTests = 0, bounds = 0, boundsTests = 0;
    int colorHits = 0, colorMisses = 0;
    int rejected = 0, nodes = 0, splits = 0, vertices = 0, vertexTests = 0;
    int Micros(Phase phase) const {
        return static_cast<int>(ticks[phase] * 1000000LL / CLOCKS_PER_SEC);
    }
};
class WorldLightScope {
    WorldLightProfile & stats;
    WorldLightProfile::Phase phase;
    timing::Stamp start = 0;
    bool running;
public:
    WorldLightScope(WorldLightProfile & value, WorldLightProfile::Phase category)
        : stats(value), phase(category), running(value.enabled) {
        if (running) start = timing::Now();
    }
    void Stop() {
        if (running) {
            stats.ticks[phase] += static_cast<long long>(timing::Now() - start);
            running = false;
        }
    }
    ~WorldLightScope() { Stop(); }
};
} // namespace ps2::view
