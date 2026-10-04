#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "ps2/renderer/lighting_lod.h"
using namespace ps2::view;
using u32 = std::uint32_t;
namespace math { struct Vec3 { float x,y,z; }; struct Vec4 {float x,y,z,w;}; }
struct ClipVertex { math::Vec4 pos, color; };
namespace mod {
struct ModelSurface {const void * samples;};
struct PolyVertex {math::Vec3 position;};
struct ModelPoly {PolyVertex * vertexes;};
struct ModelTriangle {int vertexes[3]; void * litCacheVertices; u32 litCacheKey;};
}
static bool s_farLightingEnabled=true, s_worldLightingPass=true;
static math::Vec3 s_lightingEye={};
constexpr u32 kFarLightingKey=0xA17F39C5u;
static float s_lightMaxSamplesPerEdge=12, s_lightErrorTolerance=8;
constexpr int kMaxLightSubdivideDepth=8;
constexpr float kMinLightSamplesPerEdge=1;
static int s_litBuildFineSplits;
static std::vector<ClipVertex> output;
static float LightEdgeLengthSq(const ClipVertex & a,const ClipVertex & b) {
    const float x=a.pos.x-b.pos.x,y=a.pos.y-b.pos.y;
    return x*x+y*y;
}
static void AppendCachedTriangle(const ClipVertex (&c)[3]) {output.insert(output.end(),c,c+3);}
static math::Vec4 Color(math::Vec4 p) {return {p.x*2,p.y*2,0,128};}
static ClipVertex LightMidpoint(const ClipVertex & a,const ClipVertex & b,const mod::ModelSurface &) {
    const math::Vec4 p={(a.pos.x+b.pos.x)/2,(a.pos.y+b.pos.y)/2,0,1}; return {p,Color(p)};
}
static ClipVertex LightCentroid(const ClipVertex (&c)[3],const mod::ModelSurface &) {
    const math::Vec4 p={(c[0].pos.x+c[1].pos.x+c[2].pos.x)/3,(c[0].pos.y+c[1].pos.y+c[2].pos.y)/3,0,1};
    return {p,Color(p)};
}
static float MaxLightError(math::Vec4 c,float r,float g,float b) {
    return std::max(std::fabs(c.x-r),std::max(std::fabs(c.y-g),std::fabs(c.z-b)));
}
#include "lighting.inc"
static float Area() {
    float total=0;
    for (size_t i=0;i<output.size();i+=3) {
        const auto a=output[i].pos,b=output[i+1].pos,c=output[i+2].pos;
        total+=std::fabs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))/2;
    }
    return total;
}
int main() {
    mod::PolyVertex vertices[]={{{600,0,0}},{{650,0,0}},{{600,50,0}}};
    mod::ModelPoly poly={vertices}; int retained=1;
    mod::ModelTriangle tri={{0,1,2},&retained,123};
    const u32 nearKey=123, farKey=nearKey^kFarLightingKey;
    assert(TriangleLightingKey(poly,tri,nearKey)==farKey);
    tri.litCacheKey=farKey;
    s_lightingEye.x=200; //400 away: retain far within hysteresis band.
    assert(TriangleLightingKey(poly,tri,nearKey)==farKey);
    s_lightingEye.x=216; assert(TriangleLightingKey(poly,tri,nearKey)==nearKey);
    tri.litCacheKey=nearKey; s_lightingEye.x=200;
    assert(TriangleLightingKey(poly,tri,nearKey)==nearKey);
    s_lightingEye.x=0; s_worldLightingPass=false;
    assert(TriangleLightingKey(poly,tri,nearKey)==nearKey);
    s_worldLightingPass=true; s_farLightingEnabled=false;
    assert(TriangleLightingKey(poly,tri,nearKey)==nearKey);
    s_farLightingEnabled=true; vertices[0].position.x=0;
    assert(TriangleLightingKey(poly,tri,nearKey)==nearKey); //long triangle reaches eye.
    assert(!FarLighting(true,true,512*512,false));
    assert(!FarLighting(true,true,384*384,true));
    const auto limits=FarLightLimits(12,8); assert(limits.spacing==24 && limits.error==16);
    const auto user=FarLightLimits(32,24); assert(user.spacing==32 && user.error==24);
    mod::ModelSurface surf={&retained};
    ClipVertex corners[]={{{0,0,0,1},{}},{{64,0,0,1},{}},{{0,64,0,1},{}}};
    for (auto & c:corners) c.color=Color(c.pos);
    BuildCachedLitTriangle(corners,surf,0);
    const auto near=output; const auto nearSize=output.size();
    assert(std::fabs(Area()-2048)<0.01f);
    output.clear(); BuildCachedLitTriangle(corners,surf,0,0,false);
    assert(output.size()==nearSize);
    for (size_t i=0;i<output.size();++i) assert(output[i].pos.x==near[i].pos.x && output[i].pos.y==near[i].pos.y);
    output.clear(); BuildCachedLitTriangle(corners,surf,0,0,true);
    assert(output.size()<nearSize && std::fabs(Area()-2048)<0.01f);
    std::printf("Distant subdivision: %zu -> %zu triangles, identical covered area PASS\n",nearSize/3,output.size()/3);
    output.clear(); surf.samples=nullptr; BuildCachedLitTriangle(corners,surf,0,0,true);
    assert(output.size()==3);
    puts("Production LOD key: near/long faces, hysteresis, disabled/brush and limits PASS");
}
