#pragma once
#include <cassert>
#include <cstdint>
using u32 = std::uint32_t;
using u64 = std::uint64_t;
#define PS2_Assert(x) assert(x)
#define PS2_AssertMsg(x, message) assert(x)
#define Com_Printf(...) ((void)0)
#define Com_DPrintf(...) ((void)0)
