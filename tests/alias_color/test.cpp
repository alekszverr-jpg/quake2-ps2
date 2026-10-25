#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
constexpr int MAX_VERTS = 2048;
constexpr float POWERSUIT_SCALE = 4.0f;
namespace math {
struct Vec3 { float x, y, z; };
struct alignas(16) Vec4 { float x, y, z, w; };
}
namespace vu1 {
u32 PackColorRGBA(u32 r, u32 g, u32 b, u32 a) {
    return r | (g << 8) | (b << 16) | (a << 24);
}
}
template<class T, std::size_t N> constexpr std::size_t ArrayLength(const T (&)[N]) { return N; }
constexpr float kAliasNormals[][3] = {
#include "client/anorms.h"
};
static int shadeCalls;
#define PS2_Assert(x) assert(x)
#include "production.inc"
#include "shading_reference.inc"

struct Vertex { u8 v[3], lightnormalindex; };
using dtrivertx_t = Vertex;
struct Frame { Vertex verts[MAX_VERTS]; };
struct Header { int num_xyz; };
static PreparedAliasVertex s_preparedAliasVerts[MAX_VERTS];

#define PARAMETERS const Header * md2, const Frame * frame, const Frame * oldFrame, \
    const float * move, const float * backScale, const float * frontScale, \
    bool shell, const math::Vec3 & modelLight, const math::Vec3 & shadeVector, \
    float entityAlpha, bool clipAlias
void Reference(PARAMETERS) {
#include "reference.inc"
}
void Production(PARAMETERS) {
#include "vertices.inc"
}

int main() {
    Frame frame = {}, old = {};
    int scenarios = 0;
    const math::Vec3 lights[] = {{0,0,0},{1,1,1},{-.1f,.5f,2},{8,.3f,.00001f}};
    for (int pattern = 0; pattern < 4; ++pattern)
    for (int count : {0, 1, 7, 162, 256, MAX_VERTS})
    for (const auto & light : lights)
    for (float yaw : {-180.0f, -13.5f, 0.0f, 117.3f})
    for (float alpha : {0.0f, .01f, .5f, 1.0f})
    for (int mode = 0; mode < 3; ++mode) {
        const bool shell = mode == 2, clipAlias = mode != 0;
        const float angle = yaw * .017453292519943295f;
        const math::Vec3 shade = {std::cos(angle)*.70710678118f,
                                 std::sin(angle)*.70710678118f,.70710678118f};
        std::vector<bool> seen(162);
        int unique = 0;
        for (int i = 0; i < count; ++i) {
            const auto normal = static_cast<u8>(pattern == 0 ? 42 : pattern == 1 ? i % 8 :
                pattern == 2 ? i % 256 : (i * 73 + scenarios) % 256);
            frame.verts[i] = {{static_cast<u8>(i),static_cast<u8>(i*3),static_cast<u8>(i*7)},normal};
            old.verts[i] = {{static_cast<u8>(i*11),static_cast<u8>(i*13),static_cast<u8>(i*17)},0};
            const int n = normal < 162 ? normal : 0;
            if (!seen[n]) { seen[n] = true; ++unique; }
        }
        const Header header = {count};
        const float move[] = {-301.17f, .02f, 4096.0f};
        const float back[] = {.23f, -.19f, 0}, front[] = {.17f, 0, 1.9f};
        Reference(&header,&frame,&old,move,back,front,shell,light,shade,alpha,clipAlias);
        std::vector<PreparedAliasVertex> expected(s_preparedAliasVerts,s_preparedAliasVerts+count);
        // Poison output so a missing write cannot pass using the reference result.
        std::memset(s_preparedAliasVerts,0xa5,sizeof(s_preparedAliasVerts));
        shadeCalls = 0;
        Production(&header,&frame,&old,move,back,front,shell,light,shade,alpha,clipAlias);
        assert(shadeCalls == (shell ? 0 : unique));
        for (int i = 0; i < count; ++i) {
            const auto & actual = s_preparedAliasVerts[i];
            assert(std::memcmp(&actual.pos,&expected[i].pos,sizeof(actual.pos)) == 0);
            if (clipAlias) assert(std::memcmp(&actual.color,&expected[i].color,sizeof(actual.color)) == 0);
            else assert(actual.packedColor == expected[i].packedColor);
        }
        ++scenarios;
    }
    static_assert(sizeof(s_aliasColors)+sizeof(s_aliasColorReady) == 2754,
                  "Fixed colour cache must remain bounded");
    std::printf("MD2 colour reuse: %d scenarios, bit-exact positions/float and packed colours; per-draw invalidation and normal bounds passed\n",scenarios);
}
