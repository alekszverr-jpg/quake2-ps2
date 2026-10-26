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
    int scenePart=PS2_SCENE_OTHER;
    long long pendingScene[PS2_SCENE_PARTS]={}, scene[PS2_SCENE_PARTS]={}, worstScene[PS2_SCENE_PARTS]={};
    int pendingCounts[3]={}, worstCounts[3]={};
    long long counts[3]={};
    // A slow View need not belong to the longest full frame. Keep one coherent
    // View snapshot rather than combining independent per-stage maxima.
    long long peakView=0, peakViewScene[PS2_SCENE_PARTS]={};
    int peakViewFrameMs=0, peakViewCounts[3]={};
    int objectPart=PS2_OBJECT_BASE;
    long long pendingObject[PS2_OBJECT_PARTS]={}, object[PS2_OBJECT_PARTS]={}, worstObject[PS2_OBJECT_PARTS]={};
    ps2_fx_counters_t pendingFX={}, fx={}, worstFX={};
    double longestSum=0;
    bool valid=true;
    void Switch(int next, std::uint32_t now) {
        const std::uint32_t dt=now-stamp;
        if (dt>0x7fffffffu) valid=false;
        else {
            pending[phase]+=dt;
            if(phase==PS2_FRAME_RENDER) {
                pendingRender[renderPart]+=dt;
                if(renderPart==PS2_RENDER_VIEW) {
                    pendingScene[scenePart]+=dt;
                    if(scenePart==PS2_SCENE_OBJECTS) pendingObject[objectPart]+=dt;
                }
            }
        }
        stamp=now; phase=next;
    }
    void RenderPart(int next, std::uint32_t now) { Switch(phase,now); renderPart=next; }
    void ScenePart(int next, std::uint32_t now) { Switch(phase,now); scenePart=next; }
    void ObjectPart(int next,std::uint32_t now) { Switch(phase,now); objectPart=next; }
    void Boundary(std::uint32_t now) { Switch(phase,now); }
    void Discard(std::uint32_t now) {
        stamp=now; valid=true; pendingReads=0; pendingIO=0;
        for (auto & value:pending) value=0;
        for (auto & value:pendingRender) value=0;
        for (auto & value:pendingScene) value=0;
        for (auto & value:pendingCounts) value=0;
        for (auto & value:pendingObject) value=0;
        pendingFX={};
    }
    void Add(int ms, const int micros[3], const int auxiliary[5]) {
        if (ms<=0 || !valid) return;
        reads+=pendingReads; io+=pendingIO;
        fx.effects+=pendingFX.effects; fx.passes+=pendingFX.passes; fx.steps+=pendingFX.steps;
        fx.particles+=pendingFX.particles; fx.exhausted+=pendingFX.exhausted; fx.length+=pendingFX.length;
        longestSum+=static_cast<double>(pendingFX.longest);
        for(int i=0;i<PS2_OBJECT_PARTS;++i) object[i]+=pendingObject[i];
        ++frames; elapsedMs+=ms; over33+=ms>33; over50+=ms>50;
        for(int i=0;i<PS2_FRAME_PHASES;++i) total[i]+=pending[i];
        for(int i=0;i<3;++i) draw[i]+=micros[i];
        for(int i=0;i<PS2_RENDER_PARTS;++i) render[i]+=pendingRender[i];
        for(int i=0;i<5;++i) aux[i]+=auxiliary[i];
        for(int i=0;i<PS2_SCENE_PARTS;++i) scene[i]+=pendingScene[i];
        for(int i=0;i<3;++i) counts[i]+=pendingCounts[i];
        if(pendingRender[PS2_RENDER_VIEW]>peakView) {
            peakView=pendingRender[PS2_RENDER_VIEW];
            peakViewFrameMs=ms;
            for(int i=0;i<PS2_SCENE_PARTS;++i) peakViewScene[i]=pendingScene[i];
            for(int i=0;i<3;++i) peakViewCounts[i]=pendingCounts[i];
        }
        if(ms>maxMs) {
            worstFX=pendingFX;
            for(int i=0;i<PS2_OBJECT_PARTS;++i) worstObject[i]=pendingObject[i];
            maxMs=ms; worstReads=pendingReads; worstIO=pendingIO;
            for(int i=0;i<PS2_FRAME_PHASES;++i) worst[i]=pending[i];
            for(int i=0;i<3;++i) worstDraw[i]=micros[i];
            for(int i=0;i<PS2_RENDER_PARTS;++i) worstRender[i]=pendingRender[i];
            for(int i=0;i<5;++i) worstAux[i]=auxiliary[i];
            for(int i=0;i<PS2_SCENE_PARTS;++i) worstScene[i]=pendingScene[i];
            for(int i=0;i<3;++i) worstCounts[i]=pendingCounts[i];
        }
    }
};
}
