#pragma once
// Signal modes use the usual overscan-safe NTSC active area: 224p / 448i.
// The low-resolution mode keeps the engine/UI in 640x448 logical coordinates.
namespace ps2::video {
struct Mode { int index; int height; int uiHeight; bool interlaced; bool filtered; bool frameMode; };
inline Mode Select(float requested, bool pal)
{
    const int index = requested >= 0.0f && requested <= 3.0f
        ? static_cast<int>(requested) : 0;
    switch (index) {
    case 1: return {1,224,448,false,false,false};
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
