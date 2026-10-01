#pragma once
#include <cstdint>
#include <cstring>

namespace ps2::view {
// Surface-local direct-mapped memoization. Exact keys preserve float coordinates,
// static RGBA and the selected light order. Collisions simply recompute.
class WorldLightCache {
    using Word = std::uint32_t;
    static_assert(sizeof(float) == sizeof(Word), "Cache requires 32-bit float coordinates");
    struct Entry { Word key[5], color, generation; };
    Entry entries[64] = {};
    Word generation = 1;
public:
    struct Key { Word words[5]; Word slot; };
    Key MakeKey(float x, float y, float z, Word color, Word mask) const {
        Key key = {};
        std::memcpy(&key.words[0], &x, sizeof(x));
        std::memcpy(&key.words[1], &y, sizeof(y));
        std::memcpy(&key.words[2], &z, sizeof(z));
        key.words[3] = color; key.words[4] = mask;
        Word hash = key.words[0] ^ (key.words[1] << 11 | key.words[1] >> 21) ^
            (key.words[2] << 22 | key.words[2] >> 10) ^ color ^ mask;
        hash ^= hash >> 16; hash *= 0x7feb352du; hash ^= hash >> 15;
        key.slot = hash & 63u;
        return key;
    }
    bool Find(const Key & key, Word & color) const {
        const Entry & entry = entries[key.slot];
        if (entry.generation != generation) return false;
        for (int i = 0; i < 5; ++i)
            if (entry.key[i] != key.words[i]) return false;
        color = entry.color;
        return true;
    }
    void Store(const Key & key, Word color) {
        Entry & entry = entries[key.slot];
        for (int i = 0; i < 5; ++i) entry.key[i] = key.words[i];
        entry.color = color; entry.generation = generation;
    }
    void Clear() {
        if (++generation == 0) {
            std::memset(entries, 0, sizeof(entries));
            generation = 1;
        }
    }
};
} // namespace ps2::view
