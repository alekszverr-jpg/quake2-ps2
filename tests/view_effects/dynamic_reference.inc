// Alpha.93 production subdivision retained for differential regression.
void ReferenceDynamicTriangle(const ClipVertex (&corners)[3],
                                  const math::Mat4 & mvp,
                                  const tex::Texture & texture, int depth = 0)
{
    const u32 surfaceMask = s_surfaceLightMask;
    u32 triangleMask = 0;
    float mins[3] = { corners[0].pos.x, corners[0].pos.y, corners[0].pos.z };
    float maxs[3] = { mins[0], mins[1], mins[2] };
    for (int v = 1; v < 3; ++v)
    {
        const float point[3] = { corners[v].pos.x, corners[v].pos.y, corners[v].pos.z };
        for (int axis = 0; axis < 3; ++axis)
        {
            mins[axis] = std::min(mins[axis], point[axis]);
            maxs[axis] = std::max(maxs[axis], point[axis]);
        }
    }
    for (int light = 0; light < s_worldLightCount; ++light)
    {
        if ((surfaceMask & (1u << light)) == 0) continue;
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
        if (squaredDistance < radius * radius) triangleMask |= 1u << light;
    }
    s_surfaceLightMask = triangleMask;
    int longest = 0;
    float longestSquared = 0.0f;
    for (int edge = 0; edge < 3; ++edge)
    {
        const math::Vec4 & a = corners[edge].pos;
        const math::Vec4 & b = corners[(edge + 1) % 3].pos;
        const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
        const float squared = x*x + y*y + z*z;
        if (squared > longestSquared) { longest = edge; longestSquared = squared; }
    }
    if (triangleMask != 0 && longestSquared > 64.0f * 64.0f && depth < 7)
    {
        const int next = (longest + 1) % 3, other = (longest + 2) % 3;
        ClipVertex midpoint = {};
        math::LerpTo(midpoint.pos, corners[longest].pos, corners[next].pos, 0.5f);
        math::LerpTo(midpoint.st, corners[longest].st, corners[next].st, 0.5f);
        math::LerpTo(midpoint.color, corners[longest].color, corners[next].color, 0.5f);
        SetClipDistances(midpoint, mvp);
        const ClipVertex first[3] = { corners[longest], midpoint, corners[other] };
        const ClipVertex second[3] = { midpoint, corners[next], corners[other] };
        ReferenceDynamicTriangle(first, mvp, texture, depth + 1);
        ReferenceDynamicTriangle(second, mvp, texture, depth + 1);
    }
    else
    {
        SubmitWorldTriangle(corners, mvp, texture);
    }
    s_surfaceLightMask = surfaceMask;
}
