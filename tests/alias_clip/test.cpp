#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <random>
#include <limits>
#include "ps2/renderer/world_profile.h"
using ps2::view::WorldProfile;
using ps2::view::WorldDetailScope;
using ps2::view::WorldSampleScope;
static WorldProfile s_worldProfile;
using u32 = std::uint32_t;
constexpr int MAX_VERTS = 2048, kNumClipPlanes = 6, kScratchMaxVerts = 24;
constexpr float kClipEpsilon = 0.01f;
static int transforms;
namespace math {
struct alignas(16) Vec4 { float x,y,z,w; };
struct Mat4 { float offset; };
Vec4 Transform(const Vec4 & v, const Mat4 & m) {
    ++transforms;
    return {v.x + m.offset, v.y - m.offset, v.z, v.w + v.z * 0.6f};
}
static int batchCalls, matrixLoads;
void TransformStrided(const void * source,int sourceStride,void * destination,
                      int destinationStride,int count,const Mat4 & matrix) {
    if (count<=0) return;
    ++batchCalls; ++matrixLoads;
    const auto * input=static_cast<const unsigned char *>(source);
    auto * output=static_cast<unsigned char *>(destination);
    for (int i=0;i<count;++i) {
        Vec4 position; std::memcpy(&position,input,sizeof(position));
        const auto result=Transform(position,matrix);
        std::memcpy(output,&result,sizeof(result));
        input+=sourceStride; output+=destinationStride;
    }
}
void LerpTo(Vec4 & o, const Vec4 & a, const Vec4 & b, float t) {
    o = {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
}
}
namespace tex { struct Texture {}; }
namespace vu1 {
constexpr float kGuardBandNdcLimit = 2.0f;
struct DrawVertex { float x,y,z,w; u32 rgba; float s,t,q; };
u32 PackColorRGBA(u32 r,u32 g,u32 b,u32 a) { return r | (g<<8) | (b<<16) | (a<<24); }
}
struct dtriangle_t { short index_xyz[3], index_st[3]; };
struct dstvert_t { short s,t; };
#include "types.inc"
static PreparedAliasVertex s_preparedAliasVerts[MAX_VERTS];
static AliasClipData s_aliasClipData[MAX_VERTS];
static AliasTexCoord s_preparedAliasTexCoords[MAX_VERTS];
static bool s_aliasTexCoordsPrepared;
static WorldClipEntry s_worldClipCache[128];
static u32 s_worldClipEpoch;
static vu1::DrawVertex s_scratchVerts[kScratchMaxVerts];
static int s_scratchVertCount;
static u32 s_surfaceLightMask;
static std::vector<vu1::DrawVertex> emitted;
static std::vector<int> batches;
static int trisDrawn, trisCulled, trisClipped;
#define PS2_STAT_INC(x) (++x)
#define PS2_STAT_ADD(x,n) (x += n)
#define PS2_Assert(x) assert(x)
u32 AddWorldLights(u32 color, const math::Vec4 &) { return color ^ 0x00101010u; }
void FlushScratch(const math::Mat4 &, const tex::Texture &, bool alphaBlend = false, int fixedAlpha = -1) {
    assert(fixedAlpha == -1);
    batches.push_back(s_scratchVertCount * (alphaBlend ? -1 : 1));
    for (int i=0;i<s_scratchVertCount;++i) emitted.push_back(s_scratchVerts[i]);
    s_scratchVertCount = 0;
}
#include "generic.inc"
#include "alias.inc"
#include "worldclip.inc"
static int dynamicCalls;
void SubmitDynamicallyLitTriangle(const ClipVertex (&corners)[3], const math::Mat4 & mvp,
    const tex::Texture & texture, int depth, bool boundsSelected) {
    assert(depth == 0); (void)boundsSelected;
    ++dynamicCalls;
    SubmitWorldTriangle(corners,mvp,texture);
}
#include "cachedworld.inc"
static void Reset() {
    s_scratchVertCount = trisDrawn = trisCulled = trisClipped = 0;
    emitted.clear(); batches.clear();
}
int main() {
    std::mt19937 random(105);
    std::uniform_real_distribution<float> position(-4.0f,4.0f), color(0.0f,128.0f);
    dstvert_t st[MAX_VERTS+1] = {{0,0},{63,47},{94,-8},{-3,16},{17,29},{120,71}};
    tex::Texture texture;
    int cases = 0, drawnCases = 0, clippedCases = 0, culledCases = 0;
    for (int scenario=0;scenario<400;++scenario) {
        for (int i=0;i<32;++i) {
            auto & v=s_preparedAliasVerts[i];
            v.pos={position(random),position(random),position(random),1.0f};
            v.color={color(random),color(random),color(random),color(random)};
        }
        // Exact/epsilon plane boundaries plus shared corners, as in view weapons.
        s_preparedAliasVerts[0].pos = {0,0,0,1};
        s_preparedAliasVerts[1].pos = {0.2f,0,0,1};
        s_preparedAliasVerts[2].pos = {0,0.2f,0,1};
        s_preparedAliasVerts[3].pos = {0,0,2,1};
        s_preparedAliasVerts[4].pos = {9,9,-3,1};
        s_preparedAliasVerts[5].pos = {0,0,1.0f / 0.4f,1};
        s_preparedAliasVerts[6].pos = {0,0,(1.0f-kClipEpsilon) / 0.4f,1};
        dtriangle_t triangles[80];
        for (auto & triangle : triangles) for (int c=0;c<3;++c) {
            triangle.index_xyz[c]=static_cast<short>(random()%32);
            triangle.index_st[c]=static_cast<short>(random()%6);
        }
        triangles[0]={{0,1,2},{0,1,2}};
        triangles[1]={{4,4,4},{1,2,3}};
        triangles[2]={{0,3,2},{2,3,4}};
        triangles[3]={{0,5,6},{3,4,5}};
        const math::Mat4 matrix={scenario%2 ? 0.0f : 0.31f};
        const bool shell=(scenario%3 == 0), translucent=(scenario%2 == 0);
        const int stCount=scenario%7 == 0 ? MAX_VERTS+1 : 6;
        s_surfaceLightMask=scenario%5 == 0 ? 1u : 0u;
        Reset();
        for (const auto & triangle : triangles) {
            ClipVertex corners[3]={};
            for (int c=0;c<3;++c) {
                const auto & v=s_preparedAliasVerts[triangle.index_xyz[c]];
                const auto & uv=st[triangle.index_st[c]];
                corners[c].pos=v.pos; corners[c].color=v.color;
                corners[c].st={shell ? 0.5f : (static_cast<float>(uv.s)+0.5f)/128.0f,
                    shell ? 0.5f : (static_cast<float>(uv.t)+0.5f)/64.0f,0,0};
                SetClipDistances(corners[c].d,v.pos,matrix);
            }
            SubmitWorldTriangle(corners,matrix,texture,translucent);
        }
        FlushScratch(matrix,texture,translucent);
        const auto reference=emitted;
        const auto referenceBatches=batches;
        const int drawn=trisDrawn, culled=trisCulled, clipped=trisClipped;
        drawnCases+=drawn; culledCases+=culled; clippedCases+=clipped;
        Reset(); transforms=0;
        const int beforeBatch=math::batchCalls, beforeMatrices=math::matrixLoads;
        PrepareAliasClipData(32,matrix);
        assert(math::batchCalls==beforeBatch+1 && math::matrixLoads==beforeMatrices+1);
        PrepareAliasTexCoords(st,stCount,1.0f/128.0f,1.0f/64.0f,shell);
        assert(transforms == 32);
        for (const auto & triangle : triangles)
            SubmitAliasTriangle(triangle,st,32,stCount,1.0f/128.0f,1.0f/64.0f,shell,matrix,texture,translucent);
        FlushScratch(matrix,texture,translucent);
        assert(transforms == 32); // 240 per-corner transforms replaced by 32 shared transforms.
        assert(trisDrawn == drawn && trisCulled == culled && trisClipped == clipped);
        assert(batches == referenceBatches && emitted.size() == reference.size());
        assert(std::memcmp(emitted.data(),reference.data(),emitted.size()*sizeof(vu1::DrawVertex)) == 0);
        // Opaque corners use packed colours, independent of world modulation.
        for (int i=0;i<32;++i) s_preparedAliasVerts[i].packedColor=PackFloatColor(s_preparedAliasVerts[i].color);
        Reset();
        for (const auto & triangle : triangles) {
            if (s_scratchVertCount+3 > kScratchMaxVerts) FlushScratch(matrix,texture);
            for (int c=0;c<3;++c) {
                const auto & v=s_preparedAliasVerts[triangle.index_xyz[c]];
                const auto & uv=st[triangle.index_st[c]];
                auto & out=s_scratchVerts[s_scratchVertCount++];
                out={v.pos.x,v.pos.y,v.pos.z,1.0f,v.packedColor,
                    (static_cast<float>(uv.s)+0.5f)/128.0f,
                    (static_cast<float>(uv.t)+0.5f)/64.0f,1.0f};
            }
        }
        FlushScratch(matrix,texture);
        const auto opaqueReference=emitted;
        const auto opaqueBatches=batches;
        Reset(); PrepareAliasTexCoords(st,stCount,1.0f/128.0f,1.0f/64.0f,false);
        for (const auto & triangle : triangles)
            SubmitOpaqueAliasTriangle(triangle,st,32,stCount,1.0f/128.0f,1.0f/64.0f,matrix,texture);
        FlushScratch(matrix,texture);
        assert(trisDrawn == 80 && batches == opaqueBatches && emitted.size() == opaqueReference.size());
        assert(std::memcmp(emitted.data(),opaqueReference.data(),emitted.size()*sizeof(vu1::DrawVertex)) == 0);
        cases += 80;
    }
    assert(drawnCases && clippedCases && culledCases);
    // Seam indices need not match XYZ indices; oversized tables retain fallback.
    std::vector<dstvert_t> largeST(32768);
    for (int i=0;i<32768;++i) largeST[i]={static_cast<short>(i-16384),static_cast<short>(32767-i)};
    for (int count : {1,MAX_VERTS,MAX_VERTS+1,32768}) {
        for (bool shell : {false,true}) {
            PrepareAliasTexCoords(largeST.data(),count,1.0f/512.0f,1.0f/256.0f,shell);
            assert(s_aliasTexCoordsPrepared == (!shell && count<=MAX_VERTS));
            for (int i=0;i<count;++i) {
                const auto uv=AliasTexCoordsAt(i,largeST.data(),1.0f/512.0f,1.0f/256.0f,shell);
                assert(uv.s == (shell ? 0.5f : (static_cast<float>(largeST[i].s)+0.5f)/512.0f));
                assert(uv.t == (shell ? 0.5f : (static_cast<float>(largeST[i].t)+0.5f)/256.0f));
            }
        }
    }
    assert(sizeof(s_preparedAliasTexCoords) == 16384);
    // Full maximum-index cache and replacement between different model matrices.
    PrepareAliasClipData(MAX_VERTS,math::Mat4{0.0f});
    PrepareAliasClipData(MAX_VERTS,math::Mat4{1.0f});
    assert(sizeof(s_aliasClipData) == 65536);
    for (int count:{0,1,3,31,32,33,MAX_VERTS}) {
        for (int i=0;i<count;++i) {
            s_preparedAliasVerts[i].pos={position(random),position(random),position(random),1};
            s_preparedAliasVerts[i].color={64,96,128,128};
        }
        std::memset(s_aliasClipData,0xA5,sizeof(s_aliasClipData));
        const int beforeBatches=math::batchCalls;
        const math::Mat4 matrix={0.25f};
        PrepareAliasClipData(count,matrix);
        assert(math::batchCalls==beforeBatches+(count>0));
        for (int i=0;i<count;++i) {
            ClipDists reference;
            SetClipDistances(reference,s_preparedAliasVerts[i].pos,matrix);
            assert(std::memcmp(s_aliasClipData[i].distances,reference.f,6*sizeof(float))==0);
            u32 mask=0;
            for (int p=0;p<6;++p) if (!(reference.f[p]>=0.0f)) mask|=1u<<p;
            assert(s_aliasClipData[i].outsideMask==mask);
            assert(s_aliasClipData[i].packedColor==PackFloatColor(s_preparedAliasVerts[i].color));
        }
        if (count<MAX_VERTS) {
            unsigned char untouched[sizeof(AliasClipData)]; std::memset(untouched,0xA5,sizeof(untouched));
            assert(std::memcmp(&s_aliasClipData[count],untouched,sizeof(untouched))==0);
        }
    }
    puts("Strided MD2 preparation: zero/odd/max counts, full records and write bounds PASS (host VU stub)");
    // World/brush MVP contexts invalidate exact shared positions without
    // retaining colour or texture state. Hash collisions must recompute.
    int reuseHits = 0;
    for (int context=0;context<100;++context) {
        BeginWorldClipCache();
        const math::Mat4 matrix={static_cast<float>(context)*0.07f};
        for (int i=0;i<120;++i) {
            CachedLitVertex vertex={position(random),position(random),position(random),0,0,0};
            if (i==0) vertex={0,0,0,0,0,0};
            if (i==1) vertex={-0.0f,0,0,0,0,0};
            ClipDists reference, cached;
            SetClipDistances(reference,{vertex.x,vertex.y,vertex.z,1},matrix);
            u32 referenceMask=0;
            for (int p=0;p<6;++p) if (!(reference.f[p]>=0.0f)) referenceMask |= 1u<<p;
            assert(CachedWorldClipDistances(cached,vertex,matrix) == referenceMask);
            assert(std::memcmp(&cached,&reference,sizeof(cached)) == 0);
            const int before=transforms;
            vertex.packedColor=0x80808080u; vertex.s=3.0f; // key depends only on position
            assert(CachedWorldClipDistances(cached,vertex,matrix) == referenceMask);
            assert(transforms == before && std::memcmp(&cached,&reference,sizeof(cached)) == 0);
            ++reuseHits;
        }
    }
    s_worldClipEpoch=UINT32_MAX;
    BeginWorldClipCache(); assert(s_worldClipEpoch == 1);
    ClipDists cached, reference;
    const CachedLitVertex origin={0,0,0,0,0,0};
    const math::Mat4 moved={3.0f};
    SetClipDistances(reference,{0,0,0,1},moved);
    CachedWorldClipDistances(cached,origin,moved);
    assert(std::memcmp(&cached,&reference,sizeof(cached)) == 0);
    assert(sizeof(s_worldClipCache) == 8192 && reuseHits == 12000);
    // Explicit non-finite and boundary inputs exercise unordered comparisons,
    // signed zero and epsilon neighbours on both cache misses and hits.
    const float special[] = {0.0f, -0.0f, kClipEpsilon,
        std::nextafter(kClipEpsilon, 0.0f), std::nextafter(kClipEpsilon, 1.0f),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()};
    for (float x:special) for (float y:special) for (float z:special) {
        BeginWorldClipCache();
        const CachedLitVertex vertex={x,y,z,0,0,0};
        SetClipDistances(reference,{x,y,z,1},moved);
        u32 expectedMask=0;
        for (int p=0;p<6;++p) if (!(reference.f[p]>=0.0f)) expectedMask|=1u<<p;
        assert(CachedWorldClipDistances(cached,vertex,moved)==expectedMask);
        assert(std::memcmp(&cached,&reference,sizeof(cached))==0);
        const int before=transforms;
        assert(CachedWorldClipDistances(cached,vertex,moved)==expectedMask);
        assert(transforms==before);
    }
    std::puts("512 non-finite/signed-zero/epsilon cache inputs preserve mask and distance bits PASS");
    std::puts("12000 world/brush clip-cache results match direct transforms; context and epoch wrap PASS");
    int earlyRejected = 0;
    for (int scenario=0; scenario<600; ++scenario) {
        CachedLitVertex vertices[60];
        for (auto & vertex : vertices)
            vertex={position(random),position(random),position(random),0x80804020u,position(random),position(random)};
        for (auto & vertex : vertices) {
            vertex.packedColor=(random()%129u) | ((random()%129u)<<8) | ((random()%129u)<<16) | 0x80000000u;
            if (scenario%4==0) { vertex.x*=0.03f; vertex.y*=0.03f; vertex.z*=0.03f; }
        }
        // Common-plane rejection, exact plane boundary and an intersecting
        // triangle with different outside planes must all retain exact output.
        vertices[0]={9,0,0,0x80804020u,0,0};
        vertices[1]={10,1,0,0x80804020u,0,0};
        vertices[2]={11,-1,0,0x80804020u,0,0};
        vertices[3]={0,0,(1.0f-kClipEpsilon)/0.4f,0x80804020u,0,0};
        vertices[4]={-9,0,0,0x80804020u,0,0};
        vertices[5]={9,0,0,0x80804020u,0,0};
        vertices[6]={0,0,0,0x80804020u,0,0};
        vertices[7]={0.2f,0,0,0x80804020u,0,0};
        vertices[8]={0,0.2f,0,0x80804020u,0,0};
        const math::Mat4 matrix={scenario%2 ? 0.0f : 0.31f};
        const float scroll=scenario%3 ? 0.0f : -0.37f;
        s_surfaceLightMask=0;
        Reset();
        int expectedInside = 0;
        for (int first=0; first<60; first+=3) {
            ClipVertex corners[3]={};
            for (int v=0;v<3;++v) {
                const auto & src=vertices[first+v];
                corners[v].pos={src.x,src.y,src.z,1};
                corners[v].st={src.s+scroll,src.t,0,0};
                UnpackCachedColor(corners[v].color,src.packedColor);
                SetClipDistances(corners[v].d,corners[v].pos,matrix);
            }
            bool inside=true;
            for (const auto & corner : corners)
                for (int plane=0;plane<kNumClipPlanes;++plane) inside &= corner.d.f[plane]>=0.0f;
            if (inside) ++expectedInside;
            SubmitWorldTriangle(corners,matrix,texture);
        }
        FlushScratch(matrix,texture);
        const auto expected=emitted;
        const auto expectedBatches=batches;
        const int drawn=trisDrawn, culled=trisCulled, clipped=trisClipped;
        earlyRejected+=culled;
        Reset(); BeginWorldClipCache();
        s_worldProfile = {}; s_worldProfile.enabled = true;
        s_worldProfile.sampled = true;
        s_worldProfile.singleDetails = true;
        s_worldProfile.sampleCursor = 32;
        s_worldProfile.activePhase = WorldProfile::Geometry;
        EmitCachedWorld(vertices,60,matrix,texture,scroll);
        FlushScratch(matrix,texture);
        assert(trisDrawn==drawn && trisCulled==culled && trisClipped==clipped);
        assert(batches==expectedBatches && emitted.size()==expected.size());
        assert(std::memcmp(emitted.data(),expected.data(),emitted.size()*sizeof(vu1::DrawVertex))==0);
        assert(s_worldProfile.cachedTriangles==20 && s_worldProfile.unlitTriangles==20);
        assert(s_worldProfile.earlyRejects==culled);
        assert(s_worldProfile.sampleRoots==20 && s_worldProfile.sampleCount==1);
        assert(s_worldProfile.packedInside==expectedInside);
        assert(expectedInside>0);
        // Active-light triangles still reach the original subdivision entry,
        // including triangles rejected by the common-plane test.
        Reset(); BeginWorldClipCache(); s_surfaceLightMask=1; dynamicCalls=0;
        s_worldProfile = {}; s_worldProfile.enabled = true;
        s_worldProfile.sampled = true;
        s_worldProfile.singleDetails = true;
        s_worldProfile.activePhase = WorldProfile::Geometry;
        EmitCachedWorld(vertices,60,matrix,texture,scroll);
        assert(dynamicCalls==20);
        assert(s_worldProfile.cachedTriangles==20 && s_worldProfile.unlitTriangles==0 && s_worldProfile.earlyRejects==0);
        assert(s_worldProfile.packedInside==0);
        assert(s_worldProfile.sampleRoots==20 && s_worldProfile.sampleCount==1);
    }
    assert(earlyRejected>600);
    std::puts("12000 cached BSP triangles match generic clipping/output/batches; active-light dispatch preserved PASS");
    std::printf("%d MD2 triangles match production clipper, colours, UVs, batches and alpha; shared transforms, opaque UV reuse and oversized fallback PASS\n",cases);
}
