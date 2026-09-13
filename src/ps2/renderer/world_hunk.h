#pragma once

#include "ps2/common.h"
#include "ps2/system/heap.h"
#include <cstring>

namespace ps2::mod {

// Only the world owns these blocks. Inline models borrow the geometry.
struct alignas(16) WorldHunkBlock
{
    WorldHunkBlock * next;
    u32 capacity;
    u32 used;
};

inline void FreeWorldHunkBlocks(WorldHunkBlock * & head)
{
    while (head != nullptr)
    {
        WorldHunkBlock * block = head;
        head = block->next;
        PS2_MemFree(block, sizeof(WorldHunkBlock) + block->capacity, MEMTAG_MDL_WORLD);
    }
}

// Fallback arena: each loader array remains contiguous, but unrelated arrays
// and polygon records need not share one multi-megabyte allocation.
inline void * AllocWorldHunkSegment(WorldHunkBlock * & head, u32 bytes, u32 remaining)
{
    PS2_Assert((bytes & 15u) == 0 && bytes <= remaining);
    if (head == nullptr || head->capacity - head->used < bytes)
    {
        u32 capacity = remaining < 65536u ? remaining : 65536u;
        if (capacity < bytes) { capacity = bytes; }
        if (capacity == 0) { capacity = 16; } // Preserve valid zero-size allocations.
        auto * block = static_cast<WorldHunkBlock *>(PS2_MemTryAllocAligned(
            16, sizeof(WorldHunkBlock) + capacity, MEMTAG_MDL_WORLD));
        if (block == nullptr)
        {
            // A smaller gap can still hold this array when the preferred arena
            // chunk cannot fit. Required allocations retain the fatal OOM path.
            capacity = bytes != 0 ? bytes : 16;
            block = static_cast<WorldHunkBlock *>(PS2_MemAllocAligned(
                16, sizeof(WorldHunkBlock) + capacity, MEMTAG_MDL_WORLD));
        }
        block->next = head;
        block->capacity = capacity;
        block->used = 0;
        std::memset(block + 1, 0, capacity);
        head = block;
    }
    auto * result = reinterpret_cast<unsigned char *>(head + 1) + head->used;
    head->used += bytes;
    return result;
}

} // namespace ps2::mod
