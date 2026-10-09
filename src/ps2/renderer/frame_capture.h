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
    int pendingReads=0, worstReads=0;
    long long reads=0, pendingIO=0, io=0, worstIO=0;
    int renderPart=PS2_RENDER_VIEW;
    long long pendingRender[PS2_RENDER_PARTS]={}, render[PS2_RENDER_PARTS]={}, worstRender[PS2_RENDER_PARTS]={};
    long long aux[5]={}, worstAux[5]={}; // setup, VU wait, reuse, upload DMA, 2D submission (us)
    bool valid=true;
    void Switch(int next, std::uint32_t now) {
        const std::uint32_t dt=now-stamp;
        if (dt>0x7fffffffu) valid=false;
        else {
            pending[phase]+=dt;
            if(phase==PS2_FRAME_RENDER) pendingRender[renderPart]+=dt;
        }
        stamp=now; phase=next;
    }
    void RenderPart(int next, std::uint32_t now) { Switch(phase,now); renderPart=next; }
    void Boundary(std::uint32_t now) { Switch(phase,now); }
    void Discard(std::uint32_t now) {
        stamp=now; valid=true; pendingReads=0; pendingIO=0;
        for (auto & value:pending) value=0;
        for (auto & value:pendingRender) value=0;
    }
    void Add(int ms, const int micros[3], const int auxiliary[5]) {
        if (ms<=0 || !valid) return;
        reads+=pendingReads; io+=pendingIO;
        ++frames; elapsedMs+=ms; over33+=ms>33; over50+=ms>50;
        for(int i=0;i<PS2_FRAME_PHASES;++i) total[i]+=pending[i];
        for(int i=0;i<3;++i) draw[i]+=micros[i];
        for(int i=0;i<PS2_RENDER_PARTS;++i) render[i]+=pendingRender[i];
        for(int i=0;i<5;++i) aux[i]+=auxiliary[i];
        if(ms>maxMs) {
            maxMs=ms; worstReads=pendingReads; worstIO=pendingIO;
            for(int i=0;i<PS2_FRAME_PHASES;++i) worst[i]=pending[i];
            for(int i=0;i<3;++i) worstDraw[i]=micros[i];
            for(int i=0;i<PS2_RENDER_PARTS;++i) worstRender[i]=pendingRender[i];
            for(int i=0;i<5;++i) worstAux[i]=auxiliary[i];
        }
    }
};
}
