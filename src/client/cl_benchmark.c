#include "client.h"
#include "benchmark.h"

enum { BENCH_RUNS = 3 };
static int active, pending, run, sampling, first, last;
static int frames[BENCH_RUNS], milliseconds[BENCH_RUNS];
static int ready, previousFrame;
static int firstFrame[BENCH_RUNS], lastFrame[BENCH_RUNS];
static char status[80] = "Select benchmark to run demo1 three times";
static const char * const settings[] = {
    "timedemo", "paused", "ps2_show_fps", "ps2_show_memstats",
    "ps2_show_vramstats", "ps2_show_drawstats", "developer", "con_notifytime"
};
static float saved[sizeof(settings) / sizeof(settings[0])];

static void Restore(void)
{
    unsigned i;
    active = pending = sampling = ready = 0;
    for (i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i)
        Cvar_SetValue(settings[i], saved[i]);
}

void CL_BenchmarkCancel(void)
{
    if (!active)
        return;
    Restore();
    CL_Disconnect();
    SCR_EndLoadingPlaque();
    strcpy(status, "Cancelled - incomplete results");
    Cbuf_AddText("killserver\n");
}

static void NextRun(void)
{
    if (!active || !pending)
        return;
    pending = 0;
    if (frames[run] < 2 || milliseconds[run] <= 0)
    {
        CL_BenchmarkCancel();
        strcpy(status, "Demo failed: no measurable frames");
        Cbuf_AddText("benchmark_results\n");
        return;
    }
    Com_Printf("Benchmark demo1 run %d: %d frames, %.3f s, %.2f fps\n",
               run + 1, frames[run], milliseconds[run] / 1000.0,
               frames[run] * 1000.0 / milliseconds[run]);
    if (++run == BENCH_RUNS)
    {
        CL_Disconnect();
        Restore();
        strcpy(status, "Complete - demo1, uncapped, VSync off");
        if (frames[0] != frames[1] || frames[0] != frames[2] ||
            firstFrame[0] != firstFrame[1] || firstFrame[0] != firstFrame[2] ||
            lastFrame[0] != lastFrame[1] || lastFrame[0] != lastFrame[2])
            strcpy(status, "Frame ranges differ - repeat the test");
        Cbuf_AddText("killserver\nbenchmark_results\n");
        return;
    }
    first = last = ready = 0;
    Cbuf_AddText("demomap demo1.dm2\n");
}

void CL_BenchmarkStart(void)
{
    FILE * demo = NULL;
    unsigned i;
    if (active)
        return;
    /* Check through the virtual filesystem: the stock demo lives in pak0. */
    if (FS_FOpenFile("demos/demo1.dm2", &demo) <= 0 || !demo)
    {
        if (demo) fclose(demo);
        memset(frames, 0, sizeof(frames));
        memset(milliseconds, 0, sizeof(milliseconds));
        strcpy(status, "Missing demos/demo1.dm2 in game data");
        M_BenchmarkResults();
        return;
    }
    fclose(demo);
    CL_Disconnect();
    M_ForceMenuOff();
    for (i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i)
    {
        saved[i] = Cvar_VariableValue(settings[i]);
        Cvar_SetValue(settings[i], i == 0 ? 1 : 0);
    }
    memset(frames, 0, sizeof(frames));
    memset(milliseconds, 0, sizeof(milliseconds));
    active = 1;
    pending = sampling = run = first = last = ready = 0;
    memset(firstFrame, 0, sizeof(firstFrame));
    memset(lastFrame, 0, sizeof(lastFrame));
    strcpy(status, "Running demo1 (3 passes)...");
    /* Explicitly leave the current session; never write a savegame. */
    Cbuf_AddText("killserver\ndemomap demo1.dm2\n");
}

int CL_BenchmarkDemoCompleted(void)
{
    if (!active)
        return 0;
    if (!pending)
    {
        pending = 1;
        Cbuf_AddText("benchmark_next\n");
    }
    return 1;
}

/* A queued demomap still leaves the previous client frame valid until
 * serverdata arrives. Do not count that stale frame in the new pass. */
void CL_BenchmarkServerData(void)
{
    if (active && !pending)
    {
        ready = 1;
        previousFrame = -1;
    }
}

void CL_BenchmarkBeginFrame(void)
{
    /* Replaying the same recording repeats its servercount. The ordinary
     * connection path cannot use a changed servercount to dismiss the plaque.
     * Fresh serverdata plus a valid frame and completed registration are the
     * benchmark's readiness boundary; never dismiss it for stale/loading data. */
    if (active && ready && !pending && cls.state == ca_active &&
        cl.refresh_prepped && cl.frame.valid && cls.disable_screen)
        SCR_EndLoadingPlaque();
    sampling = active && ready && !pending && cls.state == ca_active &&
               cl.refresh_prepped && cl.frame.valid && !cls.disable_screen &&
               cl.frame.serverframe != previousFrame;
    if (sampling && !frames[run])
        first = Sys_Milliseconds();
}

void CL_BenchmarkEndFrame(void)
{
    if (!sampling || !active)
        return;
    last = Sys_Milliseconds();
    if (!frames[run]) firstFrame[run] = cl.frame.serverframe;
    lastFrame[run] = previousFrame = cl.frame.serverframe;
    ++frames[run];
    milliseconds[run] = last - first;
    sampling = 0;
}

static void Line(int y, const char * text)
{
    int i;
    int x = (viddef.width - 320) / 2 + 8;
    y += (viddef.height - 240) / 2;
    for (i = 0; text[i]; ++i)
        re.DrawChar(x + i * 8, y, text[i]);
}

void CL_BenchmarkDraw(void)
{
    int i, totalFrames = 0, totalTime = 0;
    char text[80];
    re.DrawFill(0, 0, viddef.width, viddef.height, 0);
    Line(25, "QUAKE II - BENCHMARK Alpha.79");
    Line(45, status);
    Line(66, "Run    Frames   Seconds     FPS");
    for (i = 0; i < BENCH_RUNS; ++i)
    {
        if (milliseconds[i] > 0)
            Com_sprintf(text, sizeof(text), "%d      %5d    %6.2f    %6.2f", i + 1,
                        frames[i], milliseconds[i] / 1000.0,
                        frames[i] * 1000.0 / milliseconds[i]);
        else
            Com_sprintf(text, sizeof(text), "%d      --", i + 1);
        Line(82 + i * 26, text);
        if (frames[i])
        {
            Com_sprintf(text, sizeof(text), "       Demo frames: %d..%d", firstFrame[i], lastFrame[i]);
            Line(94 + i * 26, text);
        }
        totalFrames += frames[i];
        totalTime += milliseconds[i];
    }
    if (totalTime > 0)
    {
        Com_sprintf(text, sizeof(text), "Combined: %.2f FPS / %.2f ms", totalFrames * 1000.0 / totalTime,
                    (double)totalTime / totalFrames);
        Line(166, text);
    }
    Line(186, "Loading excluded; no extra warm-up.");
    Line(200, "Compare same data and video settings.");
    Line(220, "Back: return to menu");
}

void CL_BenchmarkInit(void)
{
    Cmd_AddCommand("benchmark_next", NextRun);
    Cmd_AddCommand("benchmark_results", M_BenchmarkResults);
}
