#include "client.h"
#include "benchmark.h"
#include "benchmark_stats.h"

enum { BENCH_RUNS = 6, BENCH_GROUP = 3 };
#ifndef PS2_BUILD_VERSION
#define PS2_BUILD_VERSION "unversioned"
#endif
static int lightProfile, cacheComparison, modelProfile, worldProfile;
static int comparison, runLimit = BENCH_GROUP, resultWorldLights;
static int active, pending, run, sampling, first, last;
static int frames[BENCH_RUNS], milliseconds[BENCH_RUNS];
static int ready, previousFrame;
static int firstFrame[BENCH_RUNS], lastFrame[BENCH_RUNS];
static unsigned long long totals[BENCH_RUNS][BENCH_STATS_COUNT];
static int detailPage;
static char status[80] = "Select benchmark to run demo1 three times";
static const char * const settings[] = {
    "timedemo", "paused", "ps2_show_fps", "ps2_show_memstats",
    "ps2_show_vramstats", "ps2_show_drawstats", "developer", "con_notifytime",
    "ps2_world_dlights", "ps2_profile_world_lights", "ps2_world_light_cache", "ps2_profile_models",
    "ps2_profile_world"
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
    if (++run == runLimit)
    {
        CL_Disconnect();
        Restore();
        strcpy(status, "Complete - demo1, uncapped, VSync off");
        {
            int i;
            for (i = 1; i < runLimit; ++i)
                if (frames[0] != frames[i] || firstFrame[0] != firstFrame[i] ||
                    lastFrame[0] != lastFrame[i])
                    strcpy(status, "Frame ranges differ - repeat the test");
        }
        Cbuf_AddText("killserver\nbenchmark_results\n");
        return;
    }
    first = last = ready = 0;
    if (cacheComparison)
        Cvar_SetValue("ps2_world_light_cache", run < BENCH_GROUP ? 1 : 0);
    else if (comparison)
        Cvar_SetValue("ps2_world_dlights", run < BENCH_GROUP ? 1 : 0);
    Cbuf_AddText("demomap demo1.dm2\n");
}

static void Start(int compareLights)
{
    FILE * demo = NULL;
    unsigned i;
    if (active)
        return;
    comparison = compareLights >= 1 && compareLights <= 3;
    modelProfile = compareLights == 4;
    worldProfile = compareLights == 5;
    lightProfile = compareLights == 2;
    cacheComparison = compareLights == 3;
    runLimit = comparison ? BENCH_RUNS : BENCH_GROUP;
    detailPage = 0;
    memset(totals, 0, sizeof(totals));
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
    Cvar_SetValue("ps2_world_light_cache", cacheComparison ? 1 : saved[10]);
    Cvar_SetValue("ps2_profile_world_lights", lightProfile ? 1 : 0);
    Cvar_SetValue("ps2_profile_models", modelProfile ? 1 : 0);
    Cvar_SetValue("ps2_profile_world", worldProfile ? 1 : 0);
    resultWorldLights = comparison ? 1 : saved[8] != 0;
    Cvar_SetValue("ps2_world_dlights", comparison ? 1 : saved[8]);
    memset(frames, 0, sizeof(frames));
    memset(milliseconds, 0, sizeof(milliseconds));
    active = 1;
    pending = sampling = run = first = last = ready = 0;
    memset(firstFrame, 0, sizeof(firstFrame));
    memset(lastFrame, 0, sizeof(lastFrame));
    strcpy(status, cacheComparison ? "Running: 3 cache ON, then 3 OFF..." :
        comparison ? "Running: 3 lights ON, then 3 OFF..." : "Running demo1 (3 passes)...");
    /* Explicitly leave the current session; never write a savegame. */
    Cbuf_AddText("killserver\ndemomap demo1.dm2\n");
}

void CL_BenchmarkStart(void) { Start(0); }
void CL_BenchmarkWorldLights(void) { Start(1); }
void CL_BenchmarkLightProfile(void) { Start(2); }
void CL_BenchmarkLightCache(void) { Start(3); }
void CL_BenchmarkModelProfile(void) { Start(4); }
void CL_BenchmarkWorldProfile(void) { Start(5); }

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
    int values[BENCH_STATS_COUNT], i;
    if (!sampling || !active)
        return;
    last = Sys_Milliseconds();
    if (!frames[run]) firstFrame[run] = cl.frame.serverframe;
    lastFrame[run] = previousFrame = cl.frame.serverframe;
    ++frames[run];
    milliseconds[run] = last - first;
    PS2_ReadBenchmarkStats(values);
    for (i = 0; i < BENCH_STATS_COUNT; ++i)
        if (values[i] > 0)
            totals[run][i] += (unsigned)values[i];
    sampling = 0;
}

static void Line(int y, const char * text)
{
    int i;
    int x = (viddef.width - 320) / 2 + 8;
    y += (viddef.height - 240) / 2;
    if (viddef.width <= 320) x = 0;
    for (i = 0; text[i]; ++i)
        re.DrawChar(x + i * 8, y, text[i]);
}

void CL_BenchmarkDraw(void)
{
    int i, totalFrames = 0, totalTime = 0;
    int base = comparison && (detailPage == 2 || detailPage == 4 || detailPage == 6) ? BENCH_GROUP : 0;
    char text[80];
    re.DrawFill(0, 0, viddef.width, viddef.height, 0);
    Line(25, viddef.width <= 320 ? "BENCHMARK " PS2_BUILD_VERSION : "QUAKE II - BENCHMARK " PS2_BUILD_VERSION);
    if (worldProfile && detailPage == 6)
    {
        static const char * labels[] = { "Planes us", "Emit us", "Clip rest us" };
        Line(43, "BSP sampled planes vs emission");
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = 0; i < 3; ++i)
        {
            double average[3];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j)
                average[j] = totals[j][BENCH_WORLD_SAMPLE_COUNT] ? (double)totals[j][BENCH_WORLD_PLANES_MS+i] / totals[j][BENCH_WORLD_SAMPLE_COUNT] : 0;
            Com_sprintf(text, sizeof(text), "%-13s %7.2f %7.2f %7.2f", labels[i], average[0], average[1], average[2]);
            Line(73+i*12, text);
        }
        Line(125, "us/sample; no frame extrapolation.");
        Line(137, "1/32 roots; nested work included.");
        Line(149, "Planes: cached distances + tests.");
        Line(161, "Emit: colour/UV prep + writes.");
        Line(173, "Submit excluded; clocks add overhead.");
        Line(185, "Includes seals; excludes sky/MD2.");
        Line(200, "Timers affect FPS; use normal bench.");
        Line(214, "Left/Right: pages; Back: menu");
        return;
    }
    if (worldProfile && (detailPage == 4 || detailPage == 5))
    {
        const int insidePage = detailPage == 5;
        const int countedMetric = insidePage ? BENCH_WORLD_PACKED_INSIDE : BENCH_WORLD_EARLY_REJECTS;
        static const char * rejectLabels[] = {
            "Cached tris", "No-light tris", "Early rejects", "Of all %", "Of unlit %"
        };
        static const char * insideLabels[] = {
            "Cached tris", "No-light tris", "Packed inside", "Unlit clip", "Of all %", "Of unlit %"
        };
        const char * const * labels = insidePage ? insideLabels : rejectLabels;
        Line(43, insidePage ? "BSP packed inside coverage" : "BSP early reject coverage");
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = 0; i < (insidePage ? 6 : 5); ++i)
        {
            double average[3];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j) {
                if (i < 3)
                    average[j] = frames[j] ? (double)totals[j][i == 2 ? countedMetric : BENCH_WORLD_CACHED_TRIS+i] / frames[j] : 0;
                else if (insidePage && i == 3)
                    average[j] = frames[j] ? ((double)totals[j][BENCH_WORLD_UNLIT_TRIS] -
                        (double)totals[j][BENCH_WORLD_EARLY_REJECTS] - (double)totals[j][BENCH_WORLD_PACKED_INSIDE]) / frames[j] : 0;
                else {
                    const double denominator = (double)totals[j][i == (insidePage ? 4 : 3) ? BENCH_WORLD_CACHED_TRIS : BENCH_WORLD_UNLIT_TRIS];
                    average[j] = denominator > 0 ? (double)totals[j][countedMetric] * 100.0 / denominator : 0;
                }
            }
            Com_sprintf(text, sizeof(text), "%-13s %7.2f %7.2f %7.2f", labels[i], average[0], average[1], average[2]);
            Line(73+i*12, text);
        }
        Line(149, "Counts per frame, before clipping.");
        Line(161, "No-light: no selected dynamic light.");
        Line(173, insidePage ? "Inside: all planes accept corners." : "Reject: one shared outside plane.");
        Line(185, "World only; sky/seals excluded.");
        Line(200, "Timers affect FPS; use normal bench.");
        Line(214, "Left/Right: pages; Back: menu");
        return;
    }
    if (worldProfile && detailPage == 3)
    {
        static const char * labels[] = { "Light us", "Clip/emit us", "Root tris", "Samples" };
        static const int fields[] = { BENCH_WORLD_PREP_MS, BENCH_WORLD_CLIP_MS, BENCH_WORLD_SAMPLE_ROOTS, BENCH_WORLD_SAMPLE_COUNT };
        Line(43, "Sampled triangle work (1/32)");
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = 0; i < 4; ++i)
        {
            double average[3];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j)
                average[j] = i < 2 ? (totals[j][BENCH_WORLD_SAMPLE_COUNT] ? (double)totals[j][fields[i]] / totals[j][BENCH_WORLD_SAMPLE_COUNT] : 0) :
                    (frames[j] ? (double)totals[j][fields[i]] / frames[j] : 0);
            Com_sprintf(text, sizeof(text), "%-13s %7.2f %7.2f %7.2f",
                labels[i], average[0], average[1], average[2]);
            Line(73+i*12, text);
        }
        Line(137, "us/sample; root/sample counts/frame.");
        Line(149, "Light: dynamic subdivision only.");
        Line(161, "Clip: planes, clipping + emission.");
        Line(173, "Tex/seal prep not measured here.");
        Line(185, "Submit excluded; full counters kept.");
        Line(200, "Timers affect FPS; use normal bench.");
        Line(214, "Left/Right: pages; Back: menu");
        return;
    }
    if (worldProfile && detailPage == 2)
    {
        static const char * labels[] = {
            "BSP/PVS ms", "Sky ms", "Geometry ms", "Submit ms",
            "BSP nodes", "Surfaces", "Triangles", "Batches"
        };
        Line(43, "World profile (opaque pass + sky)");
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = BENCH_WORLD_VISIBILITY_MS; i <= BENCH_WORLD_BATCHES; ++i)
        {
            double average[3];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j) {
                average[j] = frames[j] ? (double)totals[j][i] / frames[j] : 0;
                if (i <= BENCH_WORLD_SUBMIT_MS) average[j] /= 1000.0;
            }
            Com_sprintf(text, sizeof(text), "%-13s %7.2f %7.2f %7.2f",
                labels[i-BENCH_WORLD_VISIBILITY_MS], average[0], average[1], average[2]);
            Line(73+(i-BENCH_WORLD_VISIBILITY_MS)*9, text);
        }
        Line(164, "Geometry: lighting + tex prefetch.");
        Line(176, "Submit excluded from Sky/Geometry.");
        Line(188, "Submit CPU; deferred waits excluded.");
        Line(200, "Timers affect FPS; use normal bench.");
        Line(214, "Left/Right: pages; Back: menu");
        return;
    }
    if (modelProfile && detailPage == 2)
    {
        static const char * labels[] = {
            "Setup/cull ms", "Lighting ms", "Verts ms", "Tris/clip ms", "Submit ms",
            "Models", "Culled", "Unique verts", "Source tris", "Batches"
        };
        Line(43, "MD2 profile (incl view weapon)");
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = BENCH_MODEL_SETUP; i < BENCH_WORLD_VISIBILITY_MS; ++i)
        {
            double average[3];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j) {
                average[j] = frames[j] ? (double)totals[j][i] / frames[j] : 0;
                if (i <= BENCH_MODEL_SUBMIT) average[j] /= 1000.0;
            }
            Com_sprintf(text, sizeof(text), "%-13s %7.2f %7.2f %7.2f",
                labels[i-BENCH_MODEL_SETUP], average[0], average[1], average[2]);
            Line(73+(i-BENCH_MODEL_SETUP)*9, text);
        }
        Line(172, "Verts: animation + vertex color.");
        Line(184, "Tris/clip excludes Submit CPU time.");
        Line(196, "Timers affect FPS; use normal bench.");
        Line(214, "Left/Right: pages; Back: menu");
        return;
    }
    if (lightProfile && detailPage >= 5)
    {
        static const char * labels[] = {
            "Select ms", "Split ms", "Color ms", "Surfaces", "Plane tests",
            "Bounds calls", "Bounds tests", "Rejected", "Split nodes",
            "Splits", "Lit vertices", "Vertex tests", "Color hits", "Color misses"
        };
        Line(43, base ? "Light profile OFF (incl brushes)" : "Light profile ON (incl brushes)");
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = BENCH_LIGHT_SELECT; i < BENCH_MODEL_SETUP; ++i)
        {
            double average[BENCH_GROUP];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j)
            {
                average[j] = frames[base+j] ? (double)totals[base+j][i] / frames[base+j] : 0;
                if (i <= BENCH_LIGHT_COLOR) average[j] /= 1000.0;
            }
            Com_sprintf(text,sizeof(text),"%-12s %7.2f %7.2f %7.2f",
                labels[i-BENCH_LIGHT_SELECT],average[0],average[1],average[2]);
            Line(69+(i-BENCH_LIGHT_SELECT)*8,text);
        }
        Line(190,"Timers perturb FPS; use compare mode.");
        Line(202,"Color times misses; excludes lookup.");
        Line(214,"Left/Right: pages; Back: menu");
        return;
    }
    if (comparison && detailPage == 0)
    {
        int group;
        double mean[2] = {0,0};
        Line(45, status);
        Line(64, cacheComparison ? "Color cache: 3 ON / 3 OFF" : "World lights: 3 ON / 3 OFF");
        Line(83, "Mode      FPS   Frame ms  World ms");
        for (group = 0; group < 2; ++group)
        {
            unsigned long long world = 0;
            totalFrames = totalTime = 0;
            for (i = group * BENCH_GROUP; i < (group+1)*BENCH_GROUP; ++i)
            {
                totalFrames += frames[i]; totalTime += milliseconds[i];
                world += totals[i][BENCH_WORLD];
            }
            if (totalFrames && totalTime)
            {
                mean[group] = (double)totalTime / totalFrames;
                Com_sprintf(text,sizeof(text),"%-4s %8.2f %8.2f %9.2f",
                    group ? "OFF" : "ON",totalFrames*1000.0/totalTime,
                    mean[group],(double)world / totalFrames / 1000.0);
            }
            else Com_sprintf(text,sizeof(text),"%s       --",group ? "OFF" : "ON");
            Line(103+group*20,text);
        }
        if (mean[0] && mean[1] && !strncmp(status,"Complete",8))
        {
            Com_sprintf(text,sizeof(text), cacheComparison ? "Cache gain: %+.2f ms/frame" : "Light cost: %+.2f ms/frame",
                cacheComparison ? mean[1]-mean[0] : mean[0]-mean[1]);
            Line(152,text);
        }
        Line(173,cacheComparison ? "World lights ON; detailed timers OFF." : lightProfile ? "Diagnostic timers ON: FPS perturbed." : "Entity lights and styles unchanged.");
        Line(188,"Loading excluded; no extra warm-up.");
        Line(204,"Left/Right: ON/OFF runs and details");
        Line(220,"Back: return to menu");
        return;
    }
    if (comparison ? detailPage >= 3 : detailPage != 0)
    {
        static const char * labels[BENCH_LIGHT_SELECT] = {
            "World ms", "Entities ms", "Setup ms", "Particles ms",
            "VUwait ms", "TexDMA ms", "VRAMwait ms", "Uploads", "Reloads",
            "Evictions", "VRAMsync", "VU vertices", "Triangles"
        };
        Line(43, "Averages per frame (incl HUD)");
        Line(52, cacheComparison ? (base ? "Color cache OFF; world lights ON" : "Color cache ON; world lights ON") : comparison ? (base ? "World dynamic lights: OFF" : "World dynamic lights: ON") :
            (resultWorldLights ? "World dynamic lights: ON" : "World dynamic lights: OFF"));
        Line(59, "Metric          Run1    Run2    Run3");
        for (i = 0; i < BENCH_LIGHT_SELECT; ++i)
        {
            double average[3];
            int j;
            for (j = 0; j < BENCH_GROUP; ++j)
                average[j] = frames[base+j] ? (double)totals[base+j][i] / frames[base+j] : 0;
            if (i < BENCH_UPLOADS)
                for (j = 0; j < BENCH_GROUP; ++j) average[j] /= 1000.0;
            Com_sprintf(text, sizeof(text), "%-12s %7.2f %7.2f %7.2f", labels[i],
                        average[0], average[1], average[2]);
            Line(73 + i * 9, text);
        }
        Line(198, "Waits overlap phases; do not add them.");
        Line(211, worldProfile ? "Left/Right: World profile" : modelProfile ? "Left/Right: MD2 profile" : "Left/Right: FPS page");
        Line(224, "Back: return to menu");
        return;
    }
    Line(45, cacheComparison ? (base ? "Color cache OFF - world lights ON" : "Color cache ON - world lights ON") : comparison ? (base ? "World lights OFF - demo1, uncapped" : "World lights ON - demo1, uncapped") : status);
    Line(66, "Run    Frames   Seconds     FPS");
    for (i = base; i < base + BENCH_GROUP; ++i)
    {
        if (milliseconds[i] > 0)
            Com_sprintf(text, sizeof(text), "%d      %5d    %6.2f    %6.2f", i - base + 1,
                        frames[i], milliseconds[i] / 1000.0,
                        frames[i] * 1000.0 / milliseconds[i]);
        else
            Com_sprintf(text, sizeof(text), "%d      --", i - base + 1);
        Line(82 + (i-base) * 26, text);
        if (frames[i])
        {
            Com_sprintf(text, sizeof(text), "       Demo frames: %d..%d", firstFrame[i], lastFrame[i]);
            Line(94 + (i-base) * 26, text);
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
    Line(186, worldProfile ? "World timers ON: FPS perturbed." : modelProfile ? "Model timers ON: FPS perturbed." : "Loading excluded; no extra warm-up.");
    Line(200, "Left/Right: renderer details");
    Line(220, "Back: return to menu");
}

void CL_BenchmarkTogglePage(void)
{
    detailPage = (detailPage + 1) % (worldProfile ? 7 : modelProfile ? 3 : lightProfile ? 7 : comparison ? 5 : 2);
}

void CL_BenchmarkInit(void)
{
    Cvar_Get("ps2_world_dlights", "1", 0);
    Cvar_Get("ps2_profile_world_lights", "0", 0);
    Cvar_Get("ps2_world_light_cache", "1", 0);
    Cvar_Get("ps2_profile_models", "0", 0);
    Cvar_Get("ps2_profile_world", "0", 0);
    Cmd_AddCommand("benchmark_next", NextRun);
    Cmd_AddCommand("benchmark_results", M_BenchmarkResults);
}
