math::Vec4 ReferenceAliasVertexColor(const math::Vec3 & modelLight,
                            const math::Vec3 & shadeVector, u8 normalIndex)
{
    constexpr int kNumAliasNormals =
        static_cast<int>(sizeof(kAliasNormals) / sizeof(kAliasNormals[0]));
    if (normalIndex >= kNumAliasNormals)
    {
        normalIndex = 0;
    }

    // This is the formula represented by ref_gl's 16-angle shadedot table:
    // dot(normal, normalize(cos(-yaw), sin(-yaw), 1)) + 1.
    const float * normal = kAliasNormals[normalIndex];
    float intensity = normal[0] * shadeVector.x +
                      normal[1] * shadeVector.y +
                      normal[2] * shadeVector.z + 1.0f;
    if (intensity < 0.0f) { intensity = 0.0f; }

    auto channel = [intensity](float light) -> float {
        float value = light * intensity;
        if (value < 0.0f) { value = 0.0f; }
        if (value > 1.0f) { value = 1.0f; }
        return value * 128.0f;
    };
    return {
        channel(modelLight.x),
        channel(modelLight.y),
        channel(modelLight.z),
        128.0f
    };
}