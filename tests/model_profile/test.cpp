#include <cassert>
#include <cstdio>
#include "ps2/renderer/model_profile.h"
#include "ps2/renderer/world_profile.h"
namespace math { struct Mat4 {}; }
namespace tex { struct Texture {}; }
namespace vu1 {
static int calls;
void DrawTriangles(const math::Mat4 &, const tex::Texture &, int *, int, bool, int) {
    ++calls;
    ps2::timing::now += 30;
}
}
using namespace ps2::view;
static ModelProfile s_modelProfile;
static WorldProfile s_worldProfile;
static bool s_modelSubmitActive;
static int s_scratchVertCount;
static int s_scratchVerts[12];
#define PS2_STAT_INC(x) ((void)0)
#include "flush.inc"
int main() {
    math::Mat4 matrix;
    tex::Texture texture;
    // Normal draws and empty flushes read no profiling clocks.
    s_scratchVertCount = 3;
    FlushScratch(matrix, texture);
    assert(vu1::calls == 1 && ps2::timing::reads == 0);
    s_modelProfile.enabled = true;
    FlushScratch(matrix, texture);
    assert(ps2::timing::reads == 0);
    ps2::timing::now = 0;
    {
        ModelSubmitContext context(s_modelSubmitActive, true);
        ModelScope timer(s_modelProfile, ModelProfile::Setup);
        ps2::timing::now = 10;
        timer.Switch(ModelProfile::Lighting);
        ps2::timing::now = 30;
        timer.Switch(ModelProfile::Vertices);
        ps2::timing::now = 60;
        timer.Switch(ModelProfile::Triangles);
        s_scratchVertCount = 3;
        FlushScratch(matrix, texture); // nested 30 ticks inside triangle phase
        ps2::timing::now += 40;
        timer.Stop(); timer.Stop();
    }
    assert(!s_modelSubmitActive && s_modelProfile.batches == 1);
    assert(s_modelProfile.Micros(ModelProfile::Setup) == 10);
    assert(s_modelProfile.Micros(ModelProfile::Lighting) == 20);
    assert(s_modelProfile.Micros(ModelProfile::Vertices) == 30);
    assert(s_modelProfile.Micros(ModelProfile::Triangles) == 40);
    assert(s_modelProfile.Micros(ModelProfile::Submit) == 30);
    int reads = ps2::timing::reads;
    s_scratchVertCount = 3;
    FlushScratch(matrix, texture); // outside alias context, e.g. world/particles
    assert(ps2::timing::reads == reads && s_modelProfile.batches == 1);
    s_modelProfile = {};
    {
        ModelSubmitContext context(s_modelSubmitActive, s_modelProfile.enabled);
        ModelScope timer(s_modelProfile, ModelProfile::Setup);
        timer.Switch(ModelProfile::Vertices);
        s_scratchVertCount = 3;
        FlushScratch(matrix, texture);
    }
    assert(ps2::timing::reads == reads && !s_modelSubmitActive);
    assert(s_modelProfile.Micros(ModelProfile::Submit) == 0);
    ps2::timing::now = 0;
    s_worldProfile.enabled = true;
    {
        WorldScope timer(s_worldProfile, WorldProfile::Visibility);
        ps2::timing::now += 10;
        timer.Switch(WorldProfile::Sky);
        ps2::timing::now += 20;
        s_scratchVertCount = 3;
        FlushScratch(matrix, texture);
        timer.Switch(WorldProfile::Geometry);
        ps2::timing::now += 40;
        s_scratchVertCount = 3;
        FlushScratch(matrix, texture);
        timer.Stop(); timer.Stop();
    }
    assert(s_worldProfile.Micros(WorldProfile::Visibility) == 10);
    assert(s_worldProfile.Micros(WorldProfile::Sky) == 20);
    assert(s_worldProfile.Micros(WorldProfile::Geometry) == 40);
    assert(s_worldProfile.Micros(WorldProfile::Submit) == 60);
    assert(s_worldProfile.batches == 2);
    reads = ps2::timing::reads;
    s_scratchVertCount = 3;
    FlushScratch(matrix, texture); // entities/late alpha outside world context
    assert(ps2::timing::reads == reads && s_worldProfile.batches == 2);
    s_worldProfile = {};
    {
        WorldScope timer(s_worldProfile, WorldProfile::Visibility);
        timer.Switch(WorldProfile::Geometry);
        s_scratchVertCount = 3;
        FlushScratch(matrix, texture);
    }
    assert(ps2::timing::reads == reads && s_worldProfile.batches == 0);
    // Recursive detail categories must be exclusive even when a category
    // recurs through another one, and submission must be subtracted once.
    s_worldProfile.enabled = true;
    ps2::timing::now = 0;
    {
        WorldScope geometry(s_worldProfile, WorldProfile::Geometry);
        WorldDetailScope prep(s_worldProfile, WorldProfile::Preparation);
        ps2::timing::now += 10;
        {
            WorldDetailScope textures(s_worldProfile, WorldProfile::Textures);
            ps2::timing::now += 20;
            textures.Stop(); textures.Stop();
        }
        {
            WorldDetailScope clip(s_worldProfile, WorldProfile::Clip);
            ps2::timing::now += 40;
            {
                WorldDetailScope dynamicLight(s_worldProfile, WorldProfile::Preparation);
                ps2::timing::now += 50;
                {
                    WorldDetailScope nestedLight(s_worldProfile, WorldProfile::Preparation);
                    ps2::timing::now += 60;
                    WorldDetailScope leafClip(s_worldProfile, WorldProfile::Clip);
                    ps2::timing::now += 70;
                    s_scratchVertCount = 3;
                    FlushScratch(matrix, texture);
                }
            }
        }
        {
            WorldDetailScope seals(s_worldProfile, WorldProfile::Seals);
            ps2::timing::now += 80;
        }
    }
    assert(s_worldProfile.DetailMicros(WorldProfile::Preparation) == 120);
    assert(s_worldProfile.DetailMicros(WorldProfile::Textures) == 20);
    assert(s_worldProfile.DetailMicros(WorldProfile::Clip) == 110);
    assert(s_worldProfile.DetailMicros(WorldProfile::Seals) == 80);
    assert(s_worldProfile.Micros(WorldProfile::Geometry) == 330);
    assert(s_worldProfile.Micros(WorldProfile::Submit) == 30);
    assert(s_worldProfile.activeDetail == WorldProfile::DetailCount);
    reads = ps2::timing::reads;
    { WorldDetailScope outside(s_worldProfile, WorldProfile::Clip); }
    assert(ps2::timing::reads == reads);
    s_worldProfile.RecordCachedTriangle(true,true);
    assert(s_worldProfile.cachedTriangles==0 && s_worldProfile.earlyRejects==0);
    s_worldProfile = {};
    s_worldProfile.RecordCachedTriangle(true,true);
    assert(s_worldProfile.cachedTriangles==0 && s_worldProfile.earlyRejects==0);
    {
        WorldScope disabled(s_worldProfile, WorldProfile::Geometry);
        WorldDetailScope detail(s_worldProfile, WorldProfile::Preparation);
    }
    assert(ps2::timing::reads == reads);
    puts("MD2/World exclusive phase timing, submission context and disabled clocks PASS");
    s_worldProfile = {}; s_worldProfile.enabled = true;
    {
        WorldScope geometry(s_worldProfile, WorldProfile::Geometry);
        WorldDetailScope clip(s_worldProfile, WorldProfile::Clip);
        { WorldDetailScope planes(s_worldProfile, WorldProfile::Planes); ps2::timing::now += 10; }
        {
            WorldDetailScope emit(s_worldProfile, WorldProfile::Emit);
            ps2::timing::now += 20;
            s_scratchVertCount = 3; FlushScratch(matrix, texture);
        }
        ps2::timing::now += 40;
    }
    assert(s_worldProfile.DetailMicros(WorldProfile::Planes)==10);
    assert(s_worldProfile.DetailMicros(WorldProfile::Emit)==20);
    assert(s_worldProfile.DetailMicros(WorldProfile::Clip)==40);
    assert(s_worldProfile.Micros(WorldProfile::Geometry)==70);
    assert(s_worldProfile.Micros(WorldProfile::Submit)==30);
    puts("BSP planes/emission/rest exclude nested submission PASS");
    s_worldProfile = {}; s_worldProfile.enabled = true; s_worldProfile.sampled = true;
    s_worldProfile.activePhase = WorldProfile::Geometry;
    reads = ps2::timing::reads;
    for (int i=0;i<64;++i) {
        WorldSampleScope root(s_worldProfile);
        WorldDetailScope clip(s_worldProfile, WorldProfile::Clip);
        { WorldDetailScope planes(s_worldProfile, WorldProfile::Planes); ps2::timing::now+=10; }
        { WorldDetailScope emit(s_worldProfile, WorldProfile::Emit); ps2::timing::now+=20; }
        {
            WorldSampleScope child(s_worldProfile);
            assert(s_worldProfile.sampleSelected==(i%32==0));
            WorldDetailScope light(s_worldProfile, WorldProfile::Preparation);
            ps2::timing::now+=4;
        }
        ps2::timing::now+=1;
        s_worldProfile.RecordCachedTriangle(true,false,true);
    }
    assert(s_worldProfile.sampleRoots==64 && s_worldProfile.sampleCount==2);
    assert(s_worldProfile.cachedTriangles==64 && s_worldProfile.packedInside==64);
    assert(s_worldProfile.sampleDepth==0 && !s_worldProfile.sampleSelected);
    assert(ps2::timing::reads-reads==16);
    assert(s_worldProfile.DetailMicros(WorldProfile::Planes)==20);
    assert(s_worldProfile.DetailMicros(WorldProfile::Emit)==40);
    assert(s_worldProfile.DetailMicros(WorldProfile::Preparation)==8);
    assert(s_worldProfile.DetailMicros(WorldProfile::Clip)==2);
    int chosen[64]={};
    for (unsigned offset=0;offset<32;++offset) {
        s_worldProfile = {}; s_worldProfile.enabled=true; s_worldProfile.sampled=true;
        s_worldProfile.activePhase=WorldProfile::Geometry; s_worldProfile.sampleCursor=offset;
        for (int i=0;i<64;++i) {
            WorldSampleScope root(s_worldProfile);
            if (s_worldProfile.sampleSelected) ++chosen[i];
        }
        assert(s_worldProfile.sampleCount==2);
    }
    for (int count:chosen) assert(count==1);
    puts("1/32 rotating root samples, nested selection, full counters and skipped clocks PASS");
    const auto before = s_worldProfile;
    reads = ps2::timing::reads;
    ps2::timing::readCost = 1;
    CalibrateWorldTimers(s_worldProfile);
    ps2::timing::readCost = 0;
    assert(ps2::timing::reads-reads == 64*6);
    assert(s_worldProfile.emptyCount == 64);
    assert(s_worldProfile.emptyMicros[0] == 64 && s_worldProfile.emptyMicros[1] == 64);
    assert(s_worldProfile.emptyMicros[2] == 192 && s_worldProfile.emptyMicros[3] == 320);
    assert(s_worldProfile.sampleCursor == before.sampleCursor);
    assert(s_worldProfile.sampleRoots == before.sampleRoots && s_worldProfile.sampleCount == before.sampleCount);
    assert(s_worldProfile.activePhase == before.activePhase && s_worldProfile.activeDetail == before.activeDetail);
    for (int i=0;i<WorldProfile::DetailCount;++i) {
        assert(s_worldProfile.detailTicks[i] == before.detailTicks[i]);
        assert(s_worldProfile.detailChildren[i] == before.detailChildren[i]);
    }
    for (int i=0;i<WorldProfile::PhaseCount;++i) assert(s_worldProfile.ticks[i] == before.ticks[i]);
    s_worldProfile.enabled = false;
    reads = ps2::timing::reads;
    CalibrateWorldTimers(s_worldProfile);
    assert(reads == ps2::timing::reads);
    s_worldProfile.enabled = true; s_worldProfile.sampled = false;
    CalibrateWorldTimers(s_worldProfile);
    assert(reads == ps2::timing::reads);
    puts("Empty timer reference accounts for clock cost without modifying real statistics PASS");
    int planeChosen[64]={}, emitChosen[64]={};
    for (unsigned offset=0;offset<64;++offset) {
        s_worldProfile = {}; s_worldProfile.enabled = s_worldProfile.sampled = s_worldProfile.singleDetails = true;
        s_worldProfile.activePhase=WorldProfile::Geometry; s_worldProfile.sampleCursor=offset;
        reads = ps2::timing::reads;
        for (int i=0;i<64;++i) {
            WorldSampleScope root(s_worldProfile);
            if (s_worldProfile.sampleSelected) {
                if (s_worldProfile.selectedDetail==WorldProfile::Planes) ++planeChosen[i];
                else ++emitChosen[i];
            }
            WorldDetailScope clip(s_worldProfile, WorldProfile::Clip);
            { WorldDetailScope planes(s_worldProfile, WorldProfile::Planes); ps2::timing::now+=10; }
            {
                WorldSampleScope child(s_worldProfile);
                WorldDetailScope light(s_worldProfile, WorldProfile::Preparation);
                WorldDetailScope emit(s_worldProfile, WorldProfile::Emit);
                // Repeated nested same-category scope must not read clocks.
                WorldDetailScope duplicate(s_worldProfile, WorldProfile::Emit);
                ps2::timing::now+=20;
            }
            s_worldProfile.RecordCachedTriangle(true,false,true);
        }
        assert(s_worldProfile.planeSamples==1 && s_worldProfile.emitSamples==1);
        assert(s_worldProfile.sampleCount==2 && s_worldProfile.sampleRoots==64);
        assert(s_worldProfile.cachedTriangles==64 && s_worldProfile.packedInside==64);
        assert(ps2::timing::reads-reads==4);
        assert(s_worldProfile.DetailMicros(WorldProfile::Planes)==10);
        assert(s_worldProfile.DetailMicros(WorldProfile::Emit)==20);
        assert(s_worldProfile.DetailMicros(WorldProfile::Clip)==0);
        assert(s_worldProfile.DetailMicros(WorldProfile::Preparation)==0);
    }
    for (int i=0;i<64;++i) assert(planeChosen[i]==1 && emitChosen[i]==1);
    reads=ps2::timing::reads; ps2::timing::readCost=1;
    CalibrateWorldTimers(s_worldProfile);
    ps2::timing::readCost=0;
    assert(ps2::timing::reads-reads==64*4);
    assert(s_worldProfile.emptyCount==64);
    assert(s_worldProfile.emptyMicros[0]==64 && s_worldProfile.emptyMicros[1]==64);
    assert(s_worldProfile.emptyMicros[2]==0);
    puts("Separate categories: balanced rotating roots, no nested clocks, full counts and empty reference PASS");
}
