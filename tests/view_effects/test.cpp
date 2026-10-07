#include "ps2/renderer/view_effects.h"
#include "ps2/renderer/lighting_lod.h"
using ps2::view::DynamicLightSpacing;
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
#include "ps2/renderer/world_profile.h"
using ps2::view::WorldProfile;
using ps2::view::WorldDetailScope;
static WorldProfile s_worldProfile;
#include "ps2/renderer/world_light_cache.h"
using ps2::view::WorldLightCache;
static WorldLightCache s_worldLightCache;
static bool s_worldLightCacheEnabled = true;
#include "ps2/renderer/surface_light_cache.h"
using ps2::view::SurfaceLightCache;
static SurfaceLightCache s_surfaceLightCache;
static bool referenceMode;
static int edgeEvaluations, referenceEdgeEvaluations;

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
static bool s_farDynamicLightingEnabled=false, s_worldLightingPass=false;
static math::Vec3 s_lightingEye={};
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
struct cplane_t { float normal[3], dist; };
namespace mod {
struct PolyVertex { math::Vec3 position; float texture_s, texture_t; };
struct ModelTriangle { int vertexes[3]; };
struct ModelPoly { int numVerts; const ModelPoly * next; PolyVertex * vertexes; ModelTriangle * triangles; };
struct ModelTexInfo { int flags; };
struct ModelSurface { const ModelTexInfo * texInfo; const ModelPoly * polys; const cplane_t * plane = nullptr; };
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
// Recursion test records every leaf; real clipping/flush equivalence is tested
// separately by alias_clip using the production SubmitDynamicLeaf.
void SubmitDynamicLeaf(const ClipVertex & a, const ClipVertex & b,
                       const ClipVertex & c, const math::Mat4 & mvp,
                       const tex::Texture & texture)
{
    const ClipVertex corners[3] = { a, b, c };
    SubmitWorldTriangle(corners, mvp, texture);
}
#include "effects.inc"
#include "dynamic_reference.inc"
#include "dynamic129_reference.inc"
#include "selection_reference.inc"

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
        s_worldLightCacheEnabled = scenario % 3 != 0;
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
        emitted.clear(); SubmitDynamicallyLitTriangle(large,{},{},0,true);
        assert(emitted == reference && s_surfaceLightMask == selected);
    }
    // Vary geometry, edge ties/thresholds and large translated coordinates,
    // not only light positions. Reference recursion copies full child arrays.
    for (int scenario = 0; scenario < 400; ++scenario) {
        ClipVertex probe[3] = {};
        for (int v = 0; v < 3; ++v) {
            const float offset = scenario % 11 == 0 ? 33554432.0f :
                (scenario % 5 == 0 ? 65536.0f : 0.0f);
            probe[v].pos = {offset + float((scenario*13+v*131)%700-350) + .1f,
                float((scenario*31+v*193)%800-400) + .3f,
                float((scenario*7+v*41)%256) + .7f, 1};
            probe[v].st = {float(v)*0.3f, float(v)*0.7f, 0, 0};
            probe[v].color = {20+float(v)*13, 40+float(v)*7, 70, 128};
        }
        if (scenario % 7 == 0) probe[2] = probe[1];
        for (int i = 0; i < 3; ++i) {
            lights[i] = {float(80 + (scenario*19+i*37)%500), {1, .3f, .8f}};
            s_worldLightOrigins[i] = {probe[i].pos.x + 30, probe[i].pos.y - 50, probe[i].pos.z};
        }
        s_worldLightCount = 3;
        s_surfaceLightMask = 7;
        emitted.clear(); referenceMode = true;
        const int depth = scenario % 9 == 0 ? 7 : 0;
        ReferenceDynamicTriangle(probe, {}, {}, depth);
        referenceMode = false;
        const auto expected = emitted;
        emitted.clear(); s_worldLightCache.Clear();
        SubmitDynamicallyLitTriangle(probe, {}, {}, depth);
        assert(emitted == expected && s_surfaceLightMask == 7);
        // The frozen129 path also covers reduced distant spacing, brushes,
        // overlapping lights and depth-cap calls. Keep every leaf byte equal.
        s_farDynamicLightingEnabled = scenario % 2 != 0;
        s_worldLightingPass = scenario % 3 != 0;
        s_lightingEye = {-1000, -1000, -1000};
        emitted.clear(); referenceMode = true;
        Reference129DynamicVertices(probe[0], probe[1], probe[2], {}, {}, depth);
        referenceMode = false;
        const auto expected129 = emitted;
        emitted.clear(); s_worldLightCache.Clear();
        SubmitDynamicallyLitTriangle(probe, {}, {}, depth);
        assert(emitted == expected129 && s_surfaceLightMask == 7);
        s_farDynamicLightingEnabled = s_worldLightingPass = false;
    }
    // A large triangle under an explosion reaches the unchanged depth cap.
    // Count actual edge arithmetic separately from emitted geometry/cost.
    ClipVertex explosion[3] = {};
    explosion[0].pos = {-4096, -4096, 0, 1};
    explosion[1].pos = {4096, -4096, 0, 1};
    explosion[2].pos = {0, 4096, 0, 1};
    for (auto & v : explosion) v.color = {50, 70, 80, 128};
    lights[0] = {100000, {1, .5f, .5f}};
    s_worldLightOrigins[0] = {0, 0, 0};
    s_surfaceLightMask = 1;
    emitted.clear(); referenceMode = true; referenceEdgeEvaluations = 0;
    Reference129DynamicVertices(explosion[0], explosion[1], explosion[2], {}, {});
    referenceMode = false;
    const auto expectedExplosion = emitted;
    emitted.clear(); s_worldLightCache.Clear(); edgeEvaluations = 0;
    SubmitDynamicallyLitTriangle(explosion, {}, {});
    assert(emitted == expectedExplosion && s_surfaceLightMask == 1);
    assert(edgeEvaluations == 192 && referenceEdgeEvaluations == 765);
    std::printf("Explosion depth-cap edge calculations: %d -> %d; identical leaves/colours PASS\n",
                referenceEdgeEvaluations, edgeEvaluations);
    s_worldLightCount = 32;
    s_surfaceLightMask = 0;
    s_worldLightCacheEnabled = true;
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
    ClipVertex policyCorners[3]={large[0],large[1],large[2]};
    for (auto & corner:policyCorners) {corner.pos.x*=0.25f; corner.pos.y*=0.25f;}
    emitted.clear(); triangles=0;
    SubmitDynamicallyLitTriangle(policyCorners,{},{});
    const auto fullGeometry=emitted;
    const int fullCount=triangles;
    s_farDynamicLightingEnabled=true; s_worldLightingPass=true;
    assert(DynamicLightSpacing(384.0f*384.0f)==64.0f);
    assert(DynamicLightSpacing(768.0f*768.0f)==128.0f);
    assert(DynamicLightSpacing(600.0f*600.0f)>64.0f && DynamicLightSpacing(600.0f*600.0f)<128.0f);
    s_lightingEye={-2000,0,0};
    assert(DynamicTriangleEdgeSquared(policyCorners[0],policyCorners[1],policyCorners[2])==128.0f*128.0f);
    emitted.clear(); triangles=0; s_worldLightCache.Clear();
    SubmitDynamicallyLitTriangle(policyCorners,{},{});
    assert(triangles<fullCount && s_surfaceLightMask==1);
    const int reducedCount=triangles;
    // Covered area is conserved; no root policy may drop lit geometry.
    auto area=[](const std::vector<std::array<u32,13>> & records) {
        float sum=0;
        for (size_t i=0;i<records.size();i+=3) {
            float p[3][3];
            for (int v=0;v<3;++v) std::memcpy(p[v],records[i+static_cast<size_t>(v)].data(),sizeof(p[v]));
            const float ax=p[1][0]-p[0][0], ay=p[1][1]-p[0][1], az=p[1][2]-p[0][2];
            const float bx=p[2][0]-p[0][0], by=p[2][1]-p[0][1], bz=p[2][2]-p[0][2];
            const float x=ay*bz-az*by, y=az*bx-ax*bz, z=ax*by-ay*bx;
            sum+=std::sqrt(x*x+y*y+z*z)*0.5f;
        }
        return sum;
    };
    assert(std::fabs(area(emitted)-area(fullGeometry))<0.01f);
    s_worldLightingPass=false; emitted.clear(); triangles=0; s_worldLightCache.Clear();
    SubmitDynamicallyLitTriangle(policyCorners,{},{});
    assert(emitted==fullGeometry); // Brushes preserve the full tree.
    s_worldLightingPass=true; s_lightingEye={0,0,0};
    emitted.clear(); s_worldLightCache.Clear(); SubmitDynamicallyLitTriangle(policyCorners,{},{});
    assert(emitted==fullGeometry); // Near and long faces keep full detail.
    s_farDynamicLightingEnabled=false; s_worldLightingPass=false;
    std::printf("Far dynamic policy: %d -> %d leaves; area, near/brush output and mask restoration PASS\n",fullCount,reducedCount);
    WorldLightScope stopped(s_lightProfile,WorldLightProfile::Select);
    stopped.Stop();
    const auto stoppedTicks = s_lightProfile.ticks[WorldLightProfile::Select];
    stopped.Stop();
    assert(s_lightProfile.ticks[WorldLightProfile::Select] == stoppedTicks);
    s_lightProfile = {}; s_surfaceLightMask = 0;
    std::puts("160 Alpha.93 differential light/subdivision scenarios passed");

    // Masks must match the frozen selector for varied, boundary and
    // degenerate bounds. Keep high-bit sources, dense masks and strict tangency.
    for (int scenario = 0; scenario < 2400; ++scenario) {
        const float scale = scenario%2 ? 0.125f : 32.0f;
        math::Vec4 points[3];
        for (int v = 0; v < 3; ++v)
            points[v] = {float((scenario*19+v*31)%127-63)*scale,
                float((scenario*23+v*11)%97-48)*scale,
                float((scenario*7+v*17)%73-36)*scale,1};
        if (scenario%5 == 0) points[1] = points[2] = points[0];
        for (int i = 0; i < 32; ++i) {
            lights[i].intensity = float(1+(scenario*13+i*31)%257)*scale;
            s_worldLightOrigins[i] = {float((scenario*3+i*17)%191-95)*scale,
                float((scenario*11+i*23)%151-75)*scale,float((scenario*7+i*13)%109-54)*scale};
        }
        const u32 mask = scenario%3 == 0 ? 0xffffffffu : scenario%3 == 1 ? 0x80000001u : 0;
        assert(SelectTriangleLights(mask,points) == ReferenceSelectTriangleLights(mask,points));
    }
    const math::Vec4 pointBounds[3] = {{0,0,0,1},{0,0,0,1},{0,0,0,1}};
    lights[31].intensity = 64;
    for (float delta : {63.999f,64.0f,64.001f}) {
        s_worldLightOrigins[31] = {delta,0,0};
        assert(SelectTriangleLights(0x80000000u,pointBounds) == ReferenceSelectTriangleLights(0x80000000u,pointBounds));
    }
    s_worldLightCache.Clear(); s_lightProfile = {}; s_lightProfile.enabled = true;
    s_surfaceLightMask = 1; lights[0].intensity = 1000000; s_worldLightOrigins[0] = {0,0,0};
    emitted.clear(); SubmitDynamicallyLitTriangle(large,{},{});
    const auto normalTree = emitted;
    const int normalBounds = s_lightProfile.bounds;
    s_lightProfile = {}; s_lightProfile.enabled = true;
    emitted.clear(); SubmitDynamicallyLitTriangle(large,{},{},0,true);
    assert(emitted == normalTree && s_lightProfile.bounds == normalBounds-1);
    assert(s_surfaceLightMask == 1);
    std::puts("2400 selectors match Alpha.97; known root removes exactly one bounds call");

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
    s_worldLightCacheEnabled = false;
    const int hitsBefore = s_lightProfile.colorHits;
    for (int v = 0; v < 100; ++v) {
        const math::Vec4 point = {float(v),0,0,1};
        s_surfaceLightMask = 0x80000001u;
        assert(AddWorldLights(0x80404040u,point) == DirectWorldLights(0x80404040u,point));
        assert(AddWorldLights(0x80404040u,point) == DirectWorldLights(0x80404040u,point));
    }
    assert(s_lightProfile.colorHits == hitsBefore);
    s_worldLightCacheEnabled = true;
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

    // Production surface selector must reuse only in an unchanged light
    // context. Both empty masks and bit31 are cacheable; a new brush/frame
    // context must recompute even when the surface address is identical.
    cplane_t plane = {{1,0,0},0};
    mod::ModelSurface face = {}; face.plane = &plane;
    s_worldLightCount = 32;
    for (int i = 0; i < 32; ++i) {
        lights[i].intensity = 64;
        s_worldLightOrigins[i] = {float(i == 31 ? 0 : 1000),0,0};
    }
    s_surfaceLightCache.Clear(); s_lightProfile = {}; s_lightProfile.enabled = true;
    SelectSurfaceLights(face); assert(s_surfaceLightMask == 0x80000000u);
    SelectSurfaceLights(face); assert(s_surfaceLightMask == 0x80000000u && s_lightProfile.surfaceTests == 32);
    s_worldLightOrigins[31].x = 1000; // transformed/new frame origins
    s_surfaceLightCache.Clear();
    SelectSurfaceLights(face); assert(s_surfaceLightMask == 0 && s_lightProfile.surfaceTests == 64);
    SelectSurfaceLights(face); assert(s_surfaceLightMask == 0 && s_lightProfile.surfaceTests == 64);
    s_worldLightCount = 0;
    SelectSurfaceLights(face); assert(s_surfaceLightMask == 0 && s_lightProfile.surfaceTests == 64);
    s_worldLightCount = 32;
    mod::ModelSurface faces[300] = {};
    cplane_t planes[300] = {};
    for (int context = 0; context < 12; ++context) {
        s_surfaceLightCache.Clear();
        for (int i = 0; i < 32; ++i) {
            s_worldLightOrigins[i] = {float(i*17-context*11),float(i*7),float(context*3)};
            lights[i].intensity = float(20+i*3);
        }
        for (int pass = 0; pass < 2; ++pass)
            for (int f = 0; f < 300; ++f) {
                planes[f] = {{0.6f,0.8f,0},float(f*3-300)};
                faces[f].plane = &planes[f];
                u32 expected = 0;
                for (int i = 0; i < 32; ++i) {
                    const auto & o = s_worldLightOrigins[i];
                    const float distance = o.x*planes[f].normal[0]+o.y*planes[f].normal[1]+o.z*planes[f].normal[2]-planes[f].dist;
                    if (std::fabs(distance) < lights[i].intensity) expected |= 1u << i;
                }
                SelectSurfaceLights(faces[f]);
                assert(s_surfaceLightMask == expected);
            }
    }
    s_surfaceLightCache.Clear(); s_lightProfile = {}; s_surfaceLightMask = 0;
    std::puts("7200 surface masks match scalar reference; context invalidation and reuse pass");

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
