#include <cassert>
#include <cstdio>
#include "ps2/renderer/model_profile.h"
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
    puts("MD2 exclusive phase timing, submission context and disabled clocks PASS");
}
