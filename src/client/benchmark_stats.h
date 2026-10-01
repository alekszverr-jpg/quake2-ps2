#ifndef CL_BENCHMARK_STATS_H
#define CL_BENCHMARK_STATS_H

/* First seven fields and LIGHT_SELECT/SPLIT/COLOR are microseconds;
 * other fields are event counts. Light timings require diagnostic mode.
 * Wait timings overlap the world/entity phases and must not be added to them. */
enum {
    BENCH_WORLD, BENCH_ENTITIES, BENCH_SETUP, BENCH_PARTICLES,
    BENCH_VU_WAIT, BENCH_TEX_DMA, BENCH_VRAM_WAIT,
    BENCH_UPLOADS, BENCH_RELOADS, BENCH_EVICTIONS, BENCH_SYNCS,
    BENCH_VERTICES, BENCH_TRIANGLES,
    BENCH_LIGHT_SELECT, BENCH_LIGHT_SPLIT, BENCH_LIGHT_COLOR,
    BENCH_LIGHT_SURFACES, BENCH_LIGHT_SURFACE_TESTS, BENCH_LIGHT_BOUNDS,
    BENCH_LIGHT_BOUNDS_TESTS, BENCH_LIGHT_REJECTED, BENCH_LIGHT_NODES,
    BENCH_LIGHT_SPLITS, BENCH_LIGHT_VERTICES, BENCH_LIGHT_VERTEX_TESTS,
    BENCH_STATS_COUNT
};
#ifdef __cplusplus
extern "C" {
#endif
void PS2_ReadBenchmarkStats(int values[BENCH_STATS_COUNT]);
#ifdef __cplusplus
}
#endif
#endif
