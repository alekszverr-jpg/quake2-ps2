#pragma once
namespace ps2::view {
inline bool FarLighting(bool enabled, bool world, float distanceSq, bool wasFar) {
    const float threshold = wasFar ? 384.0f : 512.0f;
    return enabled && world && distanceSq > threshold * threshold;
}
struct LightLodLimits { float spacing, error; };
inline LightLodLimits FarLightLimits(float spacing, float error) {
    return {spacing < 24.0f ? 24.0f : spacing, error < 16.0f ? 16.0f : error};
}
} // namespace ps2::view
