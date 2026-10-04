/* Exercise the production coordinator with a fake engine/clock. */
#define CL_CLIENT_H
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "client/benchmark.h"
#include "client/benchmark_stats.h"
#include <limits.h>
enum { ca_uninitialized = 0, ca_disconnected = 1, ca_active = 2 };
static struct { int state, disable_screen, disable_servercount; } cls;
static struct { int refresh_prepped; struct { int valid, serverframe; } frame; } cl;
static struct { int width, height; } viddef;
static char drawn[8192];
static int checkCanvas;
static void DrawChar(int x, int y, int c)
{
    size_t n = strlen(drawn);
    if (checkCanvas) { assert(x >= 0 && x + 8 <= viddef.width); assert(y >= 0 && y + 8 <= viddef.height); }
    (void)x; (void)y;
    assert(n+1 < sizeof(drawn)); drawn[n] = (char)c; drawn[n+1] = 0;
}
static int fills;
static void DrawFill(int x, int y, int w, int h, int c)
{ (void)x; (void)y; (void)w; (void)h; assert(c == 0); ++fills; }
static struct { void (*DrawChar)(int, int, int); void (*DrawFill)(int, int, int, int, int); } re = { DrawChar, DrawFill };
static int now, missing, resultScreens;
static int statReads;
void PS2_ReadBenchmarkStats(int values[BENCH_STATS_COUNT])
{
    int i;
    ++statReads;
    for (i = 0; i < BENCH_STATS_COUNT; ++i) values[i] = (i + 1) * 1000;
    values[BENCH_VERTICES] = INT_MAX;
    values[BENCH_WORLD_CACHED_TRIS] = 2000;
    values[BENCH_WORLD_UNLIT_TRIS] = 1000;
    values[BENCH_WORLD_EARLY_REJECTS] = 100;
    values[BENCH_WORLD_PACKED_INSIDE] = 800;
    values[BENCH_WORLD_SAMPLE_ROOTS] = 3200;
    values[BENCH_WORLD_SAMPLE_COUNT] = 100;
    values[BENCH_WORLD_PLANE_SAMPLES] = 50;
    values[BENCH_WORLD_EMIT_SAMPLES] = 50;
    values[BENCH_WORLD_EMPTY_COUNT] = 64;
    values[BENCH_WORLD_EMPTY_PLANES] = 64;
    values[BENCH_WORLD_EMPTY_EMIT] = 128;
    values[BENCH_WORLD_EMPTY_REST] = 192;
    values[BENCH_WORLD_EMPTY_TOTAL] = 384;
}
static char queued[4096];
static float values[13] = { 0, 0, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1 };
static int Setting(const char * name)
{
    const char * names[] = { "timedemo", "paused", "ps2_show_fps", "ps2_show_memstats",
                            "ps2_show_vramstats", "ps2_show_drawstats", "developer", "con_notifytime", "ps2_world_dlights", "ps2_profile_world_lights", "ps2_world_light_cache", "ps2_profile_models", "ps2_profile_world" };
    int i;
    for (i = 0; i < 13; ++i) if (!strcmp(name, names[i])) return i;
    assert(0); return 0;
}
static float farLightingValue;
static float Cvar_VariableValue(const char * name) {
    if (!strcmp(name,"ps2_far_lighting")) return farLightingValue;
    return values[Setting(name)];
}
static void Cvar_SetValue(const char * name, float value) { values[Setting(name)] = value; }
static void Cvar_Get(const char * name, const char * value, int flags)
{ assert(flags == 0); assert(((!strcmp(name,"ps2_world_dlights") || !strcmp(name,"ps2_world_light_cache")) && !strcmp(value,"1")) || ((!strcmp(name,"ps2_profile_world_lights") || !strcmp(name,"ps2_profile_models") || !strcmp(name,"ps2_profile_world")) && !strcmp(value,"0"))); }
static void Cbuf_AddText(const char * text) { assert(strlen(queued) + strlen(text) < sizeof(queued)); strcat(queued, text); }
static void CL_Disconnect(void) { cls.state = ca_disconnected; }
static void SCR_EndLoadingPlaque(void) { cls.disable_screen = 0; }
static void M_ForceMenuOff(void) {}
void M_BenchmarkResults(void) { ++resultScreens; }
static int FS_FOpenFile(const char * name, FILE ** file)
{ (void)name; *file = missing ? NULL : tmpfile(); return *file ? 1 : -1; }
static int Sys_Milliseconds(void) { return now; }
static void Com_Printf(const char * fmt, ...) { (void)fmt; }
static void Com_sprintf(char * out, int size, const char * fmt, ...)
{ va_list args; va_start(args, fmt); vsnprintf(out, (size_t)size, fmt, args); va_end(args); }
static void Cmd_AddCommand(const char * name, void (*fn)(void)) { (void)name; (void)fn; }
#include "../../src/client/cl_benchmark.c"
/* Extracted verbatim from cl_main.c by run.py, not a mock of the drop path. */
#include "benchmark_drop.inc"

static void Frame(int begin, int end)
{ now = begin; CL_BenchmarkBeginFrame(); now = end; CL_BenchmarkEndFrame(); }
int main(void)
{
    int i;
    CL_BenchmarkInit();
    missing = 1;
    CL_BenchmarkStart();
    assert(!active && resultScreens == 1 && !queued[0]);
    missing = 0;
    CL_BenchmarkStart();
    assert(active && values[0] == 1 && values[5] == 0 && values[11] == 0);
    queued[0] = 0;
    CL_Drop(); /* SV_InitGame normally drops an already-disconnected client. */
    assert(active && values[0] == 1 && !queued[0]);
    cls.state = ca_active;
    CL_Drop(); /* A normal connected drop must also not enqueue killserver. */
    assert(active && values[0] == 1 && !queued[0]);
    for (i = 0; i < 3; ++i)
    {
        queued[0] = 0;
        cls.disable_screen = 123;
        cls.state = ca_active; cl.refresh_prepped = 0; cl.frame.valid = 1;
        Frame(0, 5000); /* loading must not count */
        assert(frames[i] == 0);
        assert(cls.disable_screen == 123);
        cl.refresh_prepped = 1;
        Frame(100, 200); /* old map frame before new serverdata */
        assert(frames[i] == 0);
        CL_BenchmarkServerData();
        cl.refresh_prepped = 0;
        Frame(300, 400);
        assert(cls.disable_screen == 123 && frames[i] == 0);
        cl.refresh_prepped = 1;
        cl.frame.serverframe = 10;
        Frame(5000, 5010);
        assert(cls.disable_screen == 0);
        Frame(5010, 5015); /* duplicate demo frame is not counted twice */
        assert(frames[i] == 1);
        cl.frame.serverframe = 11;
        Frame(5020, 5030);
        assert(frames[i] == 2 && milliseconds[i] == 30);
        assert(totals[i][BENCH_WORLD] == 2000);
        assert(totals[i][BENCH_VERTICES] == 2ULL * INT_MAX);
        assert(statReads == (i + 1) * 2);
        assert(CL_BenchmarkDemoCompleted());
        assert(CL_BenchmarkDemoCompleted()); /* duplicate EOF queues once */
        assert(!strcmp(queued, "benchmark_next\n"));
        Frame(6000, 7000); /* EOF must not include a stale rendered frame */
        assert(frames[i] == 2 && milliseconds[i] == 30);
        NextRun();
    }
    assert(!active && values[0] == 0 && values[5] == 1 && values[6] == 1 && values[7] == 3);
    assert(strstr(queued, "benchmark_results"));
    assert(!CL_BenchmarkDemoCompleted());
    CL_BenchmarkTogglePage();
    CL_BenchmarkDraw();
    assert(detailPage == 1);
    CL_BenchmarkStart();
    assert(detailPage == 0 && totals[0][BENCH_WORLD] == 0);
    CL_BenchmarkCancel();
    NextRun();
    assert(!active && values[0] == 0 && values[5] == 1 && values[6] == 1 && values[7] == 3);
    CL_BenchmarkStart();
    assert(CL_BenchmarkDemoCompleted());
    NextRun(); /* empty/corrupt demo cannot be reported as complete */
    assert(!active && strstr(status, "failed"));
    CL_BenchmarkDraw();
    assert(fills == 2);
    values[8] = 0;
    CL_BenchmarkWorldLights();
    assert(comparison && active && runLimit == 6 && values[8] == 1);
    CL_BenchmarkStart(); /* Active comparison cannot be replaced. */
    assert(comparison && runLimit == 6);
    for (i = 0; i < 6; ++i)
    {
        assert(values[8] == (i < 3 ? 1 : 0));
        cls.state = ca_active; cl.refresh_prepped = cl.frame.valid = 1;
        CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(100,110);
        cl.frame.serverframe = 11; Frame(120,i < 3 ? 140 : 130);
        assert(CL_BenchmarkDemoCompleted()); NextRun();
    }
    assert(!active && values[8] == 0 && !strncmp(status,"Complete",8));
    for (i = 0; i < 5; ++i)
    {
        drawn[0] = 0; assert(detailPage == i); CL_BenchmarkDraw();
        assert(strstr(drawn,PS2_BUILD_VERSION));
        if (!i) assert(strstr(drawn,"Light cost: +5.00 ms/frame"));
        CL_BenchmarkTogglePage();
    }
    assert(detailPage == 0);
    CL_BenchmarkWorldLights();
    /* Cancellation after switching OFF preserves the caller's original OFF. */
    for (i = 0; i < 3; ++i)
    {
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(values[8] == 0); CL_BenchmarkCancel(); assert(values[8] == 0);
    values[8] = 1;
    CL_BenchmarkWorldLights();
    Cvar_SetValue("ps2_world_dlights",0); CL_BenchmarkCancel(); assert(values[8] == 1);
    CL_BenchmarkWorldLights();
    for (i = 0; i < 6; ++i)
    {
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = i == 5 ? 9 : 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(!active && strstr(status,"ranges differ") && values[8] == 1);
    drawn[0] = 0; CL_BenchmarkDraw(); assert(!strstr(drawn,"Light cost:"));
    CL_BenchmarkStart(); assert(!comparison && values[8] == 1); CL_BenchmarkCancel();
    CL_BenchmarkLightProfile();
    assert(lightProfile && comparison && runLimit == 6 && values[9] == 1);
    for (i = 0; i < 6; ++i)
    {
        assert(values[9] == 1 && values[8] == (i < 3 ? 1 : 0));
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(!active && values[9] == 1);
    for (i = 0; i < 7; ++i)
    {
        drawn[0] = 0; assert(detailPage == i); CL_BenchmarkDraw();
        if (!i) assert(strstr(drawn,"FPS perturbed"));
        if (i >= 5) {
            assert(strstr(drawn,i == 5 ? "Light profile ON" : "Light profile OFF"));
            assert(strstr(drawn,"Select ms      14.00"));
            assert(strstr(drawn,"Surfaces     17000.00"));
            assert(strstr(drawn,"Vertex tests"));
            assert(strstr(drawn,"Color hits   26000.00"));
            assert(strstr(drawn,"Color misses 27000.00"));
        }
        CL_BenchmarkTogglePage();
    }
    assert(detailPage == 0);
    CL_BenchmarkWorldLights(); assert(values[9] == 0); CL_BenchmarkCancel(); assert(values[9] == 1);
    values[9] = 0;
    CL_BenchmarkLightProfile(); CL_BenchmarkDemoCompleted(); NextRun();
    assert(!active && values[9] == 0); // Empty demo restores diagnostics.
    CL_BenchmarkLightProfile(); assert(values[9] == 1); CL_BenchmarkCancel(); assert(values[9] == 0);
    values[8] = 0; values[9] = 1; values[10] = 0;
    CL_BenchmarkLightCache();
    assert(cacheComparison && comparison && !lightProfile && runLimit == 6);
    for (i = 0; i < 6; ++i) {
        assert(values[8] == 1 && values[9] == 0 && values[10] == (i < 3 ? 1 : 0));
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,i < 3 ? 30 : 40);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(!active && values[8] == 0 && values[9] == 1 && values[10] == 0);
    for (i = 0; i < 5; ++i) {
        drawn[0] = 0; assert(detailPage == i); CL_BenchmarkDraw();
        if (!i) {
            assert(strstr(drawn,"Color cache: 3 ON / 3 OFF"));
            assert(strstr(drawn,"Cache gain: +5.00 ms/frame"));
            assert(strstr(drawn,"World lights ON; detailed timers OFF"));
            assert(!strstr(drawn,"Light cost"));
        } else assert(strstr(drawn,i == 2 || i == 4 ? "Color cache OFF" : "Color cache ON"));
        CL_BenchmarkTogglePage();
    }
    assert(detailPage == 0);
    CL_BenchmarkLightCache(); CL_BenchmarkCancel();
    assert(values[8] == 0 && values[9] == 1 && values[10] == 0);
    CL_BenchmarkLightCache(); CL_BenchmarkDemoCompleted(); NextRun();
    assert(!active && values[8] == 0 && values[9] == 1 && values[10] == 0);
    CL_BenchmarkLightCache();
    for (i = 0; i < 6; ++i) {
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = i == 5 ? 9 : 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    drawn[0] = 0; CL_BenchmarkDraw(); assert(!strstr(drawn,"Cache gain"));
    CL_BenchmarkWorldLights(); assert(!cacheComparison && values[10] == 0); CL_BenchmarkCancel();
    values[10] = 1;
    CL_BenchmarkLightCache();
    for (i = 0; i < 3; ++i) {
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(values[10] == 0 && values[8] == 1 && values[9] == 0);
    CL_BenchmarkCancel(); assert(values[10] == 1 && values[8] == 0 && values[9] == 1);
    missing = 1; CL_BenchmarkLightCache();
    assert(!active && values[10] == 1 && values[8] == 0 && values[9] == 1);
    missing = 0;
    CL_BenchmarkModelProfile();
    assert(modelProfile && !comparison && !lightProfile && !cacheComparison && runLimit == 3);
    for (i = 0; i < 3; ++i) {
        assert(values[11] == 1 && values[8] == 0 && values[9] == 0 && values[10] == 1);
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(!active && values[11] == 1 && values[8] == 0 && values[9] == 1 && values[10] == 1);
    viddef.width = 320; viddef.height = 224;
    for (i = 0; i < 3; ++i) {
        checkCanvas = i == 2;
        drawn[0] = 0; assert(detailPage == i); CL_BenchmarkDraw();
        if (i == 0) assert(strstr(drawn, "Model timers ON"));
        if (i == 1) assert(strstr(drawn, "Left/Right: MD2 profile"));
        if (i == 2) {
            assert(strstr(drawn, "MD2 profile (incl view weapon)"));
            assert(strstr(drawn, "Setup/cull ms   28.00"));
            assert(strstr(drawn, "Batches      "));
            assert(strstr(drawn, "Tris/clip excludes Submit CPU time"));
        }
        CL_BenchmarkTogglePage();
    }
    assert(detailPage == 0);
    values[11] = 0;
    CL_BenchmarkModelProfile(); CL_BenchmarkCancel(); assert(values[11] == 0);
    CL_BenchmarkModelProfile(); CL_BenchmarkDemoCompleted(); NextRun();
    assert(!active && values[11] == 0 && values[9] == 1);
    missing = 1; CL_BenchmarkModelProfile(); assert(!active && values[11] == 0);
    missing = 0; values[11] = 1;
    CL_BenchmarkStart(); assert(values[11] == 0); CL_BenchmarkCancel(); assert(values[11] == 1);
    checkCanvas = 0;
    CL_BenchmarkWorldProfile();
    assert(worldProfile && !modelProfile && !comparison && runLimit == 3);
    for (i = 0; i < 3; ++i) {
        assert(values[12] == 1 && values[11] == 0 && values[9] == 0);
        assert(values[8] == 0 && values[10] == 1);
        cls.state = ca_active; CL_BenchmarkServerData();
        cl.frame.serverframe = 10; Frame(0,10);
        cl.frame.serverframe = 11; Frame(20,30);
        CL_BenchmarkDemoCompleted(); NextRun();
    }
    assert(!active && values[12] == 1 && values[11] == 1 && values[9] == 1);
    for (i = 0; i < 8; ++i) {
        checkCanvas = i >= 2;
        drawn[0] = 0; assert(detailPage == i); CL_BenchmarkDraw();
        if (i == 0) assert(strstr(drawn, "World timers ON"));
        if (i == 1) assert(strstr(drawn, "Left/Right: World profile"));
        if (i == 2) {
            assert(strstr(drawn, "World profile (opaque pass + sky)"));
            assert(strstr(drawn, "BSP/PVS ms      38.00"));
            assert(strstr(drawn, "Batches       45000.00"));
            assert(strstr(drawn, "Submit excluded from Sky/Geometry"));
        }
        if (i == 3) {
            assert(strstr(drawn, "Separate timer sample counts"));
            assert(strstr(drawn, "Plane roots") && strstr(drawn, "50.00"));
            assert(strstr(drawn, "Emit roots") && strstr(drawn, "50.00"));
            assert(strstr(drawn, "Root tris") && strstr(drawn, "3200.00"));
            assert(strstr(drawn, "Samples") && strstr(drawn, "100.00"));
        }
        if (i == 4) {
            assert(strstr(drawn, "BSP early reject coverage"));
            assert(strstr(drawn, "Cached tris   2000.00"));
            assert(strstr(drawn, "Early rejects  100.00"));
            assert(strstr(drawn, "Of all %         5.00"));
            assert(strstr(drawn, "Of unlit %      10.00"));
        }
        if (i == 5) {
            assert(strstr(drawn, "BSP packed inside coverage"));
            assert(strstr(drawn, "Packed inside  800.00"));
            assert(strstr(drawn, "Unlit clip     100.00"));
            assert(strstr(drawn, "Of all %        40.00"));
            assert(strstr(drawn, "Of unlit %      80.00"));
        }
        if (i == 6) {
            assert(strstr(drawn, "BSP separate planes vs emission"));
            assert(strstr(drawn, "Planes us") && strstr(drawn, "1080.00"));
            assert(strstr(drawn, "Emit us") && strstr(drawn, "1100.00"));
            assert(!strstr(drawn, "Clip rest us"));
            assert(strstr(drawn, "us/sample; no frame extrapolation."));
        }
        if (i == 7) {
            assert(strstr(drawn, "Empty single timer reference"));
            assert(strstr(drawn, "Empty planes     1.00"));
            assert(strstr(drawn, "Empty emit       2.00"));
            assert(!strstr(drawn, "Empty rest"));
        }
        CL_BenchmarkTogglePage();
    }
    assert(detailPage == 0);
    values[12] = 0;
    detailPage = 4;
    totals[0][BENCH_WORLD_CACHED_TRIS] = totals[0][BENCH_WORLD_UNLIT_TRIS] = 0;
    totals[0][BENCH_WORLD_EARLY_REJECTS] = 0;
    drawn[0] = 0; CL_BenchmarkDraw();
    assert(strstr(drawn, "Of all %         0.00") && strstr(drawn, "Of unlit %       0.00"));
    detailPage = 5; totals[0][BENCH_WORLD_PACKED_INSIDE] = 0;
    drawn[0] = 0; CL_BenchmarkDraw();
    assert(strstr(drawn, "Of all %         0.00") && strstr(drawn, "Of unlit %       0.00"));
    detailPage = 0;
    CL_BenchmarkWorldProfile(); CL_BenchmarkCancel(); assert(values[12] == 0);
    detailPage = 6; totals[0][BENCH_WORLD_PLANE_SAMPLES] = totals[0][BENCH_WORLD_EMIT_SAMPLES] = 0;
    drawn[0] = 0; CL_BenchmarkDraw();
    assert(strstr(drawn, "0.00") && !strstr(drawn, "nan") && !strstr(drawn, "inf"));
    detailPage = 7; totals[0][BENCH_WORLD_EMPTY_COUNT] = 0;
    drawn[0] = 0; CL_BenchmarkDraw();
    assert(strstr(drawn, "Empty planes     0.00") && !strstr(drawn, "nan") && !strstr(drawn, "inf"));
    detailPage = 0;
    CL_BenchmarkWorldProfile(); CL_BenchmarkDemoCompleted(); NextRun();
    assert(!active && values[12] == 0 && values[11] == 1 && values[9] == 1);
    missing = 1; CL_BenchmarkWorldProfile(); assert(!active && values[12] == 0);
    missing = 0; values[12] = 1;
    CL_BenchmarkStart(); assert(values[12] == 0); CL_BenchmarkCancel(); assert(values[12] == 1);
    farLightingValue=1; CL_BenchmarkStart(); assert(resultFarLighting);
    CL_BenchmarkCancel(); assert(farLightingValue==1);
    drawn[0]=0; CL_BenchmarkDraw(); assert(strstr(drawn,"Far light detail: reduced"));
    farLightingValue=0; CL_BenchmarkStart(); assert(!resultFarLighting);
    CL_BenchmarkCancel(); drawn[0]=0; CL_BenchmarkDraw();
    assert(strstr(drawn,"Far light detail: full"));
    puts("Benchmark lifecycle, loading exclusion, cancellation and restoration PASS");
    return 0;
}
