#pragma once
#include "ps2/renderer/timing.h"

namespace ps2::view {
struct WorldProfile {
    enum Phase { Visibility, Sky, Geometry, Submit, PhaseCount };
    enum Detail { Preparation, Clip, Textures, Seals, DetailCount };
    bool enabled = false;
    Phase activePhase = PhaseCount;
    long long ticks[PhaseCount] = {};
    long long nestedSubmit[PhaseCount] = {};
    Detail activeDetail = DetailCount;
    long long detailTicks[DetailCount] = {}, detailChildren[DetailCount] = {};
    int nodes = 0, surfaces = 0, triangles = 0, batches = 0;
    int Micros(Phase phase) const {
        const long long value = ticks[phase] - nestedSubmit[phase];
        return static_cast<int>((value > 0 ? value : 0) * 1000000LL / CLOCKS_PER_SEC);
    }
    int DetailMicros(Detail detail) const {
        const long long value = detailTicks[detail] - detailChildren[detail];
        return static_cast<int>((value > 0 ? value : 0) * 1000000LL / CLOCKS_PER_SEC);
    }
};
// Exclusive nested categories inside Geometry only. Recursive scopes, including
// repeated categories, subtract their whole duration from the immediate parent.
class WorldDetailScope {
    WorldProfile & stats;
    WorldProfile::Detail category, parent;
    timing::Stamp start = 0;
    bool running;
public:
    WorldDetailScope(WorldProfile & value, WorldProfile::Detail detail)
        : stats(value), category(detail), parent(value.activeDetail),
          running(value.enabled && value.activePhase == WorldProfile::Geometry) {
        if (running) { stats.activeDetail = category; start = timing::Now(); }
    }
    void Stop() {
        if (running) {
            const auto elapsed = static_cast<long long>(timing::Now() - start);
            stats.detailTicks[category] += elapsed;
            if (parent != WorldProfile::DetailCount) stats.detailChildren[parent] += elapsed;
            stats.activeDetail = parent;
            running = false;
        }
    }
    ~WorldDetailScope() { Stop(); }
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
    WorldProfile::Detail detail;
    timing::Stamp start = 0;
    bool running;
public:
    explicit WorldSubmitScope(WorldProfile & value)
        : stats(value), parent(value.activePhase), detail(value.activeDetail),
          running(value.enabled && parent != WorldProfile::PhaseCount) {
        if (running) { start = timing::Now(); ++stats.batches; }
    }
    ~WorldSubmitScope() {
        if (running) {
            const auto elapsed = static_cast<long long>(timing::Now() - start);
            stats.ticks[WorldProfile::Submit] += elapsed;
            stats.nestedSubmit[parent] += elapsed;
            if (detail != WorldProfile::DetailCount) stats.detailChildren[detail] += elapsed;
        }
    }
};
} // namespace ps2::view
