#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <limits>
using u32 = std::uint32_t;
using u16 = std::uint16_t;
#define PS2_Assert(x) assert(x)
constexpr int kMaxFineLightSubdivideDepth = 6;
constexpr int MEMTAG_MDL_WORLD = 1;
#include "vertex.inc"
namespace mod {
#include "triangle.inc"
struct ModelPoly { int numVerts; ModelTriangle * triangles; ModelPoly * next; };
struct ModelSurface { ModelPoly * polys; ModelSurface * textureChain; };
}
namespace tex { struct Texture { mod::ModelSurface * textureChain; }; }
static tex::Texture * s_chainTextures[2];
static int s_chainTextureCount;
static bool failAllocation;
static int allocations, frees;
void * PS2_MemTryAllocAligned(int, int bytes, int) {
    if (failAllocation) return nullptr;
    ++allocations;
    return std::calloc(1, static_cast<size_t>(bytes));
}
void PS2_MemFree(void * p, int, int) { ++frees; std::free(p); }
#include "cache.inc"
constexpr int full = kLitCacheChunkBytes / static_cast<int>(sizeof(CachedLitVertex));

void Retain(mod::ModelTriangle & tri, int count, int fine = 1) {
    s_litBuildFineSplits = fine;
    auto * p = AllocateLitCacheVertices(count);
    assert(p);
    tri.litCacheVertices = p;
    tri.litCacheKey = 123;
    tri.litCacheColorKey = 456;
    tri.litCacheVertexCount = tri.litCacheCapacity = static_cast<u16>(count);
    for (int i = 0; i < 3; ++i) tri.litCacheCornerIndices[i] = static_cast<u16>(i);
    p[0].packedColor = 0x12345678;
    s_cachedLitTriangles.push_back(&tri);
}
void CheckReset(const mod::ModelTriangle & tri) {
    assert(!tri.litCacheVertices && !tri.litCacheKey && !tri.litCacheColorKey);
    assert(!tri.litCacheVertexCount && !tri.litCacheCapacity);
    for (int i = 0; i < 3; ++i) assert(tri.litCacheCornerIndices[i] == 0xFFFFu);
}
void Clear() {
    ClearLitTriangleCaches();
    assert(s_cachedLitTriangles.empty() && !s_litCacheChunkCount);
    assert(!s_litCacheBytes && !s_litCacheFineSplits);
    assert(allocations == frees);
    failAllocation = false;
    s_chainTextureCount = 0;
    s_litCacheDisabled = false;
}
int main() {
    mod::ModelTriangle old[kLitCacheChunkCount] = {};
    BeginLitCacheFrame();
    for (auto & t : old) Retain(t, full, 3);
    assert(s_litCacheBytes == kLitCacheBudgetBytes);
    assert(allocations == kLitCacheChunkCount);
    s_litBuildFineSplits = 0;
    assert(!AllocateLitCacheVertices(3)); // All chunks touched this frame.
    assert(!AllocateLitCacheVertices(0) && !AllocateLitCacheVertices(full + 1));

    BeginLitCacheFrame();
    // An early missing texture must preserve a later visible texture, even if
    // the later triangle is not gathered yet. Include a linked surface/poly.
    mod::ModelPoly secondPoly{3, &old[0], nullptr};
    mod::ModelPoly firstPoly{2, nullptr, &secondPoly};
    mod::ModelSurface secondSurface{&firstPoly, nullptr};
    mod::ModelSurface firstSurface{nullptr, &secondSurface};
    tex::Texture later{&firstSurface};
    tex::Texture early{nullptr};
    s_chainTextures[0] = &early; s_chainTextures[1] = &later;
    s_chainTextureCount = 2;
    PinLitTextureChains();
    const void * protectedPointer = old[0].litCacheVertices;
    mod::ModelTriangle current{};
    Retain(current, full, 7);
    assert(old[0].litCacheVertices == protectedPointer);
    assert(static_cast<const CachedLitVertex *>(protectedPointer)[0].packedColor == 0x12345678);
    CheckReset(old[1]);
    assert(s_cachedLitTriangles.size() == kLitCacheChunkCount);
    assert(s_litCacheBytes == kLitCacheBudgetBytes);
    assert(s_litCacheFineSplits == 3 * (kLitCacheChunkCount - 1) + 7);
    assert(allocations == kLitCacheChunkCount); // Recycling has no heap growth.

    BeginLitCacheFrame();
    Retain(old[1], full); // Least recently used is old[2], not old[0]/current.
    CheckReset(old[2]);
    assert(current.litCacheVertices && old[0].litCacheVertices);
    assert(s_cachedLitTriangles.size() == kLitCacheChunkCount);
    Clear();

    // Reproduce a full old-room cache, then move into a new room. Its retained
    // results must survive repeated stationary frames and repeated room swaps.
    BeginLitCacheFrame();
    for (auto & t : old) Retain(t, full);
    mod::ModelTriangle rooms[2][32] = {};
    for (int visit = 0; visit < 48; ++visit) {
        BeginLitCacheFrame();
        auto & room = rooms[visit % 2];
        for (auto & t : room) PinLitCacheVertices(t.litCacheVertices);
        for (auto & t : room) if (!t.litCacheVertices) Retain(t, 128);
        const int bytes = s_litCacheBytes;
        for (int frame = 0; frame < 5; ++frame) {
            BeginLitCacheFrame();
            for (auto & t : room) {
                assert(t.litCacheVertices && t.litCacheKey == 123);
                PinLitCacheVertices(t.litCacheVertices);
            }
            assert(s_litCacheBytes == bytes && bytes <= kLitCacheBudgetBytes);
        }
        assert(s_cachedLitTriangles.size() <= 16 + 64);
    }
    assert(allocations - frees == kLitCacheChunkCount);
    Clear();

    // Optional allocation failure can reuse an unpinned existing chunk; it
    // cannot evict the current frame. The mandatory OOM reclaimer stays separate.
    BeginLitCacheFrame(); Retain(current, full);
    failAllocation = true;
    assert(!AllocateLitCacheVertices(3));
    BeginLitCacheFrame();
    mod::ModelTriangle replacement{};
    Retain(replacement, 3);
    CheckReset(current);
    assert(s_litCacheChunkCount == 1 && s_litCacheBytes == 3 * static_cast<int>(sizeof(CachedLitVertex)));
    Clear();

    // Reuse spare space in earlier chunks instead of treating only the tail
    // as writable. Age wrap must not permanently pin any stale chunks.
    BeginLitCacheFrame(); Retain(current, full - 3); Retain(replacement, full);
    s_litBuildFineSplits = 0;
    assert(AllocateLitCacheVertices(3) && s_litCacheChunkCount == 2);
    s_litCacheFrame = std::numeric_limits<u32>::max();
    BeginLitCacheFrame();
    assert(s_litCacheFrame == 1);
    for (int i = 0; i < s_litCacheChunkCount; ++i) assert(!s_litCacheChunks[i].lastUsedFrame);
    s_litCacheDisabled = true;
    assert(!AllocateLitCacheVertices(3));
    Clear();
    assert(!s_renderingFrame && !s_litBuildVertCount);
    (void)s_litBuildVerts;
    puts("Bounded lighting reuse, future-chain/seal protection, stationary hits, wrap and OOM passed");
}
