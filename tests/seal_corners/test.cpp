#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
#include <vector>
#include "ps2/renderer/world_profile.h"
using u32=std::uint32_t; using u16=std::uint16_t;
using ps2::view::WorldProfile; using ps2::view::WorldDetailScope;
static WorldProfile s_worldProfile;
namespace math {struct Vec3 {float x,y,z;}; struct alignas(16) Vec4 {float x,y,z,w;}; struct Mat4 {float bias;};}
namespace tex {struct Texture {};}
namespace effects {float FlowingScroll(float t) {return t*0.5f;}}
constexpr int SURF_FLOWING=64;
static float s_viewTime=3;
#include "types.inc"
namespace mod {
#include "triangle.inc"
struct PolyVertex {math::Vec3 position; float texture_s,texture_t,lightmap_s,lightmap_t;};
struct ModelPoly {int numVerts; PolyVertex * vertexes; ModelTriangle * triangles;};
struct ModelTexInfo {int flags;};
struct ModelSurface {ModelTexInfo * texInfo;};
}
static int samples,transforms;
static std::vector<ClipVertex> emitted;
u32 TriangleLightingKey(const mod::ModelPoly &,const mod::ModelTriangle &,u32 key) {return key;}
void SampleVertexLight(ClipVertex & v,const mod::ModelSurface &) {++samples; v.color={20,30,40,128};}
void SetClipDistances(ClipVertex & v,const math::Mat4 & m) {
    ++transforms;
    for (int i=0;i<8;++i) v.d.f[i]=m.bias+static_cast<float>(i);
}
void SubmitWorldTriangle(const ClipVertex (&c)[3],const math::Mat4 &,const tex::Texture &) {emitted.insert(emitted.end(),c,c+3);}
#include "seals.inc"
// Independent copy of the old per-frame linear lookup, preserving first match.
void Reference(const mod::ModelPoly & poly,const mod::ModelSurface & surface,const math::Mat4 & matrix,u32 key) {
    const float scroll=surface.texInfo->flags&SURF_FLOWING ? effects::FlowingScroll(s_viewTime):0;
    for (int t=0;t<poly.numVerts-2;++t) {
        const auto & tri=poly.triangles[t];
        if (tri.vertexes[0]==tri.vertexes[1] || tri.litCacheVertexCount<=3) continue;
        const auto * vertices=tri.litCacheVertices && tri.litCacheKey==key ? static_cast<const CachedLitVertex *>(tri.litCacheVertices):nullptr;
        const int count=vertices ? tri.litCacheVertexCount:0;
        ClipVertex c[3]={};
        for (int v=0;v<3;++v) {
            const auto & src=poly.vertexes[tri.vertexes[v]];
            c[v].pos={src.position.x,src.position.y,src.position.z,1};
            c[v].st={src.texture_s+scroll,src.texture_t,0,0};
            c[v].lightmap={src.lightmap_s,src.lightmap_t,0,0};
            bool found=false;
            for (int i=0;i<count;++i) {
                const auto & lit=vertices[i];
                if (lit.x==src.position.x && lit.y==src.position.y && lit.z==src.position.z) {
                    c[v].color={static_cast<float>(lit.packedColor&255u),static_cast<float>((lit.packedColor>>8)&255u),
                        static_cast<float>((lit.packedColor>>16)&255u),static_cast<float>(lit.packedColor>>24)};
                    found=true; break;
                }
            }
            if (!found) SampleVertexLight(c[v],surface);
            SetClipDistances(c[v],matrix);
        }
        SubmitWorldTriangle(c,matrix,{});
    }
}
int main() {
    for (int scenario=0;scenario<500;++scenario) {
        mod::PolyVertex verts[]={{{0,0,0},1,2,3,4},{{1,2,3},5,6,7,8},{{4,5,6},9,10,11,12}};
        if (scenario%13==0) verts[1].position.x=std::numeric_limits<float>::quiet_NaN();
        std::vector<CachedLitVertex> cache(static_cast<size_t>(6+scenario%187),{99,99,99,0x80402010u,0,0});
        const int count=static_cast<int>(cache.size());
        ClipVertex roots[3]={};
        for (int v=0;v<3;++v) {
            roots[v].pos={verts[v].position.x,verts[v].position.y,verts[v].position.z,1};
            const int i=v==0 ? 0 : (v==1 ? count/2 : count-1);
            cache[i]={verts[v].position.x,verts[v].position.y,verts[v].position.z,0x80102030u+static_cast<u32>(v),0,0};
        }
        cache[1]={-0.0f,0,0,0x80808080u,0,0}; //duplicate with a different colour.
        if (scenario%7==0) cache.back().x=99; //missing source corner fallback.
        mod::ModelTriangle tri={}; tri.vertexes[0]=0;tri.vertexes[1]=1;tri.vertexes[2]=2;
        tri.litCacheVertices=cache.data();tri.litCacheKey=123;tri.litCacheVertexCount=static_cast<u16>(count);
        CacheSealCornerIndices(tri,cache.data(),count,roots);
        assert(tri.litCacheCornerIndices[0]==0); //must preserve first equal corner.
        mod::ModelPoly poly={3,verts,&tri};
        mod::ModelTexInfo info={scenario%2 ? SURF_FLOWING:0}; mod::ModelSurface surface={&info};
        for (int pass=0;pass<7;++pass) {
            if (pass==1) for (auto & vertex:cache) vertex.packedColor^=0x00113355u; //relight, same indices.
            if (pass==2) tri.litCacheKey=124; //old key must not read cached colours.
            if (pass==3) {
                std::reverse(cache.begin(),cache.end());tri.litCacheKey=123;
                CacheSealCornerIndices(tri,cache.data(),count,roots); //topology rebuild.
            }
            if (pass==4) tri.litCacheVertices=nullptr; //nonretained rebuild.
            if (pass==5) tri.litCacheVertexCount=3; //no subdivision, no seal.
            if (pass==6) {
                tri.litCacheVertexCount=static_cast<u16>(count);
                tri.vertexes[0]=tri.vertexes[1]; //degenerate placeholder.
            }
            emitted.clear(); samples=transforms=0; Reference(poly,surface,{0.125f},123);
            const auto expected=emitted; const int sampleCount=samples,transformCount=transforms;
            emitted.clear();samples=transforms=0;
            GatherPolyCrackSeals(poly,surface,{0.125f},{},123);
            assert(samples==sampleCount && transforms==transformCount && emitted.size()==expected.size());
            if (!emitted.empty()) assert(std::memcmp(emitted.data(),expected.data(),emitted.size()*sizeof(ClipVertex))==0);
        }
    }
    puts("3500 production seal cases match old scan: duplicate/signed-zero/NaN, relight/rebuild/key/null/flow/skips PASS");
}
