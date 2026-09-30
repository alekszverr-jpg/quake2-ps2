// Allocation-free view effects shared by the renderer and host regression tests.
#pragma once
#include <cmath>
#include <cstdint>

namespace ps2::view::effects {

inline void UnderwaterFov(float time, float & x, float & y)
{
    const float wave = std::sin(time * 4.0f);
    x += wave;
    y -= wave;
}

inline std::uint32_t AddLight(std::uint32_t packed, float distanceSquared,
                              float intensity, const float * color)
{
    if (intensity <= 0.0f || distanceSquared >= intensity * intensity)
        return packed;
    // Match the entity-light scale: intensity/distance in Quake units,
    // converted from normalized RGB to GS modulation (128 = neutral).
    const float amount = (intensity - std::sqrt(distanceSquared)) * 0.5f;
    std::uint32_t result = packed & 0xFF000000u;
    for (int channel = 0; channel < 3; ++channel)
    {
        float value = static_cast<float>((packed >> (channel * 8)) & 255u)
                    + amount * color[channel];
        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;
        result |= static_cast<std::uint32_t>(value + 0.5f) << (channel * 8);
    }
    return result;
}

// Six-sided tube as in ref_gl; ring points are in world space. Degenerate
// endpoints are ignored rather than producing NaNs in the clipping pipeline.
inline bool BeamRing(const float * start, const float * end, int diameter,
                     float (&points)[12][3])
{
    float dir[3];
    float lengthSquared = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        dir[axis] = end[axis] - start[axis];
        lengthSquared += dir[axis] * dir[axis];
    }
    if (lengthSquared <= 0.0f || diameter < 2) return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    for (float & component : dir) component *= inverseLength;
    int axis = 0;
    if (std::fabs(dir[1]) < std::fabs(dir[axis])) axis = 1;
    if (std::fabs(dir[2]) < std::fabs(dir[axis])) axis = 2;
    float perpendicular[3];
    float perpendicularLength = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        perpendicular[i] = (i == axis ? 1.0f : 0.0f) - dir[axis] * dir[i];
        perpendicularLength += perpendicular[i] * perpendicular[i];
    }
    const float scale = static_cast<float>(diameter / 2) / std::sqrt(perpendicularLength);
    for (float & component : perpendicular) component *= scale;
    const float second[3] = {
        dir[1] * perpendicular[2] - dir[2] * perpendicular[1],
        dir[2] * perpendicular[0] - dir[0] * perpendicular[2],
        dir[0] * perpendicular[1] - dir[1] * perpendicular[0]
    };
    for (int side = 0; side < 6; ++side)
    {
        const float angle = static_cast<float>(side) * 1.0471975512f;
        const float cosine = std::cos(angle), sine = std::sin(angle);
        for (int i = 0; i < 3; ++i)
        {
            const float offset = cosine * perpendicular[i] + sine * second[i];
            points[side][i] = start[i] + offset;
            points[side + 6][i] = end[i] + offset;
        }
    }
    return true;
}
} // namespace ps2::view::effects
