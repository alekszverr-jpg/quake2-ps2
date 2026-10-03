#pragma once
#include "ps2/renderer/timing.h"

namespace ps2::view {
// EE diagnostic phases only. Submit is nested in Triangles; subtract its ticks
// before conversion so the displayed phases do not count submission twice.
struct ModelProfile {
    enum Phase { Setup, Lighting, Vertices, Triangles, Submit, PhaseCount };
    bool enabled = false;
    long long ticks[PhaseCount] = {};
    int models = 0, culled = 0, vertices = 0, triangles = 0, batches = 0;
    int Micros(Phase phase) const {
        long long value = ticks[phase];
        if (phase == Triangles) value -= ticks[Submit];
        return static_cast<int>((value > 0 ? value : 0) * 1000000LL / CLOCKS_PER_SEC);
    }
};
class ModelScope {
    ModelProfile & stats;
    ModelProfile::Phase phase;
    timing::Stamp start = 0;
    bool running;
public:
    ModelScope(ModelProfile & value, ModelProfile::Phase category, bool active = true)
        : stats(value), phase(category), running(value.enabled && active) {
        if (running) start = timing::Now();
    }
    void Switch(ModelProfile::Phase next) {
        if (running) {
            const auto now = timing::Now();
            stats.ticks[phase] += static_cast<long long>(now - start);
            start = now;
        }
        phase = next;
    }
    void Stop() {
        if (running) {
            stats.ticks[phase] += static_cast<long long>(timing::Now() - start);
            running = false;
        }
    }
    ~ModelScope() { Stop(); }
};
// Restore context on every exit, including the early frustum-culling return.
class ModelSubmitContext {
    bool & active;
    bool previous;
public:
    ModelSubmitContext(bool & flag, bool enabled) : active(flag), previous(flag) { active = enabled; }
    ~ModelSubmitContext() { active = previous; }
};
} // namespace ps2::view
