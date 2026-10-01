#include "ps2/renderer/view_effects.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <array>
#include "ps2/renderer/world_light_profile.h"
using ps2::view::WorldLightProfile;
using ps2::view::WorldLightScope;
static WorldLightProfile s_lightProfile;
#include "ps2/renderer/world_light_cache.h"
using ps2::view::WorldLightCache;
static WorldLightCache s_worldLightCache;
static bool referenceMode;

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
constexpr int RF_SHELL_RED = 1024, RF_SHELL_GREEN = 2048, RF_SHELL_BLUE = 4096;
constexpr int RF_SHELL_DOUBLE = 65536, RF_SHELL_HALF_DAM = 131072;
constexpr float POWERSUIT_SCALE = 4.0f;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
constexpr float kAliasNormals[][3] = {
    #include "client/anorms.h"
};
#pragma GCC diagnostic pop
constexpr int SURF_FLOWING = 64;
namespace mod {
struct PolyVertex { math::Vec3 position; float texture_s, texture_t; };
struct ModelTriangle { int vertexes[3]; };
struct ModelPoly { int numVerts; const ModelPoly * next; PolyVertex * vertexes; ModelTriangle * triangles; };
struct ModelTexInfo { int flags; };
struct ModelSurface { const ModelTexInfo * texInfo; const ModelPoly * polys; };
}
static float s_viewTime;
static tex::Texture surfaceTexture;
const tex::Texture * TextureAnimation(const mod::ModelTexInfo *, int) { return &surfaceTexture; }
struct ClipVertex { math::Vec4 pos, st, color; };
struct Light { float intensity, color[3]; };
static Light lights[32];
static const Light * s_worldLights = lights;
static int s_worldLightCount = 0;
static math::Vec3 s_worldLightOrigins[32];
static u32 s_surfaceLightMask = 0;
static int triangles, brightVertices;
static float lastS, lastT;
static int lastAlpha;
static std::vector<std::array<u32,13>> emitted;
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
u32 DirectWorldLights(u32 color, const math::Vec4 & position)
{
    for (int i = 0; i < 32; ++i)
        if (s_surfaceLightMask & (1u << i)) {
            const auto & origin = s_worldLightOrigins[i];
            const float x = position.x-origin.x, y = position.y-origin.y, z = position.z-origin.z;
            color = effects::AddLight(color,x*x+y*y+z*z,lights[i].intensity,lights[i].color);
        }
    return color;
}
u32 TestWorldLights(u32 color, const math::Vec4 & point)
{ return referenceMode ? DirectWorldLights(color,point) : AddWorldLights(color,point); }
void SetClipDistances(ClipVertex &, const math::Mat4 &) {}
void SubmitWorldTriangle(const ClipVertex (&corners)[3], const math::Mat4 &,
                         const tex::Texture &, bool = false, int alpha = -1)
{
    ++triangles;
    lastS = corners[0].st.x; lastT = corners[0].st.y; lastAlpha = alpha;
    for (const auto & corner : corners)
    {
        if (TestWorldLights(0x80404040u, corner.pos) != 0x80404040u) ++brightVertices;
        std::array<u32,13> record{};
        const float values[] = { corner.pos.x,corner.pos.y,corner.pos.z,corner.pos.w,
            corner.st.x,corner.st.y,corner.st.z,corner.st.w,
            corner.color.x,corner.color.y,corner.color.z,corner.color.w };
        std::memcpy(record.data(),values,sizeof(values));
        record[12] = TestWorldLights(0x80404040u,corner.pos);
        emitted.push_back(record);
    }
}
void FlushScratch(const math::Mat4 &, const tex::Texture &, bool, int) {}
#include "effects.inc"
#include "dynamic_reference.inc"

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
    s_worldLightCache.Clear();
    s_worldLightOrigins[31] = {10000,0,10};
    SubmitDynamicallyLitTriangle(large, {}, {});
    assert(triangles == 1 && brightVertices == 0);
    assert(s_surfaceLightMask == (1u << 31));
    s_surfaceLightMask = 0;
    assert(AddWorldLights(0x80404040u, {}) == 0x80404040u);

    // Differential test against the actual Alpha.93 recursive path: emitted
    // position, UV, static colour, rounded light contribution and order match.
    for (int scenario = 0; scenario < 160; ++scenario)
    {
        s_worldLightCache.Clear();
        s_lightProfile = {};
        s_lightProfile.enabled = scenario % 2 != 0;
        for (int i = 0; i < 32; ++i)
        {
            lights[i] = {float(40+(i*37+scenario*19)%320), {.8f,.5f,.2f}};
            s_worldLightOrigins[i] = { float((i*127+scenario*31)%1600-800),
                float((i*73+scenario*47)%1600-800), float((i*23)%180) };
        }
        const u32 mask = scenario % 4 == 0 ? 0u : scenario % 4 == 1 ? 0x80000000u :
                         scenario % 4 == 2 ? 0x80000001u : 0xffffffffu;
        s_surfaceLightMask = mask;
        const math::Vec4 positions[3] = { large[0].pos,large[1].pos,large[2].pos };
        const u32 selected = SelectTriangleLights(mask, positions);
        assert((selected & ~mask) == 0);
        emitted.clear(); referenceMode = true; ReferenceDynamicTriangle(large,{},{});
        referenceMode = false;
        const auto reference = emitted;
        assert(s_surfaceLightMask == mask);
        emitted.clear(); SubmitDynamicallyLitTriangle(large,{},{});
        assert(s_surfaceLightMask == mask && emitted == reference);
        // Filtering the parent source triangle before cached subdivision
        // cannot change the output of the transient tree.
        s_surfaceLightMask = selected;
        emitted.clear(); SubmitDynamicallyLitTriangle(large,{},{});
        assert(emitted == reference && s_surfaceLightMask == selected);
    }
    s_surfaceLightMask = 0;
    s_lightProfile = {};
    AddWorldLights(0x80404040u, {});
    assert(s_lightProfile.vertices == 0 && s_lightProfile.ticks[WorldLightProfile::Color] == 0);
    s_worldLightCache.Clear();
    s_lightProfile.enabled = true;
    lights[0] = {200, {1,0,0}}; lights[31] = lights[0];
    s_worldLightOrigins[0] = {0,0,0}; s_worldLightOrigins[31] = {10000,0,0};
    const math::Vec4 probe[3] = {{0,0,0,1},{1,0,0,1},{0,1,0,1}};
    assert(SelectTriangleLights(0x80000001u,probe) == 1u);
    assert(s_lightProfile.bounds == 1 && s_lightProfile.boundsTests == 2 && s_lightProfile.rejected == 1);
    s_surfaceLightMask = 0x80000001u;
    AddWorldLights(0x80404040u, {});
    assert(s_lightProfile.vertices == 1 && s_lightProfile.vertexTests == 2);
    AddWorldLights(0x80404040u, {});
    assert(s_lightProfile.colorHits == 1 && s_lightProfile.colorMisses == 1);
    assert(s_lightProfile.vertexTests == 2);
    s_worldLightCache.Clear();
    s_lightProfile = {}; s_lightProfile.enabled = true;
    lights[0].intensity = 1000000; s_surfaceLightMask = 1;
    triangles = 0;
    SubmitDynamicallyLitTriangle(large,{},{});
    assert(s_lightProfile.splits > 0 && s_lightProfile.nodes == 2*s_lightProfile.splits+1);
    assert(triangles == s_lightProfile.splits+1);
    WorldLightScope stopped(s_lightProfile,WorldLightProfile::Select);
    stopped.Stop();
    const auto stoppedTicks = s_lightProfile.ticks[WorldLightProfile::Select];
    stopped.Stop();
    assert(s_lightProfile.ticks[WorldLightProfile::Select] == stoppedTicks);
    s_lightProfile = {}; s_surfaceLightMask = 0;
    std::puts("160 Alpha.93 differential light/subdivision scenarios passed");

    // Cache keys must distinguish base RGBA, masks, positions and epochs;
    // the independent scalar path never consults the production cache.
    s_lightProfile.enabled = true;
    for (int batch = 0; batch < 20; ++batch)
    {
        s_worldLightCache.Clear();
        for (int i = 0; i < 32; ++i) {
            lights[i] = {float(80+i*9), {0.4f,-0.2f,0.6f}};
            s_worldLightOrigins[i] = {float(i*13-batch*7), float(i*3), float(batch*2)};
        }
        for (int v = 0; v < 200; ++v) {
            const math::Vec4 point = {float(v%23)*0.25f,float(v%17)*1.25f,float(v%7),1};
            s_surfaceLightMask = v%3 == 0 ? 0xffffffffu : v%3 == 1 ? 0x80000001u : 0u;
            const u32 base = 0x40000000u + static_cast<u32>(v)*0x10101u;
            const u32 expected = DirectWorldLights(base,point);
            assert(AddWorldLights(base,point) == expected);
            assert(AddWorldLights(base,point) == expected); // repeat is a hit
            assert(AddWorldLights(base^0x80000000u,point) == DirectWorldLights(base^0x80000000u,point));
        }
    }
    WorldLightCache collisionCache;
    const auto oldKey = collisionCache.MakeKey(1,2,3,0x80404040u,1);
    auto collisionKey = oldKey;
    for (int i = 2; i < 10000; ++i) {
        collisionKey = collisionCache.MakeKey(float(i),2,3,0x80404040u,1);
        if (collisionKey.slot == oldKey.slot) break;
    }
    assert(collisionKey.slot == oldKey.slot && collisionKey.words[0] != oldKey.words[0]);
    u32 cached;
    collisionCache.Store(oldKey,0x12345678u);
    assert(collisionCache.Find(oldKey,cached) && cached == 0x12345678u);
    assert(!collisionCache.Find(collisionKey,cached));
    collisionCache.Store(collisionKey,0x87654321u);
    assert(!collisionCache.Find(oldKey,cached));
    collisionCache.Clear();
    assert(!collisionCache.Find(collisionKey,cached));
    assert(s_lightProfile.colorHits >= 4000 && s_lightProfile.colorMisses >= 4000);
    s_lightProfile = {}; s_worldLightCache.Clear(); s_surfaceLightMask = 0;
    std::puts("12000 cached colours match independent path; key collisions/invalidation pass");

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

    const struct { int flags; math::Vec3 color; } shellCases[] = {
        {RF_SHELL_RED, {1,0,0}}, {RF_SHELL_BLUE, {0,0,1}}, {RF_SHELL_GREEN, {0,1,0}},
        {RF_SHELL_RED | RF_SHELL_BLUE, {1,0,1}},
        {RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE, {1,1,1}},
        {RF_SHELL_DOUBLE, {0.9f,0.7f,0}},
        {RF_SHELL_DOUBLE | RF_SHELL_RED, {1,0,1}},
        {RF_SHELL_DOUBLE | RF_SHELL_BLUE, {0,1,1}},
        {RF_SHELL_HALF_DAM, {0.56f,0.59f,0.45f}},
        {RF_SHELL_HALF_DAM | RF_SHELL_GREEN, {0.56f,1,0.45f}}
    };
    for (const auto & item : shellCases)
    {
        const auto actual = AliasShellColor(item.flags);
        assert(actual.x == item.color.x && actual.y == item.color.y && actual.z == item.color.z);
    }
    const auto shellColor = AliasShellVertexColor({1.2f,-0.2f,0.5f});
    assert(shellColor.x == 255 && shellColor.y == 0 && shellColor.z == 127.5f && shellColor.w == 128);
    for (int normal = 0; normal < 162; ++normal)
    {
        const auto offset = AliasShellOffset(static_cast<u8>(normal));
        assert(std::fabs(offset.x*offset.x + offset.y*offset.y + offset.z*offset.z - 16.0f) < 0.001f);
    }
    assert(AliasShellOffset(255).x == AliasShellOffset(0).x);
    assert(effects::FlowingScroll(0) == 0 && effects::FlowingScroll(40) == 0);
    assert(effects::FlowingScroll(0.3125f) == 0.5f);
    assert(std::fabs(effects::FlowingScroll(1) - 0.4f) < 0.00001f);
    assert(std::fabs(effects::FlowingScroll(41) - effects::FlowingScroll(1)) < 0.00001f);

    mod::PolyVertex surfaceVertices[] = { {{0,0,0},0.25f,0.75f}, {{1,0,0},1,0}, {{0,1,0},0,1} };
    mod::ModelTriangle surfaceTriangle = {{0,1,2}};
    mod::ModelPoly poly = {3,nullptr,surfaceVertices,&surfaceTriangle};
    mod::ModelTexInfo info = {SURF_FLOWING};
    mod::ModelSurface surface = {&info,&poly};
    s_viewTime = 0.3125f;
    DrawTranslucentSurface(surface, {}, 0, 42);
    assert(lastS == 0.75f && lastT == 0.75f && lastAlpha == 42);
    assert(surfaceVertices[0].texture_s == 0.25f); // no source/cache mutation
    info.flags = 0;
    DrawTranslucentSurface(surface, {}, 0, 42);
    assert(lastS == 0.25f && lastT == 0.75f);
    std::puts("Shell colours/normal expansion and flowing translucent submission passed");
    std::puts("View effects: projection, beam geometry, RGB falloff and bounded dynamic tessellation passed");
}
