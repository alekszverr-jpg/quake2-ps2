#include "ps2/common.h"
#include "ps2/renderer/image_load.cpp"
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <malloc.h>
static std::vector<u8> input;
static size_t allocations = 0;
int FS_LoadFile(const char *, void ** output)
{
    *output = std::malloc(input.size());
    std::memcpy(*output, input.data(), input.size());
    return static_cast<int>(input.size());
}
void FS_FreeFile(void * p) { std::free(p); }
void * PS2_MemAllocAligned(size_t align, size_t bytes, PS2MemTag)
{
    void * p = nullptr;
#ifdef _WIN32
    p = _aligned_malloc(bytes, align);
#else
    if (posix_memalign(&p, align, bytes) != 0) std::abort();
#endif
    assert(p); ++allocations; return p;
}
void PS2_MemFree(void * p, size_t, PS2MemTag)
{
    --allocations;
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}
int main()
{
    for (int depth : {24, 32}) for (int kind : {2, 10})
    {
        input.assign(18, 0); input[2] = static_cast<u8>(kind);
        input[12] = 2; input[14] = 2; input[16] = static_cast<u8>(depth);
        // RLE: a two-pixel run followed by a two-pixel raw packet.
        if (kind == 10) input.push_back(0x81);
        const int colors = kind == 10 ? 3 : 4;
        for (int i = 0; i < colors; ++i)
        {
            if (kind == 10 && i == 1) input.push_back(1);
            input.push_back(static_cast<u8>(i * 50)); input.push_back(127);
            input.push_back(255); if (depth == 32) input.push_back(33);
        }
        u8 * rgba = nullptr; u8 * rgb16 = nullptr; int w, h; bool alpha;
        assert(ps2::img::LoadTga("test", &rgba, &w, &h, &alpha));
        assert(w == 2 && h == 2 && alpha == (depth == 32));
        assert(ps2::img::LoadTga("test", &rgb16, &w, &h, &alpha, true));
        assert(!alpha);
        for (int i = 0; i < 4; ++i)
        {
            const unsigned expected = 0x8000u | ((rgba[i*4+2] >> 3) << 10) |
                ((rgba[i*4+1] >> 3) << 5) | (rgba[i*4] >> 3);
            assert((rgb16[i*2] | (rgb16[i*2+1] << 8)) == static_cast<int>(expected));
        }
        PS2_MemFree(rgba,16,MEMTAG_TEXIMAGE); PS2_MemFree(rgb16,8,MEMTAG_TEXIMAGE);
        input.resize(19); // Truncated packet/pixel data frees output on failure.
        assert(!ps2::img::LoadTga("test", &rgb16, &w, &h, &alpha, true));
        assert(allocations == 0);
    }
    std::puts("TGA direct RGB16 equivalence and failure cleanup PASS");
}
