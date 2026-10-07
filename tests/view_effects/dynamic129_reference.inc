// Alpha129 production recursion, frozen for exact differential checks.
void Reference129DynamicVertices(const ClipVertex & a, const ClipVertex & b,
                                  const ClipVertex & c,
                                  const math::Mat4 & mvp,
                                  const tex::Texture & texture, int depth = 0,
                                  bool boundsSelected = false,
                                  float edgeLimitSquared = 64.0f*64.0f)
{
    const ClipVertex * corners[3] = { &a, &b, &c };
    WorldDetailScope lightTimer(s_worldProfile, WorldProfile::Preparation);
    if (s_lightProfile.enabled) ++s_lightProfile.nodes;
    const u32 surfaceMask = s_surfaceLightMask;
    if (surfaceMask == 0)
    {
        SubmitDynamicLeaf(a, b, c, mvp, texture);
        return;
    }
    u32 triangleMask = surfaceMask;
    if (!boundsSelected)
    {
        const math::Vec4 positions[3] = { corners[0]->pos, corners[1]->pos, corners[2]->pos };
        triangleMask = SelectTriangleLights(surfaceMask, positions);
    }
    s_surfaceLightMask = triangleMask;
    if (triangleMask == 0)
    {
        SubmitDynamicLeaf(a, b, c, mvp, texture);
        s_surfaceLightMask = surfaceMask;
        return;
    }
    WorldLightScope splitTimer(s_lightProfile, WorldLightProfile::Split);
    if (depth == 0) edgeLimitSquared = DynamicTriangleEdgeSquared(a, b, c);
    int longest = 0;
    float longestSquared = 0.0f;
    for (int edge = 0; edge < 3; ++edge)
    {
        const math::Vec4 & edgeA = corners[edge]->pos;
        const math::Vec4 & edgeB = corners[(edge + 1) % 3]->pos;
        const float x = edgeA.x - edgeB.x, y = edgeA.y - edgeB.y, z = edgeA.z - edgeB.z;
        const float squared = x*x + y*y + z*z;
        ++referenceEdgeEvaluations;
        if (squared > longestSquared) { longest = edge; longestSquared = squared; }
    }
    if (triangleMask != 0 && longestSquared > edgeLimitSquared && depth < 7)
    {
        if (s_lightProfile.enabled) ++s_lightProfile.splits;
        const int next = (longest + 1) % 3, other = (longest + 2) % 3;
        ClipVertex midpoint = {};
        math::LerpTo(midpoint.pos, corners[longest]->pos, corners[next]->pos, 0.5f);
        math::LerpTo(midpoint.st, corners[longest]->st, corners[next]->st, 0.5f);
        math::LerpTo(midpoint.color, corners[longest]->color, corners[next]->color, 0.5f);
        SetClipDistances(midpoint, mvp);
        splitTimer.Stop(); // Exclude children, bounds tests and submission.
        Reference129DynamicVertices(*corners[longest], midpoint, *corners[other],
                                     mvp, texture, depth + 1, false, edgeLimitSquared);
        Reference129DynamicVertices(midpoint, *corners[next], *corners[other],
                                     mvp, texture, depth + 1, false, edgeLimitSquared);
    }
    else
    {
        splitTimer.Stop();
        SubmitDynamicLeaf(a, b, c, mvp, texture);
    }
    s_surfaceLightMask = surfaceMask;
}
