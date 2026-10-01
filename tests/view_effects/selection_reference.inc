// Frozen Alpha.97 production selection for differential regression.
u32 ReferenceSelectTriangleLights(u32 mask, const math::Vec4 (&positions)[3])
{
    WorldLightScope timer(s_lightProfile, WorldLightProfile::Select);
    if (s_lightProfile.enabled) ++s_lightProfile.bounds;
    float mins[3] = { positions[0].x, positions[0].y, positions[0].z };
    float maxs[3] = { mins[0], mins[1], mins[2] };
    for (int v = 1; v < 3; ++v)
    {
        const float point[3] = { positions[v].x, positions[v].y, positions[v].z };
        for (int axis = 0; axis < 3; ++axis)
        {
            mins[axis] = std::min(mins[axis], point[axis]);
            maxs[axis] = std::max(maxs[axis], point[axis]);
        }
    }
    u32 result = 0;
    while (mask != 0)
    {
        if (s_lightProfile.enabled) ++s_lightProfile.boundsTests;
        const int light = __builtin_ctz(mask);
        mask &= mask - 1u;
        const float origin[3] = { s_worldLightOrigins[light].x,
            s_worldLightOrigins[light].y, s_worldLightOrigins[light].z };
        float squaredDistance = 0.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float nearest = std::max(mins[axis], std::min(maxs[axis], origin[axis]));
            const float delta = origin[axis] - nearest;
            squaredDistance += delta * delta;
        }
        const float radius = s_worldLights[light].intensity;
        if (squaredDistance < radius * radius) result |= 1u << light;
        else if (s_lightProfile.enabled) ++s_lightProfile.rejected;
    }
    return result;
}
