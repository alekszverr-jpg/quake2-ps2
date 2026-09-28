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
static void DrawChar(int x, int y, int c) { (void)x; (void)y; (void)c; }
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
static float values[8] = { 0, 0, 1, 1, 1, 1, 1, 3 };
static int Setting(const char * name)
{
    const char * names[] = { "timedemo", "paused", "ps2_show_fps", "ps2_show_memstats",
                            "ps2_show_vramstats", "ps2_show_drawstats", "developer", "con_notifytime" };
    int i;
    for (i = 0; i < 8; ++i) if (!strcmp(name, names[i])) return i;
    assert(0); return 0;
}
static float Cvar_VariableValue(const char * name) { return values[Setting(name)]; }
static void Cvar_SetValue(const char * name, float value) { values[Setting(name)] = value; }
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
    puts("Benchmark lifecycle, loading exclusion, cancellation and restoration PASS");
    return 0;
}
