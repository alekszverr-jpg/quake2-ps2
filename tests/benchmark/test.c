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
static void DrawChar(int x, int y, int c)
{
    size_t n = strlen(drawn);
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
}
static char queued[4096];
static float values[10] = { 0, 0, 1, 1, 1, 1, 1, 3, 1, 1 };
static int Setting(const char * name)
{
    const char * names[] = { "timedemo", "paused", "ps2_show_fps", "ps2_show_memstats",
                            "ps2_show_vramstats", "ps2_show_drawstats", "developer", "con_notifytime", "ps2_world_dlights", "ps2_profile_world_lights" };
    int i;
    for (i = 0; i < 10; ++i) if (!strcmp(name, names[i])) return i;
    assert(0); return 0;
}
static float Cvar_VariableValue(const char * name) { return values[Setting(name)]; }
static void Cvar_SetValue(const char * name, float value) { values[Setting(name)] = value; }
static void Cvar_Get(const char * name, const char * value, int flags)
{ assert(flags == 0); assert((!strcmp(name,"ps2_world_dlights") && !strcmp(value,"1")) || (!strcmp(name,"ps2_profile_world_lights") && !strcmp(value,"0"))); }
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
    assert(active && values[0] == 1 && values[5] == 0);
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
        }
        CL_BenchmarkTogglePage();
    }
    assert(detailPage == 0);
    CL_BenchmarkWorldLights(); assert(values[9] == 0); CL_BenchmarkCancel(); assert(values[9] == 1);
    values[9] = 0;
    CL_BenchmarkLightProfile(); CL_BenchmarkDemoCompleted(); NextRun();
    assert(!active && values[9] == 0); // Empty demo restores diagnostics.
    CL_BenchmarkLightProfile(); assert(values[9] == 1); CL_BenchmarkCancel(); assert(values[9] == 0);
    puts("Benchmark lifecycle, loading exclusion, cancellation and restoration PASS");
    return 0;
}
