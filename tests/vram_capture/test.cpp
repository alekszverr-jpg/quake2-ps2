#include "ps2/common.h"
#include "ps2/renderer/vram_capture.h"
#include <cstdio>
#include <cstring>
static int panelRows=0;
void DrawInternalString(int x,int y,const char * text) {
    assert(x>=0 && x+static_cast<int>(std::strlen(text))*8<=320);
    assert(y>=0 && y+8<=224); ++panelRows;
}
namespace ps2::gs {
void FillRect(int x,int y,int w,int h,u8,u8,u8,u8) {
    assert(x>=0 && y>=0 && x+w<=320 && y+h<=224);
}
}
static cvar_t request={1};
static const cvar_t * s_captureRequest=&request;
static ps2::vram::Capture s_capture;
static bool s_captureEligible=false,s_captureRunning=false,s_captureReady=false;
static int s_capturePreviousMs=0,now=0;
static ps2::vram::Stats stats={};
static ps2::gs::TimingStats waits={};
int Sys_Milliseconds() { return now; }
void Cvar_SetValue(const char *,float value) { request.value=value; }
namespace ps2::vram { Stats GetStats() { return stats; } }
namespace ps2::gs { const TimingStats & GetTimingStats() { return waits; } }
#include "collect.inc"
#include "panel.inc"
int main() {
    stats.uploadsThisFrame=4; stats.reloadsThisFrame=3;
    stats.evictionsThisFrame=3; stats.sameFrameEvictions=1;
    stats.uploadsByType[4]={4,3,32768}; waits.textureUploadMicros=1000;
    waits.vramStallMicros=2000;
    CollectVramCapture(); assert(request.value==2 && !s_captureRunning);
    now=5000; s_captureEligible=true; CollectVramCapture();
    assert(s_captureRunning && s_capture.frames==0);
    for(int i=0;i<100;++i) { now+=100; CollectVramCapture(); }
    assert(s_captureReady && s_capture.frames==100 && s_capture.elapsedMs==10000);
    assert(s_capture.maxMs==100 && s_capture.over33==100 && s_capture.over50==100);
    assert(s_capture.types[4].bytes==3276800 && s_capture.types[4].reloads==300);
    assert(s_capture.uploadMicros==100000 && s_capture.stallMicros==200000);
    assert(s_capture.worstReloads==3 && s_capture.worstBytes==32768);
    now+=100; CollectVramCapture(); assert(s_capture.frames==100); // frozen
    DrawVramCaptureResult(); assert(panelRows==16);
    s_captureEligible=false; DrawVramCaptureResult(); assert(panelRows==16);
    s_captureEligible=true;
    request.value=1; CollectVramCapture(); assert(!s_captureReady && s_capture.frames==0);
    now+=20; CollectVramCapture(); assert(s_capture.frames==1);
    s_captureEligible=false; now+=4000; CollectVramCapture();
    assert(!s_captureRunning && s_capture.frames==0);
    s_captureEligible=true; CollectVramCapture();
    now+=2000; CollectVramCapture(); // retain severe hitches
    assert(s_capture.maxMs==2000 && s_capture.frames==1);
    request.value=0; CollectVramCapture(); assert(!s_captureRunning && !s_captureReady);
    // Natural32-bit timer wrap preserves a positive end-to-end interval.
    request.value=1; now=-10; CollectVramCapture(); now=10; CollectVramCapture();
    assert(s_capture.elapsedMs==20);
    std::puts("VRAM capture totals/worst-frame/freeze/rearm/menu interruption/long hitch/timer wrap PASS");
}
