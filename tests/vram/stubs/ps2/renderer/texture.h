#pragma once
#include "ps2/renderer/vram.h"
namespace ps2::tex {
enum class ImageType { Null, Pic, Skin, Sprite, Wall, Sky };
// Only allocator-facing fields; no GS/DMA behaviour is simulated.
struct Texture {
    static constexpr auto kNotResident = vram::Address::Invalid;
    const char * name = "test";
    ImageType type = ImageType::Wall;
    int pixelBytes = 0;
    mutable vram::Address vramAddr = kNotResident;
    mutable bool evictedSinceUpload = false;
};
}
