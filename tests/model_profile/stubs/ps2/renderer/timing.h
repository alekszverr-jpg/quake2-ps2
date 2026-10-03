#pragma once
#define CLOCKS_PER_SEC 1000000
namespace ps2::timing {
using Stamp = long long;
inline Stamp now = 0;
inline int reads = 0;
inline Stamp Now() { ++reads; return now; }
}
