#pragma once
#include <cstdint>
#include <cstring>

namespace ps2::view {
// Valid only while the light coordinate space remains fixed. Pointer collisions
// evict/recompute; clearing invalidates every entry even when addresses recur.
class SurfaceLightCache {
    struct Entry { const void * surface; std::uint32_t mask, generation; };
    Entry entries[64] = {};
    std::uint32_t generation = 1;
    static unsigned Slot(const void * surface) {
        const auto address = reinterpret_cast<std::uintptr_t>(surface);
        return static_cast<unsigned>(((address >> 4) ^ (address >> 13)) & 63u);
    }
public:
    bool Find(const void * surface, std::uint32_t & mask) const {
        const Entry & entry = entries[Slot(surface)];
        if (entry.generation != generation || entry.surface != surface) return false;
        mask = entry.mask;
        return true;
    }
    void Store(const void * surface, std::uint32_t mask) {
        entries[Slot(surface)] = {surface, mask, generation};
    }
    void Clear() {
        if (++generation == 0) {
            std::memset(entries, 0, sizeof(entries));
            generation = 1;
        }
    }
};
} // namespace ps2::view
