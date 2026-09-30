#include "ps2/renderer/view_effects.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

using u32 = std::uint32_t;
using u8 = std::uint8_t;
namespace effects = ps2::view::effects;
namespace math {
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
struct Mat4 {};
void LerpTo(Vec4 & out, const Vec4 & a, const Vec4 & b, float t)
{
    out = { a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t,
            a.z + (b.z-a.z)*t, a.w + (b.w-a.w)*t };
}
}
namespace tex { struct Texture {}; }
struct ClipVertex { math::Vec4 pos, st, color; };
struct Light { float intensity, color[3]; };
static Light lights[32];
static const Light * s_worldLights = lights;
static int s_worldLightCount = 0;
static math::Vec3 s_worldLightOrigins[32];
static u32 s_surfaceLightMask = 0;
static int triangles, brightVertices;
struct cvar_t { float value; };
static cvar_t polyBlend = {1};
const cvar_t * Cvar_Get(const char *, const char *, int) { return &polyBlend; }
constexpr int RDF_NOWORLDMODEL = 1;
struct refdef_t { int x, y, width, height, rdflags; float blend[4]; };
namespace gs {
static int fills, rect[4];
static u8 color[4];
void FillRect(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a)
{
    ++fills;
    rect[0] = x; rect[1] = y; rect[2] = w; rect[3] = h;
    color[0] = r; color[1] = g; color[2] = b; color[3] = a;
}
}
u32 AddWorldLights(u32 color, const math::Vec4 & point);
void SetClipDistances(ClipVertex &, const math::Mat4 &) {}
void SubmitWorldTriangle(const ClipVertex (&corners)[3], const math::Mat4 &,
                         const tex::Texture &)
{
    ++triangles;
    for (const auto & corner : corners)
        if (AddWorldLights(0x80404040u, corner.pos) != 0x80404040u) ++brightVertices;
}
#include "effects.inc"

int main()
{
    float x = 90.0f, y = 75.0f;
    effects::UnderwaterFov(0.0f, x, y);
    assert(x == 90.0f && y == 75.0f);
    effects::UnderwaterFov(0.39269908f, x, y);
    assert(std::fabs(x - 91.0f) < 0.001f && std::fabs(y - 74.0f) < 0.001f);
    const float yellow[3] = { 1, 1, 0 };
    assert(effects::AddLight(0x80404040u, 40000, 200, yellow) == 0x80404040u);
    assert(effects::AddLight(0x80404040u, 0, 200, yellow) == 0x8040A4A4u);
    assert(effects::AddLight(0x10404040u, 0, 1000, yellow) == 0x1040FFFFu);
    assert(effects::AddLight(0x80404040u, 0, -10, yellow) == 0x80404040u);

    const float start[3] = { 2, 3, 4 };
    const float ends[][3] = { {102,3,4}, {2,103,4}, {2,3,104}, {102,203,304} };
    float points[12][3];
    assert(!effects::BeamRing(start, start, 4, points));
    assert(!effects::BeamRing(start, ends[0], 0, points));
    for (const auto & end : ends)
    {
        assert(effects::BeamRing(start, end, 4, points));
        for (int side = 0; side < 6; ++side)
        {
            float radiusSquared = 0, dot = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float offset = points[side][axis] - start[axis];
                radiusSquared += offset * offset;
                dot += offset * (end[axis] - start[axis]);
                assert(std::fabs((points[side+6][axis]-points[side][axis]) -
                                 (end[axis]-start[axis])) < 0.0001f);
            }
            assert(std::fabs(radiusSquared - 4) < 0.0001f);
            assert(std::fabs(dot) < 0.001f);
        }
    }

    // All original vertices lie outside this flash. Transient subdivision
    // must illuminate interior vertices without losing/restamping the mask.
    s_worldLightCount = 32;
    lights[31] = { 100, {1,1,0} };
    s_worldLightOrigins[31] = { 0,0,10 };
    s_surfaceLightMask = 1u << 31;
    const ClipVertex large[3] = {
        {{-512,-512,0,1}, {}, {64,64,64,128}},
        {{512,-512,0,1}, {}, {64,64,64,128}},
        {{0,512,0,1}, {}, {64,64,64,128}}
    };
    SubmitDynamicallyLitTriangle(large, {}, {});
    assert(triangles > 1 && triangles <= 128 && brightVertices > 0);
    assert(s_surfaceLightMask == (1u << 31));
    triangles = brightVertices = 0;
    s_worldLightOrigins[31] = {10000,0,10};
    SubmitDynamicallyLitTriangle(large, {}, {});
    assert(triangles == 1 && brightVertices == 0);
    assert(s_surfaceLightMask == (1u << 31));
    s_surfaceLightMask = 0;
    assert(AddWorldLights(0x80404040u, {}) == 0x80404040u);

    // Use the stock water blend and a reduced viewport: tint only the 3D view.
    refdef_t view = { 10,20,320,240,0, {0.5f,0.3f,0.2f,0.4f} };
    RenderViewBlend(view);
    assert(gs::fills == 1 && gs::rect[0] == 10 && gs::rect[1] == 20);
    assert(gs::rect[2] == 320 && gs::rect[3] == 240);
    assert(gs::color[0] == 128 && gs::color[1] == 77 && gs::color[2] == 51 && gs::color[3] == 102);
    view.blend[3] = 0;
    RenderViewBlend(view);
    view.blend[3] = 0.5f; polyBlend.value = 0;
    RenderViewBlend(view);
    polyBlend.value = 1; view.rdflags = RDF_NOWORLDMODEL;
    RenderViewBlend(view);
    assert(gs::fills == 1);
    assert(ViewBlendByte(-0.1f) == 0 && ViewBlendByte(1.5f) == 255);
    std::puts("View effects: projection, beam geometry, RGB falloff and bounded dynamic tessellation passed");
}
