#pragma once
// Signal modes use the usual overscan-safe NTSC active area: 224p / 448i.
// Low-resolution UI uses a native 320x224 canvas: retain all eight glyph rows.
namespace ps2::video {
struct Mode { int index; int height; int uiHeight; bool interlaced; bool filtered; bool frameMode; };
inline Mode Select(float requested, bool pal)
{
    const int index = requested >= 0.0f && requested <= 3.0f
        ? static_cast<int>(requested) : 0;
    switch (index) {
    case 1: return {1,224,224,false,false,false};
    case 2: return {2,448,448,true,true,false};
    case 3: return {3,480,480,false,false,true};
    default: return {0,pal ? 512 : 448,pal ? 512 : 448,true,true,false};
    }
}
inline float UiY(float y, const Mode & mode)
{
    return y * static_cast<float>(mode.height) / static_cast<float>(mode.uiHeight);
}
} // namespace ps2::video

namespace ps2::video {
inline int UiWidth(const Mode & mode) { return mode.index == 1 ? 320 : 640; }
inline float UiX(float x, const Mode & mode)
{
    return x * 640.0f / static_cast<float>(UiWidth(mode));
}
}
