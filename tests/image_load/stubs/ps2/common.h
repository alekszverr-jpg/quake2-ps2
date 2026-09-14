#pragma once
#include <cassert>
#include <cstdint>
#include <cstddef>
#include "ps2/system/heap.h"
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using byte = unsigned char;
#define PS2_Assert(x) assert(x)
#define Com_DPrintf(...) ((void)0)
int FS_LoadFile(const char *, void **);
void FS_FreeFile(void *);
