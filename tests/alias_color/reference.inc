    // Frozen Alpha135 vertex preparation, before per-normal colour reuse.
    PS2_Assert(md2->num_xyz <= MAX_VERTS);
    for (int vertexIndex = 0; vertexIndex < md2->num_xyz; ++vertexIndex)
    {
        const dtrivertx_t & v  = frame->verts[vertexIndex];
        const dtrivertx_t & ov = oldFrame->verts[vertexIndex];
        PreparedAliasVertex & out = s_preparedAliasVerts[vertexIndex];
        out.pos = {
            move[0] + static_cast<float>(ov.v[0]) * backScale[0]
                    + static_cast<float>(v.v[0])  * frontScale[0],
            move[1] + static_cast<float>(ov.v[1]) * backScale[1]
                    + static_cast<float>(v.v[1])  * frontScale[1],
            move[2] + static_cast<float>(ov.v[2]) * backScale[2]
                    + static_cast<float>(v.v[2])  * frontScale[2],
            1.0f
        };
        if (shell)
        {
            const math::Vec3 offset = AliasShellOffset(v.lightnormalindex);
            out.pos.x += offset.x;
            out.pos.y += offset.y;
            out.pos.z += offset.z;
        }
        math::Vec4 color = shell
            ? AliasShellVertexColor(modelLight)
            : ReferenceAliasVertexColor(modelLight, shadeVector, v.lightnormalindex);
        color.w = entityAlpha * 128.0f;
        if (clipAlias)
            out.color = color;
        else
            out.packedColor = PackFloatColor(color);
    }
