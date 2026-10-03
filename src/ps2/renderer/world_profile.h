#pragma once
#include "ps2/renderer/timing.h"

namespace ps2::view {
struct WorldProfile {
    enum Phase { Visibility, Sky, Geometry, Submit, PhaseCount };
    bool enabled = false;
    Phase activePhase = PhaseCount;
    long long ticks[PhaseCount] = {};
    long long nestedSubmit[PhaseCount] = {};
    int nodes = 0, surfaces = 0, triangles = 0, batches = 0;
    int Micros(Phase phase) const {
        const long long value = ticks[phase] - nestedSubmit[phase];
        return static_cast<int>((value > 0 ? value : 0) * 1000000LL / CLOCKS_PER_SEC);
    }
};
class WorldScope {
    WorldProfile & stats;
    timing::Stamp start = 0;
    bool running;
public:
    WorldScope(WorldProfile & value, WorldProfile::Phase category)
        : stats(value), running(value.enabled) {
        if (running) { stats.activePhase = category; start = timing::Now(); }
    }
    void Switch(WorldProfile::Phase next) {
        if (running) {
            const auto now = timing::Now();
            stats.ticks[stats.activePhase] += static_cast<long long>(now - start);
            stats.activePhase = next;
            start = now;
        }
    }
    void Stop() {
        if (running) {
            stats.ticks[stats.activePhase] += static_cast<long long>(timing::Now() - start);
            stats.activePhase = WorldProfile::PhaseCount;
            running = false;
        }
    }
    ~WorldScope() { Stop(); }
};
// Subtract nested CPU submission from its parent phase before conversion.
// Deferred VU/GS work is not timed by this scope.
class WorldSubmitScope {
    WorldProfile & stats;
    WorldProfile::Phase parent;
    timing::Stamp start = 0;
    bool running;
public:
    explicit WorldSubmitScope(WorldProfile & value)
        : stats(value), parent(value.activePhase),
          running(value.enabled && parent != WorldProfile::PhaseCount) {
        if (running) { start = timing::Now(); ++stats.batches; }
    }
    ~WorldSubmitScope() {
        if (running) {
            const auto elapsed = static_cast<long long>(timing::Now() - start);
            stats.ticks[WorldProfile::Submit] += elapsed;
            stats.nestedSubmit[parent] += elapsed;
        }
    }
};
} // namespace ps2::view
