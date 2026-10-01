#ifndef CL_BENCHMARK_H
#define CL_BENCHMARK_H

/* Local demo benchmark; no game data or frame history is retained. */
void CL_BenchmarkInit(void);
void CL_BenchmarkStart(void);
void CL_BenchmarkWorldLights(void);
void CL_BenchmarkCancel(void);
int CL_BenchmarkDemoCompleted(void);
void CL_BenchmarkServerData(void);
void CL_BenchmarkBeginFrame(void);
void CL_BenchmarkEndFrame(void);
void CL_BenchmarkDraw(void);
void CL_BenchmarkTogglePage(void);
void M_BenchmarkResults(void);

#endif
