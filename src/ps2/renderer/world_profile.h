#pragma once
#include "ps2/renderer/timing.h"

namespace ps2::view {
struct WorldProfile {
    enum Phase { Visibility, Sky, Geometry, Submit, PhaseCount };
    enum Detail { Preparation, Clip, Textures, Seals, Planes, Emit, DetailCount };
    bool enabled = false;
    bool sampled = false, sampleSelected = false;
    unsigned sampleCursor = 0;
    int sampleDepth = 0, sampleRoots = 0, sampleCount = 0;
    int emptyMicros[4] = {}, emptyCount = 0;
    Phase activePhase = PhaseCount;
    long long ticks[PhaseCount] = {};
    long long nestedSubmit[PhaseCount] = {};
    Detail activeDetail = DetailCount;
    long long detailTicks[DetailCount] = {}, detailChildren[DetailCount] = {};
    int nodes = 0, surfaces = 0, triangles = 0, batches = 0;
    int cachedTriangles = 0, unlitTriangles = 0, earlyRejects = 0, packedInside = 0;
    void RecordCachedTriangle(bool unlit, bool rejected, bool packed = false) {
        if (enabled && activePhase == Geometry) {
            ++cachedTriangles;
            if (unlit) ++unlitTriangles;
            if (rejected) ++earlyRejects;
            if (packed) ++packedInside;
        }
    }
    int Micros(Phase phase) const {
        const long long value = ticks[phase] - nestedSubmit[phase];
        return static_cast<int>((value > 0 ? value : 0) * 1000000LL / CLOCKS_PER_SEC);
    }
    int DetailMicros(Detail detail) const {
        const long long value = detailTicks[detail] - detailChildren[detail];
        return static_cast<int>((value > 0 ? value : 0) * 1000000LL / CLOCKS_PER_SEC);
    }
};
// Select whole root operations; recursive clipping/lighting inherits selection.
// The renderer rotates the initial cursor between frames.
class WorldSampleScope {
    WorldProfile & stats;
    bool running;
public:
    explicit WorldSampleScope(WorldProfile & value)
        : stats(value), running(value.enabled && value.sampled &&
                                value.activePhase == WorldProfile::Geometry) {
        if (running && stats.sampleDepth++ == 0) {
            ++stats.sampleRoots;
            stats.sampleSelected = (stats.sampleCursor++ & 31u) == 0;
            if (stats.sampleSelected) ++stats.sampleCount;
        }
    }
    ~WorldSampleScope() {
        if (running && --stats.sampleDepth == 0) stats.sampleSelected = false;
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
          running(value.enabled && value.activePhase == WorldProfile::Geometry &&
                  (!value.sampled || (value.sampleDepth > 0 && value.sampleSelected))) {
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
// Reference topology: selected root -> Clip -> Planes (explicit Stop), Emit.
// No geometry, submission or recursive lighting. This is a reference, not a
// universal correction: real roots may enter more scopes or flush batches.
inline void CalibrateWorldTimers(WorldProfile & target) {
    if (!target.enabled || !target.sampled) return;
    WorldProfile empty;
    empty.enabled = empty.sampled = true;
    empty.activePhase = WorldProfile::Geometry;
    for (int i = 0; i < 64; ++i) {
        empty.sampleCursor = 0;
        WorldSampleScope root(empty);
        WorldDetailScope clip(empty, WorldProfile::Clip);
        WorldDetailScope planes(empty, WorldProfile::Planes);
        planes.Stop();
        { WorldDetailScope emit(empty, WorldProfile::Emit); }
    }
    target.emptyMicros[0] = empty.DetailMicros(WorldProfile::Planes);
    target.emptyMicros[1] = empty.DetailMicros(WorldProfile::Emit);
    target.emptyMicros[2] = empty.DetailMicros(WorldProfile::Clip);
    target.emptyMicros[3] = target.emptyMicros[0] + target.emptyMicros[1] + target.emptyMicros[2];
    target.emptyCount = empty.sampleCount;
}
} // namespace ps2::view
