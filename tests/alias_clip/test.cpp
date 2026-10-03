#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <random>
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
static void Reset() {
    s_scratchVertCount = trisDrawn = trisCulled = trisClipped = 0;
    emitted.clear(); batches.clear();
}
int main() {
    std::mt19937 random(105);
    std::uniform_real_distribution<float> position(-4.0f,4.0f), color(0.0f,128.0f);
    dstvert_t st[6] = {{0,0},{63,47},{94,-8},{-3,16},{17,29},{120,71}};
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
        PrepareAliasClipData(32,matrix);
        assert(transforms == 32);
        for (const auto & triangle : triangles)
            SubmitAliasTriangle(triangle,st,32,6,1.0f/128.0f,1.0f/64.0f,shell,matrix,texture,translucent);
        FlushScratch(matrix,texture,translucent);
        assert(transforms == 32); // 240 per-corner transforms replaced by 32 shared transforms.
        assert(trisDrawn == drawn && trisCulled == culled && trisClipped == clipped);
        assert(batches == referenceBatches && emitted.size() == reference.size());
        assert(std::memcmp(emitted.data(),reference.data(),emitted.size()*sizeof(vu1::DrawVertex)) == 0);
        cases += 80;
    }
    assert(drawnCases && clippedCases && culledCases);
    // Full maximum-index cache and replacement between different model matrices.
    PrepareAliasClipData(MAX_VERTS,math::Mat4{0.0f});
    PrepareAliasClipData(MAX_VERTS,math::Mat4{1.0f});
    assert(sizeof(s_aliasClipData) == 65536);
    std::printf("%d MD2 triangles match production clipper, colours, UVs, batches and alpha; shared transforms PASS\n",cases);
}
