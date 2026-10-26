#include "ps2/renderer/frame_capture.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
struct cvar_t { float value; } request={1};
static const cvar_t * s_frameRequest=&request;
static cvar_t detailPage={0};
static const cvar_t * s_frameDetailPage=&detailPage;
static ps2::frame::Capture s_frameCapture;
static bool s_frameRunning=false,s_frameReady=false,s_frameSaved=false,s_captureEligible=false;
static int s_framePreviousMs=0, now=0;
static std::uint32_t tick=0;
static const char * s_frameProfiles[]={"ps2_profile_world","ps2_profile_models","ps2_profile_world_lights"};
static float s_frameProfileValues[3]={}, profiles[3]={1,2,3};
std::uint32_t FrameStamp() { return tick; }
int Sys_Milliseconds() { return now; }
float Cvar_VariableValue(const char * name) {
    for(int i=0;i<3;++i) if(!std::strcmp(name,s_frameProfiles[i])) return profiles[i];
    assert(false); return 0;
}
void Cvar_SetValue(const char * name,float value) {
    if(!std::strcmp(name,"ps2_frame_capture")) { request.value=value; return; }
    for(int i=0;i<3;++i) if(!std::strcmp(name,s_frameProfiles[i])) { profiles[i]=value; return; }
    assert(false);
}
static int rows=0;
static std::vector<std::string> printed;
void DrawInternalString(int x,int y,const char * text) {
    assert(x>=0 && x+static_cast<int>(std::strlen(text))*8<=320);
    assert(y>=0 && y+8<=224); ++rows; printed.emplace_back(text);
}
namespace ps2::gs { void FillRect(int x,int y,int w,int h,int,int,int,int) { assert(x+w<=320 && y+h<=224); } }
namespace ps2::gs {
struct TimingStats { int vramStallMicros=20,textureUploadMicros=30,overlaySubmitMicros=40; } timing;
const TimingStats & GetTimingStats() { return timing; }
}
namespace ps2::vu1 {
struct TimingStats { int waitMicros=50; } timing;
const TimingStats & GetTimingStats() { return timing; }
}
namespace ps2::view {
struct DrawStats { int worldMicros=800,entityMicros=400,particleMicros=100,setupMicros=100; } draw;
const DrawStats & GetDrawStats() { return draw; }
}
#include "production.inc"
void advance(int ms) { now+=ms; tick+=static_cast<std::uint32_t>(ms)*1000u; }
void frame(int other) {
    // Audio after the preceding display boundary is charged to this interval.
    advance(2); PS2_FramePhase(PS2_FRAME_OTHER);
    advance(other); PS2_FramePhase(PS2_FRAME_SERVER);
    advance(3); PS2_FramePhase(PS2_FRAME_CLIENT);
    advance(2); PS2_FrameRenderPart(PS2_RENDER_BEGIN); PS2_FramePhase(PS2_FRAME_RENDER);
    advance(1); PS2_FrameRenderPart(PS2_RENDER_VIEW);
    PS2_FrameScenePart(PS2_SCENE_CAMERA);
    tick+=400; PS2_FrameScenePart(PS2_SCENE_OBJECTS);
    tick+=600; ++now; PS2_FrameScenePart(PS2_SCENE_OTHER);
    PS2_FrameSceneCounts(12,34,5);
    PS2_FrameRenderPart(PS2_RENDER_3D);
    advance(2); PS2_FrameRenderPart(PS2_RENDER_HUD);
    advance(1); PS2_FramePhase(PS2_FRAME_FINISH);
    advance(1); PS2_FramePhase(PS2_FRAME_PRESENT);
    advance(7); PS2_FramePhase(PS2_FRAME_RENDER);
    PS2_FramePhase(PS2_FRAME_CLIENT); CollectFrameCapture();
}
int main() {
    ps2::frame::Capture partition;
    partition.Switch(PS2_FRAME_RENDER,0); partition.ScenePart(PS2_SCENE_OBJECTS,0);
    partition.ObjectPart(PS2_OBJECT_AUTO_FX,10); partition.ObjectPart(PS2_OBJECT_BASE,40);
    partition.ScenePart(PS2_SCENE_OTHER,60);
    assert(partition.pendingObject[0]==30 && partition.pendingObject[1]==30);
    assert(partition.pendingScene[PS2_SCENE_OBJECTS]==60);

    // The longest full frame (100ms) has only1ms of View. A shorter full
    // frame carries the26ms View spike; all stages/counts must come from it.
    ps2::frame::Capture viewPeak;
    const int noDraw[3]={}, noAux[5]={};
    viewPeak.pendingRender[PS2_RENDER_VIEW]=1000;
    viewPeak.pendingScene[PS2_SCENE_CAMERA]=1000;
    viewPeak.pendingCounts[0]=7;
    viewPeak.Add(100,noDraw,noAux);
    viewPeak.Discard(0);
    viewPeak.pendingRender[PS2_RENDER_VIEW]=26000;
    viewPeak.pendingScene[PS2_SCENE_OBJECTS]=25000;
    viewPeak.pendingScene[PS2_SCENE_SORT]=1000;
    viewPeak.pendingCounts[0]=66; viewPeak.pendingCounts[1]=952;
    viewPeak.Add(67,noDraw,noAux);
    assert(viewPeak.maxMs==100 && viewPeak.worstCounts[0]==7);
    assert(viewPeak.peakView==26000 && viewPeak.peakViewFrameMs==67);
    assert(viewPeak.peakViewScene[PS2_SCENE_OBJECTS]==25000);
    assert(viewPeak.peakViewScene[PS2_SCENE_CAMERA]==0);
    assert(viewPeak.peakViewCounts[0]==66 && viewPeak.peakViewCounts[1]==952);
    // A later stage can exceed its old value without exceeding total View;
    // it must not leak into the retained peak snapshot. Ties keep the first.
    viewPeak.Discard(0);
    viewPeak.pendingRender[PS2_RENDER_VIEW]=26000;
    viewPeak.pendingScene[PS2_SCENE_EFFECTS]=26000;
    viewPeak.pendingCounts[0]=99;
    viewPeak.Add(80,noDraw,noAux);
    assert(viewPeak.peakViewScene[PS2_SCENE_EFFECTS]==0);
    assert(viewPeak.peakViewFrameMs==67 && viewPeak.peakViewCounts[0]==66);
    const auto retainedPeak=viewPeak;
    viewPeak.valid=false; viewPeak.pendingRender[PS2_RENDER_VIEW]=99000;
    viewPeak.Add(100,noDraw,noAux);
    assert(viewPeak.peakView==26000);
    viewPeak.valid=true; viewPeak.Add(0,noDraw,noAux);
    assert(viewPeak.peakView==26000);
    viewPeak={}; assert(viewPeak.peakView==0 && viewPeak.peakViewFrameMs==0);

    detailPage.value=3;
    s_frameCapture.Switch(PS2_FRAME_RENDER,0); s_frameCapture.ScenePart(PS2_SCENE_OBJECTS,0);
    assert(PS2_FrameObjectProfile() && PS2_FrameFXCounters()!=nullptr);
    PS2_FrameFXCounters()->particles=17;
    PS2_FrameObjectPart(PS2_OBJECT_AUTO_FX); assert(s_frameCapture.pendingFX.effects==1);
    s_frameCapture.ScenePart(PS2_SCENE_OTHER,0); assert(PS2_FrameFXCounters()==nullptr);
    s_frameReady=true; assert(!PS2_FrameObjectProfile()); s_frameReady=false;
    detailPage.value=0;
    PrepareFrameCapture(); assert(request.value==2 && s_frameSaved);
    for(auto p:profiles) assert(p==0);
    s_captureEligible=true; PS2_FramePhase(PS2_FRAME_CLIENT); CollectFrameCapture();
    frame(0); assert(s_frameCapture.elapsedMs==20 && s_frameCapture.frames==1);
    assert(s_frameCapture.total[PS2_FRAME_CLIENT]==4000 && s_frameCapture.total[PS2_FRAME_SERVER]==3000);
    assert(s_frameCapture.total[PS2_FRAME_RENDER]==5000 && s_frameCapture.total[PS2_FRAME_FINISH]==1000 && s_frameCapture.total[PS2_FRAME_PRESENT]==7000);
    long long renderSum=0; for(auto value:s_frameCapture.render) renderSum+=value;
    assert(renderSum==s_frameCapture.total[PS2_FRAME_RENDER]);
    assert(s_frameCapture.render[PS2_RENDER_BEGIN]==1000 && s_frameCapture.render[PS2_RENDER_3D]==2000);
    long long sceneSum=0; for(auto value:s_frameCapture.scene) sceneSum+=value;
    assert(sceneSum==s_frameCapture.render[PS2_RENDER_VIEW]);
    assert(s_frameCapture.scene[PS2_SCENE_CAMERA]==400 && s_frameCapture.scene[PS2_SCENE_OBJECTS]==600);
    assert(s_frameCapture.counts[0]==12 && s_frameCapture.worstCounts[1]==34);
    assert(s_frameCapture.aux[0]==100 && s_frameCapture.aux[4]==40);
    PS2_FrameSoundIO(60000); frame(80); assert(s_frameCapture.maxMs==100 && s_frameCapture.worst[PS2_FRAME_OTHER]==80000);
    assert(s_frameCapture.worstReads==1 && s_frameCapture.worstIO==60000);
    ps2::gs::timing.overlaySubmitMicros=900;
    frame(0); assert(s_frameCapture.worstAux[4]==40);
    assert(s_frameCapture.worst[PS2_FRAME_OTHER]==80000); // same worst frame, not per-phase maxima
    PS2_VramCaptureEligible(0); // skipped loading screen has no EndFrame
    assert(!s_frameRunning && s_frameCapture.frames==0);
    assert(s_frameCapture.peakView==0 && s_frameCapture.peakViewCounts[0]==0);
    advance(4000); CollectFrameCapture();
    assert(!s_frameRunning && s_frameCapture.frames==0);
    s_captureEligible=true; CollectFrameCapture();
    for(int i=0;i<500;++i) frame(0);
    assert(s_frameReady && s_frameCapture.frames==500 && s_frameCapture.elapsedMs==10000);
    assert(!s_frameSaved && profiles[0]==1 && profiles[1]==2 && profiles[2]==3);
    const auto total=s_frameCapture.total[PS2_FRAME_CLIENT]; frame(100); assert(total==s_frameCapture.total[PS2_FRAME_CLIENT]);
    DrawFrameCaptureResult(); assert(rows==20);
    detailPage.value=1; DrawFrameCaptureResult(); assert(rows==41);
    detailPage.value=2; DrawFrameCaptureResult(); assert(rows==61);
    detailPage.value=3; DrawFrameCaptureResult(); assert(rows==80);
    s_captureEligible=false; DrawFrameCaptureResult(); assert(rows==80);
    request.value=1; PrepareFrameCapture(); assert(!s_frameReady && profiles[0]==0);
    request.value=0; PrepareFrameCapture(); assert(profiles[0]==1 && !s_frameSaved);
    // Unsigned tick and millisecond wrap keep real positive intervals.
    request.value=1; now=-10; tick=0xfffff000u; s_captureEligible=true;
    PrepareFrameCapture(); PS2_FramePhase(PS2_FRAME_CLIENT); CollectFrameCapture();
    frame(0); assert(s_frameCapture.elapsedMs==20 && s_frameCapture.total[PS2_FRAME_CLIENT]==4000);
    // Backward clocks discard the incomplete window.
    now-=100; tick-=100000u; CollectFrameCapture(); assert(s_frameCapture.frames==0);
    // Check actual rendered values, not only the result-panel row count.
    s_frameCapture=retainedPeak;
    // Simulated partition ticks above are microseconds; panel conversion must
    // use the host's CLOCKS_PER_SEC (1000 on Windows, 1000000 on Linux).
    s_frameCapture.peakView=26*static_cast<long long>(CLOCKS_PER_SEC)/1000;
    for(auto & value:s_frameCapture.peakViewScene)
        value=value*static_cast<long long>(CLOCKS_PER_SEC)/1000000;
    printed.clear(); rows=0; DrawSceneCaptureResult(); assert(rows==20);
    bool frameLabel=false, totalLabel=false, objectLabel=false, countLabel=false;
    for(const auto & line:printed) {
        frameLabel|=line=="Peak View full frame: 67 ms";
        totalLabel|=line.find("View total")==0 && line.find("26.00")!=std::string::npos;
        objectLabel|=line.find("Objects/FX")==0 && line.find("25.00")!=std::string::npos;
        countLabel|=line.find("Entities")==0 && line.find("/ 66")!=std::string::npos;
    }
    assert(frameLabel && totalLabel && objectLabel && countLabel);
    std::puts("Coherent slowest-View snapshot independent of longest full frame PASS");
    std::puts("Full-frame exclusive phases/audio boundary/worst-frame/freeze/interruption/profile restore/wrap PASS");
}
