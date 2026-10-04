/* PS2 live video settings, included by menu.c to use its navigation helpers. */
static menuframework_s s_video_menu;
static menuslider_s s_video_brightness;
static menulist_s s_video_mode, s_video_fov, s_video_toggles[4];
static menulist_s s_video_far_lighting;
static menuaction_s s_video_defaults, s_video_back;
static int s_video_dirty;
static const char * const video_cvars[] = {
    "ps2_video_mode", "fov", "ps2_world_light_gamma", "ps2_world_dlights",
    "ps2_world_mipmaps", "gl_polyblend", "ps2_show_fps", "ps2_world_mip_level", "ps2_far_lighting"
};
static const char * const video_defaults[] = {"0","90","0.70","1","1","1","0","-1","0"};
void CL_WriteConfiguration(void);
void PS2_VideoMenuInit(void);

static float VideoClamp(float value, float low, float high)
{
    if (!(value >= low)) return low;
    return value > high ? high : value;
}
static void VideoSave(void)
{
    if (s_video_dirty) { CL_WriteConfiguration(); s_video_dirty = 0; }
}
static void VideoModeChanged(void * unused)
{
    (void)unused;
    Cvar_SetValue("ps2_video_mode", s_video_mode.curvalue); s_video_dirty = 1;
}
static void VideoBrightnessChanged(void * unused)
{
    (void)unused;
    Cvar_SetValue("ps2_world_light_gamma", 1.2f - s_video_brightness.curvalue * 0.1f);
    s_video_dirty = 1;
}
static void VideoFovChanged(void * unused)
{
    (void)unused;
    Cvar_SetValue("fov", 70 + s_video_fov.curvalue * 10); s_video_dirty = 1;
}
static void VideoToggleChanged(void * self)
{
    menulist_s * item = (menulist_s *)self;
    Cvar_SetValue(video_cvars[3 + item->generic.localdata[0]], item->curvalue);
    if (item->generic.localdata[0] == 1) Cvar_SetValue("ps2_world_mip_level", -1);
    s_video_dirty = 1;
}
static void VideoFarLightingChanged(void * unused)
{
    (void)unused;
    Cvar_SetValue("ps2_far_lighting", s_video_far_lighting.curvalue);
    s_video_dirty = 1;
}
static void VideoDefaults(void * unused)
{
    unsigned i;
    (void)unused;
    for (i = 0; i < sizeof(video_cvars)/sizeof(video_cvars[0]); ++i)
        Cvar_Set(video_cvars[i], video_defaults[i]);
    PS2_VideoMenuInit(); s_video_dirty = 1;
}
static void VideoBack(void * unused)
{
    (void)unused; VideoSave(); M_PopMenu();
}
void PS2_VideoMenuInit(void)
{
    static const char * yesno[] = {"off","on",NULL};
    static const char * farDetails[] = {"full","reduced",NULL};
    static const char * modes[] = {"auto (NTSC/PAL)","240p (640x224)","480i (640x448)","480p (640x480)",NULL};
    static const char * angles[] = {"70","80","90","100","110","120",NULL};
    static const char * names[] = {"world lighting","mipmapping","color effects","show FPS"};
    static const char * hints[] = {
        "Projectile light on walls and doors",
        "Reduces texture shimmer in the distance",
        "Water, damage and pickup color flashes",
        "Display the FPS counter during gameplay"
    };
    unsigned i;
    s_video_dirty = 0;
    memset(&s_video_menu,0,sizeof(s_video_menu));
    memset(&s_video_mode,0,sizeof(s_video_mode));
    memset(&s_video_brightness,0,sizeof(s_video_brightness));
    memset(&s_video_fov,0,sizeof(s_video_fov));
    memset(s_video_toggles,0,sizeof(s_video_toggles));
    memset(&s_video_far_lighting,0,sizeof(s_video_far_lighting));
    memset(&s_video_defaults,0,sizeof(s_video_defaults));
    memset(&s_video_back,0,sizeof(s_video_back));
    for (i = 0; i < sizeof(video_cvars)/sizeof(video_cvars[0]); ++i)
        Cvar_Get(video_cvars[i],video_defaults[i],CVAR_ARCHIVE);
    s_video_menu.x = viddef.width/2;
    s_video_menu.statusbar = "Left/Right: change; Back: save";
    s_video_mode.generic.type = MTYPE_SPINCONTROL;
    s_video_mode.generic.name = "video mode";
    s_video_mode.generic.callback = VideoModeChanged;
    s_video_mode.itemnames = modes;
    s_video_mode.curvalue = (int)VideoClamp(Cvar_VariableValue("ps2_video_mode"),0,3);
    s_video_mode.generic.statusbar = "Back: save; restart game to apply";
    Menu_AddItem(&s_video_menu,&s_video_mode);
    s_video_fov.generic.type = MTYPE_SPINCONTROL;
    s_video_fov.generic.name = "field of view"; s_video_fov.generic.y = 12;
    s_video_fov.generic.callback = VideoFovChanged;
    s_video_fov.itemnames = angles;
    s_video_fov.curvalue = (int)VideoClamp((Cvar_VariableValue("fov")-70)/10+0.5f,0,5);
    s_video_fov.generic.statusbar = "Horizontal viewing angle";
    Menu_AddItem(&s_video_menu,&s_video_fov);
    s_video_brightness.generic.type = MTYPE_SLIDER;
    s_video_brightness.generic.name = "world brightness"; s_video_brightness.generic.y = 24;
    s_video_brightness.generic.callback = VideoBrightnessChanged;
    s_video_brightness.minvalue = 0; s_video_brightness.maxvalue = 9;
    s_video_brightness.curvalue = (float)(int)VideoClamp((1.2f-Cvar_VariableValue("ps2_world_light_gamma"))*10+0.5f,0,9);
    s_video_brightness.generic.statusbar = "Brightness of walls and floors";
    Menu_AddItem(&s_video_menu,&s_video_brightness);
    for (i = 0; i < 4; ++i) {
        menulist_s * item = &s_video_toggles[i];
        item->generic.type = MTYPE_SPINCONTROL; item->generic.name = names[i];
        item->generic.y = 36+(int)i*12; item->generic.localdata[0] = (int)i;
        item->generic.callback = VideoToggleChanged; item->generic.statusbar = hints[i];
        item->itemnames = yesno; item->curvalue = Cvar_VariableValue(video_cvars[3+i]) != 0;
        Menu_AddItem(&s_video_menu,item);
    }
    s_video_far_lighting.generic.type = MTYPE_SPINCONTROL;
    s_video_far_lighting.generic.name = "far light detail";
    s_video_far_lighting.generic.y = 84;
    s_video_far_lighting.generic.callback = VideoFarLightingChanged;
    s_video_far_lighting.generic.statusbar = "Experimental: coarser distant wall lighting";
    s_video_far_lighting.itemnames = farDetails;
    s_video_far_lighting.curvalue = Cvar_VariableValue("ps2_far_lighting") != 0;
    Menu_AddItem(&s_video_menu,&s_video_far_lighting);
    s_video_defaults.generic.type = MTYPE_ACTION;
    s_video_defaults.generic.name = "restore defaults"; s_video_defaults.generic.y = 100;
    s_video_defaults.generic.callback = VideoDefaults;
    Menu_AddItem(&s_video_menu,&s_video_defaults);
    s_video_back.generic.type = MTYPE_ACTION;
    s_video_back.generic.name = "back"; s_video_back.generic.y = 112;
    s_video_back.generic.callback = VideoBack;
    Menu_AddItem(&s_video_menu,&s_video_back);
    Menu_Center(&s_video_menu);
}
void PS2_VideoMenuDraw(void)
{
    M_Banner("m_banner_video"); Menu_AdjustCursor(&s_video_menu,1); Menu_Draw(&s_video_menu);
}
const char * PS2_VideoMenuKey(int key)
{
    if (key == K_ESCAPE) VideoSave();
    return Default_MenuKey(&s_video_menu,key);
}
