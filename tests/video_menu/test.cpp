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
void Menu_Draw(menuframework_s * m) {assert(m->nitems==9);++draws;}
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
int main() {
    using namespace ps2::video;
    for(bool pal:{false,true}) {
        const Mode a=Select(0,pal), l=Select(1,pal), i=Select(2,pal), p=Select(3,pal);
        assert(a.height==(pal?512:448));
        assert(l.height==224 && l.uiHeight==448 && !l.interlaced && !l.filtered);
        assert(i.height==448 && i.interlaced && i.filtered);
        assert(p.height==480 && p.uiHeight==480 && !p.interlaced && !p.filtered && p.frameMode);
        assert(!a.frameMode && !i.frameMode && !l.frameMode);
        assert(UiY(448,l)==224 && UiY(224,l)==112 && UiY(8,l)==4);
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
    vars["fov"]={100,2};
    PS2_VideoMenuInit(); PS2_VideoMenuDraw();
    assert(draws==1 && writes==0 && s_video_fov.curvalue==3 && vars["fov"].flags==3);
    for(const auto & v:vars) assert(v.second.flags&CVAR_ARCHIVE);
    assert(s_video_brightness.curvalue==5);
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
    for(unsigned n=0;n<8;++n) assert(vars[video_cvars[n]].value==std::strtof(video_defaults[n],nullptr));
    vars["fov"].value=999; vars["ps2_world_light_gamma"].value=-999;
    PS2_VideoMenuInit(); assert(s_video_fov.curvalue==5 && s_video_brightness.curvalue==9);
    assert(vars["fov"].value==999 && vars["ps2_world_light_gamma"].value==-999);
    puts("Video menu callbacks, archival flags, Back/defaults and signal/UI modes and non-overlapping depth page tests passed");
}
