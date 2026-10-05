#pragma once
#include "ps2/renderer/vram.h"
#include "ps2/renderer/gs.h"

namespace ps2::vram {
// Fixed storage; collected after EndFrame, before any results panel is drawn.
struct Capture {
    int frames=0, elapsedMs=0, maxMs=0, over33=0, over50=0;
    long long uploads=0, reloads=0, evictions=0, sameFrame=0;
    long long uploadMicros=0, stallMicros=0;
    int worstReloads=0, worstBytes=0, worstStallMicros=0;
    struct Type { long long images=0, reloads=0, bytes=0; } types[6];
    void Add(int frameMs, const Stats & stats, const gs::TimingStats & timing) {
        if (frameMs<=0) return;
        ++frames; elapsedMs+=frameMs;
        over33+=frameMs>33; over50+=frameMs>50;
        uploads+=stats.uploadsThisFrame; reloads+=stats.reloadsThisFrame;
        evictions+=stats.evictionsThisFrame; sameFrame+=stats.sameFrameEvictions;
        uploadMicros+=timing.textureUploadMicros; stallMicros+=timing.vramStallMicros;
        int bytes=0;
        for (int i=0;i<6;++i) {
            types[i].images+=stats.uploadsByType[i].images;
            types[i].reloads+=stats.uploadsByType[i].reloads;
            types[i].bytes+=stats.uploadsByType[i].bytes;
            bytes+=stats.uploadsByType[i].bytes;
        }
        if (frameMs>maxMs) {
            maxMs=frameMs; worstReloads=stats.reloadsThisFrame;
            worstBytes=bytes; worstStallMicros=timing.vramStallMicros;
        }
    }
};
}
