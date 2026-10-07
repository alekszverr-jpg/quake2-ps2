#pragma once
#include "ps2/frame_capture.h"
#include <cstdint>
namespace ps2::frame {
// Fixed storage. Phase ticks use the same wall-clock source on PS2 as timing.h.
// A boundary splits the active phase, so post-display audio belongs to the
// next display interval. No allocations, per-surface timers or logging.
struct Capture {
    int frames=0, elapsedMs=0, maxMs=0, over33=0, over50=0;
    int phase=PS2_FRAME_OTHER;
    std::uint32_t stamp=0;
    long long pending[PS2_FRAME_PHASES]={}, total[PS2_FRAME_PHASES]={};
    long long worst[PS2_FRAME_PHASES]={};
    long long draw[3]={}, worstDraw[3]={};
    bool valid=true;
    void Switch(int next, std::uint32_t now) {
        const std::uint32_t dt=now-stamp;
        if (dt>0x7fffffffu) valid=false;
        else pending[phase]+=dt;
        stamp=now; phase=next;
    }
    void Boundary(std::uint32_t now) { Switch(phase,now); }
    void Discard(std::uint32_t now) {
        stamp=now; valid=true;
        for (auto & value:pending) value=0;
    }
    void Add(int ms, const int micros[3]) {
        if (ms<=0 || !valid) return;
        ++frames; elapsedMs+=ms; over33+=ms>33; over50+=ms>50;
        for(int i=0;i<PS2_FRAME_PHASES;++i) total[i]+=pending[i];
        for(int i=0;i<3;++i) draw[i]+=micros[i];
        if(ms>maxMs) {
            maxMs=ms;
            for(int i=0;i<PS2_FRAME_PHASES;++i) worst[i]=pending[i];
            for(int i=0;i<3;++i) worstDraw[i]=micros[i];
        }
    }
};
}
