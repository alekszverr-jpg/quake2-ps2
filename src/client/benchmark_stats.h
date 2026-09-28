#ifndef CL_BENCHMARK_STATS_H
#define CL_BENCHMARK_STATS_H

/* First seven fields are microseconds; remaining fields are event counts.
 * Wait timings overlap the world/entity phases and must not be added to them. */
enum {
    BENCH_WORLD, BENCH_ENTITIES, BENCH_SETUP, BENCH_PARTICLES,
    BENCH_VU_WAIT, BENCH_TEX_DMA, BENCH_VRAM_WAIT,
    BENCH_UPLOADS, BENCH_RELOADS, BENCH_EVICTIONS, BENCH_SYNCS,
    BENCH_VERTICES, BENCH_TRIANGLES, BENCH_STATS_COUNT
};
#ifdef __cplusplus
extern "C" {
#endif
void PS2_ReadBenchmarkStats(int values[BENCH_STATS_COUNT]);
#ifdef __cplusplus
}
#endif
#endif
