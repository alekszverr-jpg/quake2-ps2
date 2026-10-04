#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
typedef int qboolean;
#include "client/qmenu.h"
#include "ps2/renderer/video_mode.h"
#define CVAR_ARCHIVE 1
struct Var { float value; int flags; };
static std::map<std::string,Var> vars;
static int writes, pops, draws;
static struct {int width,height;} viddef = {640,448};
static Var * Cvar_Get(const char * n,const char * d,int f) {
    if (!vars.count(n)) vars[n] = {std::strtof(d,nullptr),0};
    vars[n].flags |= f; return &vars[n];
}
static float Cvar_VariableValue(const char * n) {return vars[n].value;}
static void Cvar_SetValue(const char * n,float v) {vars[n].value=v;}
static void Cvar_Set(const char * n,const char * v) {Cvar_SetValue(n,std::strtof(v,nullptr));}
void CL_WriteConfiguration(void) {++writes;}
static void M_PopMenu() {++pops;}
static void M_Banner(const char * n) {assert(!std::strcmp(n,"m_banner_video"));}
void Menu_AddItem(menuframework_s * m,void * i) {assert(m->nitems<64);m->items[m->nitems++]=i;}
void Menu_Center(menuframework_s * m) {m->y=viddef.height/2-64;}
void Menu_AdjustCursor(menuframework_s *,int) {}
void Menu_Draw(menuframework_s * m) {assert(m->nitems==10);++draws;}
void * Menu_ItemAtCursor(menuframework_s * m) {return m->items[m->cursor];}
#include "slide.inc"
static const char * Default_MenuKey(menuframework_s * m,int k) {
    if(k==K_ESCAPE) M_PopMenu();
    else if(k==K_RIGHTARROW || k==K_LEFTARROW) Menu_SlideItem(m,k==K_RIGHTARROW?1:-1);
    return nullptr;
}
#include "client/video_menu.inc"
// Emulate libgraph's linear allocator. The production depth allocation must
// reserve every page touched by the swizzled Z16S pixel rectangle.
#define GS_PSMZ_16S 58
#define GRAPH_ALIGN_PAGE 2048
#define PS2_AssertMsg(condition,message) assert(condition)
static int allocationHeight;
static int graph_vram_allocate(int width,int height,int psm,int alignment) {
    assert(psm==GS_PSMZ_16S && alignment==2048 && width==640);
    allocationHeight=height;
    return 0;
}
#include "depth.inc"
static const int font = 1;
static const int * s_texConchars = &font;
constexpr int kGlyphSize = 8;
constexpr int kUiBrightness = 128;
namespace ps2::gs {
static video::Mode glyphMode;
static float gx,gy,gw,gh;
static int glyphCalls;
static void SetTextureFor2D(const int &) {}
static void DrawTexturedRect(int x,int y,int w,int h,int u0,int v0,int u1,int v1,int brightness) {
    assert(brightness==128 && u1-u0==8 && v1-v0==8);
    gx=video::UiX(static_cast<float>(x),glyphMode);
    gy=video::UiY(static_cast<float>(y),glyphMode);
    gw=video::UiX(static_cast<float>(w),glyphMode);
    gh=video::UiY(static_cast<float>(h),glyphMode);
    ++glyphCalls;
}
}
#include "glyph.inc"
struct model_s {};
struct player_state_t { float fov; int gunindex,gunframe; float gunoffset[3],gunangles[3]; };
struct entity_t { model_s * model; float origin[3],angles[3],oldorigin[3]; int frame,oldframe,flags; float backlerp; };
static struct { model_s * model_draw[4]; struct {float vieworg[3],viewangles[3];} refdef; float lerpfrac; } cl;
static Var gunCvar = {1,0};
static Var * cl_gun = &gunCvar;
static model_s * gun_model;
static int gun_frame, weaponCalls;
static entity_t lastWeapon;
#define RF_MINLIGHT 1
#define RF_DEPTHHACK 2
#define RF_WEAPONMODEL 4
static float LerpAngle(float a,float b,float f) {return a+(b-a)*f;}
#define VectorCopy(a,b) std::memcpy(b,a,sizeof(float)*3)
static void V_AddEntity(const entity_t * e) {lastWeapon=*e; ++weaponCalls;}
#include "weapon.inc"
int main() {
    using namespace ps2::video;
    for(bool pal:{false,true}) {
        const Mode a=Select(0,pal), l=Select(1,pal), i=Select(2,pal), p=Select(3,pal);
        assert(a.height==(pal?512:448));
        assert(l.height==224 && l.uiHeight==224 && !l.interlaced && !l.filtered);
        assert(i.height==448 && i.interlaced && i.filtered);
        assert(p.height==480 && p.uiHeight==480 && !p.interlaced && !p.filtered && p.frameMode);
        assert(!a.frameMode && !i.frameMode && !l.frameMode);
        assert(UiY(224,l)==224 && UiY(112,l)==112 && UiY(8,l)==8);
        assert(UiWidth(l)==320 && UiX(320,l)==640 && UiX(8,l)==16);
        assert(UiWidth(a)==640 && UiX(640,a)==640);
        assert(UiY(480,p)==480 && UiY(448,i)==448);
    }
    assert(Select(NAN,false).index==0 && Select(-1,true).height==512 && Select(99,false).index==0);
    for(int height:{224,448,480,512}) {
        AllocateDepthBuffer(640,height);
        const int reservedWords=640*allocationHeight/2;
        int lastPageEnd=0;
        for(int y=0;y<height;++y)
            for(int x=0;x<640;++x) {
                const int page=(y/64)*10+x/64;
                if((page+1)*2048>lastPageEnd) lastPageEnd=(page+1)*2048;
            }
        assert(reservedWords==lastPageEnd);
        if(height==224 || height==480) assert(lastPageEnd>640*height/2);
        else assert(allocationHeight==height);
    }
    for(int mode:{0,1,2,3}) {
        ps2::gs::glyphMode=Select(static_cast<float>(mode),false);
        DrawGlyph(24,32,'A');
        assert(ps2::gs::gh==8); // No font rows dropped in any output mode.
        assert(ps2::gs::gw==(mode==1?16:8));
        assert(ps2::gs::gy==32 && ps2::gs::gx==(mode==1?48:24));
    }
    const int calls=ps2::gs::glyphCalls;
    DrawGlyph(0,-8,'A'); DrawGlyph(0,0,' ');
    assert(ps2::gs::glyphCalls==calls);
    viddef.width=320; viddef.height=224;
    model_s weaponModel;
    cl.model_draw[1]=&weaponModel; cl.lerpfrac=0.5f;
    player_state_t ps={}, ops={}; ps.gunindex=1; ps.gunframe=2; ops.gunframe=1;
    for(int fov:{70,80,90,100,110,120}) {
        ps.fov=static_cast<float>(fov);
        const int before=weaponCalls;
        CL_AddViewWeapon(&ps,&ops);
        assert(weaponCalls==before+1 && lastWeapon.model==&weaponModel);
        assert(lastWeapon.flags==(RF_MINLIGHT|RF_DEPTHHACK|RF_WEAPONMODEL));
        assert(lastWeapon.frame==2 && lastWeapon.oldframe==1 && lastWeapon.backlerp==0.5f);
    }
    const int before=weaponCalls;
    gunCvar.value=0; CL_AddViewWeapon(&ps,&ops); assert(weaponCalls==before);
    gunCvar.value=1; cl.model_draw[1]=nullptr; CL_AddViewWeapon(&ps,&ops); assert(weaponCalls==before);
    viddef.width=320; viddef.height=224;
    vars["fov"]={100,2};
    PS2_VideoMenuInit(); PS2_VideoMenuDraw();
    assert(draws==1 && writes==0 && s_video_fov.curvalue==3 && vars["fov"].flags==3);
    for(const auto & v:vars) assert(v.second.flags&CVAR_ARCHIVE);
    assert(s_video_brightness.curvalue==5);
    for(int n=0;n<s_video_menu.nitems;++n) {
        const auto * item=static_cast<menucommon_s *>(s_video_menu.items[n]);
        assert(std::strlen(item->name)<=16);
    }
    s_video_menu.cursor=0; PS2_VideoMenuKey(K_RIGHTARROW);
    assert(vars["ps2_video_mode"].value==1);
    s_video_menu.cursor=1; PS2_VideoMenuKey(K_RIGHTARROW);
    assert(vars["fov"].value==110);
    s_video_menu.cursor=2; PS2_VideoMenuKey(K_RIGHTARROW);
    assert(std::fabs(vars["ps2_world_light_gamma"].value-0.6f)<0.00001f);
    for(int cursor=3;cursor<=6;++cursor) {
        s_video_menu.cursor=cursor; PS2_VideoMenuKey(K_RIGHTARROW); PS2_VideoMenuKey(K_LEFTARROW);
        assert(vars[video_cvars[cursor]].value==0);
    }
    assert(vars["ps2_world_mip_level"].value==-1);
    PS2_VideoMenuKey(K_ESCAPE); assert(writes==1 && pops==1);
    PS2_VideoMenuInit(); VideoBack(nullptr); assert(writes==1 && pops==2);
    VideoDefaults(nullptr); VideoBack(nullptr); assert(writes==2);
    for(unsigned n=0;n<9;++n) assert(vars[video_cvars[n]].value==std::strtof(video_defaults[n],nullptr));
    s_video_far_lighting.curvalue=1; VideoFarLightingChanged(nullptr);
    assert(vars["ps2_far_lighting"].value==1 && s_video_dirty);
    assert(vars["ps2_far_lighting"].flags & CVAR_ARCHIVE);
    VideoDefaults(nullptr); assert(vars["ps2_far_lighting"].value==0);
    vars["fov"].value=999; vars["ps2_world_light_gamma"].value=-999;
    PS2_VideoMenuInit(); assert(s_video_fov.curvalue==5 && s_video_brightness.curvalue==9);
    assert(vars["fov"].value==999 && vars["ps2_world_light_gamma"].value==-999);
    puts("Video menu callbacks, archival flags, Back/defaults and signal/UI modes and non-overlapping depth page tests passed");
}
