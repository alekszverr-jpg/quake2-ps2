#include "ps2/renderer/world_hunk.h"
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif

static std::unordered_map<void *, size_t> live;
static size_t maxChunk = 512 * 1024;
void * PS2_MemTryAllocAligned(size_t alignment, size_t bytes, PS2MemTag tag)
{
    assert(tag == MEMTAG_MDL_WORLD);
    if (bytes > maxChunk) return nullptr;
    void * result = nullptr;
#ifdef _WIN32
    result = _aligned_malloc(bytes, alignment);
    if (result == nullptr) return nullptr;
#else
    if (posix_memalign(&result, alignment, bytes) != 0) return nullptr;
#endif
    live[result] = bytes;
    return result;
}
void * PS2_MemAllocAligned(size_t alignment, size_t bytes, PS2MemTag tag)
{
    void * result = PS2_MemTryAllocAligned(alignment, bytes, tag);
    assert(result != nullptr);
    return result;
}
void PS2_MemFree(void * ptr, size_t bytes, PS2MemTag tag)
{
    assert(tag == MEMTAG_MDL_WORLD && live.at(ptr) == bytes);
    live.erase(ptr);
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

int main()
{
    using namespace ps2::mod;
    // Simulate fragmentation: no individual allocation can hold the reported
    // world hunk, but the aggregate free space can service smaller arrays.
    constexpr u32 total = 5063632;
    assert(PS2_MemTryAllocAligned(16, total, MEMTAG_MDL_WORLD) == nullptr);
    for (int pass = 0; pass < 4; ++pass)
    {
        WorldHunkBlock * owner = nullptr;
        std::vector<unsigned char *> records;
        u32 remaining = total;
        while (remaining != 0)
        {
            const u32 wanted = records.size() % 7 == 0 ? 262144u : 96u;
            const u32 bytes = remaining < wanted ? remaining : wanted;
            auto * p = static_cast<unsigned char *>(AllocWorldHunkSegment(owner, bytes, remaining));
            assert(reinterpret_cast<std::uintptr_t>(p) % 16 == 0);
            for (u32 i = 0; i < bytes; ++i) assert(p[i] == 0);
            p[0] = 42; p[bytes - 1] = 42;
            records.push_back(p);
            remaining -= bytes;
        }
        for (auto * p : records) assert(p[0] == 42); // No relocation/overlap.
        FreeWorldHunkBlocks(owner);
        assert(owner == nullptr && live.empty());
        FreeWorldHunkBlocks(owner); // Empty cleanup is safe.
    }
    // A preferred 64 KB arena may fail while the exact small record fits.
    maxChunk = 4096;
    WorldHunkBlock * owner = nullptr;
    auto * p = AllocWorldHunkSegment(owner, 96, 65536);
    assert(p != nullptr && owner->capacity == 96);
    FreeWorldHunkBlocks(owner);
    assert(live.empty());
    std::puts("Segmented BSP: alignment, zero-fill, lifetime and repeated cleanup PASS");
}
