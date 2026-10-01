#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u8 = std::uint8_t;
#include "alpha.inc"
#define PS2_Assert(x) assert(x)
#define PS2_STAT_ADD(x,y) ((void)0)
#define PS2_PROFILE 0
constexpr int DRAW_ENABLE=1,DRAW_DISABLE=0,ATEST_METHOD_NOTEQUAL=6,ATEST_KEEP_FRAMEBUFFER=0;
constexpr int GIF_FLG_PACKED=0,GIF_REG_AD=14,PRIM_TRIANGLE=3;
constexpr int GS_REG_TEST=0x47,GS_REG_ZBUF=0x4e,GS_REG_TEX1=0x14,GS_REG_TEX0=6;
constexpr int GS_REG_MIPTBP1=0x34,GS_REG_ALPHA=0x42,GS_REG_PRMODECONT=0x1a,GS_REG_PRIM=0;
constexpr u64 kVertexRegList=0x5e2;
u64 GS_SET_ZBUF(unsigned a,unsigned p,unsigned m) { return u64(a)|(u64(p&15)<<24)|(u64(m)<<32); }
u64 GS_SET_TEST(int,int,int,int,int,int,int z,int method) { return (u64(z)<<16)|(u64(method)<<17); }
u64 GIF_SET_TAG(u64 n,int e,int,int,int,int) { return n|(u64(e)<<15); }
u64 GS_SET_ALPHA(int,int,int c,int,int f) { return u64(c)|(u64(f)<<32); }
u64 GS_SET_PRMODECONT(int v) { return u64(v); }
u64 GIF_SET_PRIM(int,int,int,int,int a,int,int,int ctx,int) { return u64(a)|(u64(ctx)<<9); }
namespace gs {
struct { unsigned address=0x80000,zsm=0x3a; } s_zbuffer;
int DepthTestMethod() { return 2; }
#include "depth.inc"
}
namespace tex { struct Texture { int width=96,height=80; }; }
struct DrawVertex {};
u64 MakeTex1Data(const tex::Texture &) { return 11; }
u64 MakeTex0Data(const tex::Texture &) { return 12; }
u64 MakeMiptbp1Data(const tex::Texture &) { return 13; }
struct VifPacket {
    std::vector<int> unpacks;
    std::vector<std::pair<u64,u64>> writes;
    int headerWords=0,vertices=0,starts=0;
    void EnsureSpace(int) {}
    void OpenInlineUnpack(int at,bool) { unpacks.push_back(at); }
    void AddU32(u32) { ++headerWords; }
    void AddQword(u64 data,u64 reg) { writes.emplace_back(data,reg); }
    void CloseInlineUnpack() {}
    void AddUnpackData(int at,const DrawVertex *,u32 words,bool) {
        assert(at==11); vertices=int(words/2);
    }
    void AddStartProgram(int) { ++starts; }
};
#include "packet.inc"
namespace math {
struct Vec3 { float x,y,z; Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; } };
struct Vec4 { float x,y,z,w; };
struct Mat4 {};
}
struct dsprframe_t { int width,height,origin_x,origin_y; };
struct dsprite_t { int numframes; dsprframe_t frames[2]; };
struct entity_t { int frame,flags; float origin[3],alpha; };
constexpr int RF_TRANSLUCENT=32;
namespace mod {
enum class ModelType { Sprite };
struct ModelInstance { ModelType type; void * hunkBase; const tex::Texture * skins[2]; };
}
namespace tex { const Texture & DebugTexture() { static Texture t; return t; } }
int GSTextureExtent(int v) { int p=1; while(p<v) p*=2; return p; }
struct ClipVertex { math::Vec4 pos,color,st; };
static float s_rightVec[3]={1,0,0},s_upVec[3]={0,0,1};
static math::Mat4 s_viewProjMatrix;
static int s_scratchVertCount=0,clips=0,flushes=0;
static bool blended=false;
static std::vector<ClipVertex> submitted;
void SetClipDistances(ClipVertex &,const math::Mat4 &) { ++clips; }
void SubmitWorldTriangle(const ClipVertex (&c)[3],const math::Mat4 &,const tex::Texture &,bool alpha) {
    blended=alpha; submitted.insert(submitted.end(),c,c+3);
}
void FlushScratch(const math::Mat4 &,const tex::Texture &,bool) { ++flushes; }
#include "sprite.inc"
int main() {
    assert(GsTextureAlpha(0)==0 && GsTextureAlpha(255)==128);
    for(int a=0;a<256;++a) {
        assert(std::fabs(float(GsTextureAlpha(u8(a)))/128-float(a)/255)<=.5f/128+.00001f);
        if(a) assert(GsTextureAlpha(u8(a))>=GsTextureAlpha(u8(a-1)));
    }
    // Both GS contexts: opaque -> blended -> opaque must restore Z writes,
    // with tests still enabled and the original Z base/format retained.
    for(int ctx=0;ctx<2;++ctx) for(bool alpha:{false,true,false}) {
        VifPacket p;
        AddBatchChunk(p,{},ctx,nullptr,84,alpha,-1,true);
        assert(p.headerWords==4 && p.writes.size()==10 && p.starts==1 && p.vertices==84);
        assert(p.writes[0].first==8 && p.writes[0].second==GIF_REG_AD);
        assert(p.writes[1].second==u64(GS_REG_TEST+ctx) && p.writes[1].first==0x50000);
        assert(p.writes[2].second==u64(GS_REG_ZBUF+ctx));
        assert((p.writes[2].first&0xffffffffu)==0x0a000100);
        assert((p.writes[2].first>>32)==u64(alpha));
        VifPacket reuse;
        AddBatchChunk(reuse,{},ctx,nullptr,3,alpha,42,false);
        assert(reuse.writes.size()==1 && reuse.unpacks.back()==10 && reuse.vertices==3);
        // Largest chunk's entire input+output stays inside a VU half.
        assert(kVertexDataAddr+2*kMaxVertsPerBatch+10+3*kMaxVertsPerBatch<=496);
    }
    dsprite_t sprite={2,{{96,80,48,40},{96,80,12,60}}};
    tex::Texture texture;
    mod::ModelInstance model={mod::ModelType::Sprite,&sprite,{&texture,&texture}};
    entity_t e={0,0,{10,20,30},.7f};
    DrawSpriteModel(e,model);
    assert(clips==6 && submitted.size()==6 && flushes==1 && !blended);
    assert(submitted[0].pos.x==-38 && submitted[0].pos.z==-10);
    assert(submitted[2].pos.x==58 && submitted[2].pos.z==70);
    assert(submitted[0].st.y==80.0f/128 && submitted[2].st.x==96.0f/128);
    assert(submitted[0].color.w==128);
    submitted.clear(); e.frame=-1; e.flags=RF_TRANSLUCENT;
    DrawSpriteModel(e,model);
    assert(blended && submitted[0].pos.x==-2 && submitted[0].pos.z==-30);
    assert(std::fabs(submitted[0].color.w-89.6f)<.001f);
    for(float a:{-1.0f,2.0f}) {
        submitted.clear(); e.alpha=a; DrawSpriteModel(e,model);
        assert(submitted[0].color.w==(a<0?0:128));
    }
    sprite.numframes=0; auto before=submitted.size(); DrawSpriteModel(e,model);
    assert(submitted.size()==before);
    std::puts("VU packet depth mask/restoration, reused state layout and clipped NPOT sprite submission passed");
}
