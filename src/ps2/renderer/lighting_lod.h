#pragma once
namespace ps2::view {
inline bool FarLighting(bool enabled, bool world, float distanceSq, bool wasFar) {
    const float threshold = wasFar ? 384.0f : 512.0f;
    return enabled && world && distanceSq > threshold * threshold;
}
struct LightLodLimits { float spacing, error; };
inline float DynamicLightSpacing(float distanceSq) {
    constexpr float nearSq = 384.0f*384.0f, farSq = 768.0f*768.0f;
    if (!(distanceSq > nearSq)) return 64.0f;
    if (distanceSq >= farSq) return 128.0f;
    return 64.0f + 64.0f * ((distanceSq-nearSq)/(farSq-nearSq));
}
inline LightLodLimits FarLightLimits(float spacing, float error) {
    return {spacing < 24.0f ? 24.0f : spacing, error < 16.0f ? 16.0f : error};
}
} // namespace ps2::view
