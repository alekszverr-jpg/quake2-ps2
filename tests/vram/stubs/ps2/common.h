#pragma once
#include <cassert>
#include <cstdint>
using u32 = std::uint32_t;
using u8 = std::uint8_t;
using u64 = std::uint64_t;
struct cvar_t { float value; };
inline cvar_t * Cvar_Get(const char *, const char *, int) {
    static cvar_t value={0}; return &value;
}
#define PS2_Assert(x) assert(x)
#define PS2_AssertMsg(x, message) assert(x)
#define Com_Printf(...) ((void)0)
#define Com_DPrintf(...) ((void)0)
