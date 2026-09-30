#include <cassert>
#include <cstdio>
#include <vector>
#include "ps2/system/heap.h"
static PS2MemoryReclaimer s_memoryReclaimer;
static bool s_reclaimingMemory;
static const char * s_memTagNames[MEMTAG_COUNT] = {};
static bool available;
static int attempts, accounted, releases, callbacks;
alignas(128) static char allocation[128];
void * dlmalloc(size_t) { ++attempts; return available ? allocation : nullptr; }
void * dlmemalign(size_t, size_t n) { return dlmalloc(n); }
size_t MemTagToIndex(PS2MemTag tag) { return tag; }
void AccountAlloc(PS2MemTag, size_t) { ++accounted; }
const char * PS2_DumpMemTags() { return "fixture"; }
void Sys_Error(const char *, ...) { throw 1; }
namespace mod {
struct ModelTriangle {
    mutable void * litCacheVertices;
    mutable int litCacheKey, litCacheColorKey, litCacheVertexCount, litCacheCapacity;
};
}
static std::vector<const mod::ModelTriangle *> s_cachedLitTriangles;
struct Chunk { void * vertices; int usedVertexCount; };
static Chunk s_litCacheChunks[2];
static int s_litCacheChunkCount, s_litCacheBytes, s_litCacheFineSplits;
static bool s_litCacheDisabled, s_renderingFrame;
constexpr int kLitCacheChunkBytes = 96 * 1024;
void PS2_MemFree(void * ptr, size_t bytes, PS2MemTag tag) {
    assert(ptr && bytes == kLitCacheChunkBytes && tag == MEMTAG_MDL_WORLD);
    ++releases; available = true;
}
#include "reclaim.inc"
static mod::ModelTriangle triangle;
static void Populate() {
    triangle = {allocation, 1, 2, 3, 4};
    s_cachedLitTriangles = {&triangle};
    s_litCacheChunks[0] = {allocation, 3};
    s_litCacheChunkCount = 1;
    s_litCacheBytes = 144; s_litCacheFineSplits = 2;
    s_litCacheDisabled = false; available = false;
    attempts = accounted = releases = callbacks = 0;
}
static int Callback() { ++callbacks; return ReclaimLightingCache(); }
int main() {
    PS2_SetMemoryReclaimer(Callback);
    Populate();
    assert(PS2_MemAlloc(60356, MEMTAG_QUAKE) == allocation);
    assert(attempts == 2 && accounted == 1 && releases == 1 && callbacks == 1);
    assert(s_cachedLitTriangles.empty() && !triangle.litCacheVertices);
    assert(!triangle.litCacheKey && !triangle.litCacheColorKey && !triangle.litCacheVertexCount && !triangle.litCacheCapacity);
    assert(!s_litCacheChunkCount && !s_litCacheBytes && !s_litCacheFineSplits && s_litCacheDisabled);
    assert(!ReclaimLightingCache());
    Populate();
    assert(PS2_MemAllocAligned(128, 65536, MEMTAG_TEXIMAGE) == allocation);
    assert(attempts == 2 && accounted == 1 && releases == 1);
    Populate();
    assert(!PS2_MemTryAllocAligned(16, 98304, MEMTAG_MDL_WORLD));
    assert(attempts == 1 && !callbacks && !accounted && !releases);
    s_renderingFrame = true;
    try { PS2_MemAlloc(60356, MEMTAG_QUAKE); assert(false); } catch (int) {}
    assert(attempts == 2 && callbacks == 1 && !releases && triangle.litCacheVertices);
    s_renderingFrame = false;
    PS2_SetMemoryReclaimer(nullptr);
    attempts = 0;
    try { PS2_MemAlloc(60356, MEMTAG_QUAKE); assert(false); } catch (int) {}
    assert(attempts == 1);
    ClearLitTriangleCaches();
    puts("Mandatory retry, optional allocation and safe lighting reclamation passed");
}
