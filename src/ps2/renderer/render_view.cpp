/* ================================================================================================
 * File: render_view.cpp
 * Brief: View/3D frame rendering: the world geometry pass behind PS2_RenderFrame.
 *
 *  RenderFrame walks the world BSP for the refdef's camera: MarkLeaves stamps the
 *  nodes reachable from the current PVS cluster, RecursiveWorldNode descends the
 *  tree front-to-back culling against the view frustum and threads every visible
 *  opaque surface onto its texture's draw chain, and DrawTextureChains then
 *  gathers each chain's triangles into a scratch buffer and submits them through
 *  vu1::DrawTriangles - one synchronous batch per texture. Sky and translucent
 *  surfaces are routed aside for later passes (skybox / alpha-blend milestones).
 *
 *  Camera mapping: Quake is Z-up with AngleVectors giving forward/right/up; those
 *  feed math::LookAt directly (its right = cross(up, -forward) lands on Quake's
 *  own right vector) and PerspectiveProjection's Y-flip puts +up up on screen, so
 *  no axis juggling is needed between the engine and the GS.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include "ps2/renderer/render_view.h"
#include "ps2/renderer/texture.h"
#include "ps2/renderer/model.h"
#include "ps2/renderer/vu1.h"
#include "ps2/renderer/gs.h"
#include "ps2/renderer/timing.h"
#include "ps2/math/vec_mat.h"
#include "ps2/builtin/builtin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
    #include "common/q_files.h" // MD2 disk structures.
}

namespace ps2::view {
namespace {

// Depth range for the world projection (ref_gl's values).
constexpr float kZNear = 4.0f;
constexpr float kZFar  = 4096.0f;
constexpr float kWeaponZNear = 0.5f;

// Vertex colour for the not-yet-lit world: GS modulate 128 = texels unchanged.
constexpr u32 kFullBright = vu1::PackColorRGBA(128, 128, 128, 0x80);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
constexpr float kAliasNormals[][3] = {
    #include "client/anorms.h"
};
#pragma GCC diagnostic pop

// ------------------------------------------------------------------------------------------------
// Frame state
// ------------------------------------------------------------------------------------------------

// Frame counters used to mark drawable surfaces:
static int s_frameCount    = 0; // Bumped per RenderFrame; stamps surfaces marked for draw.
static int s_visFrameCount = 0; // Bumped when the PVS changes; stamps reachable nodes.

// Quake 2 view clusters; BeginRegistration() resets them for a new map.
constexpr int kInvalidCluster = -1;
static int s_viewCluster      = kInvalidCluster;
static int s_viewCluster2     = kInvalidCluster;
static int s_oldViewCluster   = kInvalidCluster;
static int s_oldViewCluster2  = kInvalidCluster;

// Scene camera basis for the frame (Quake coordinates, from AngleVectors).
static vec3_t s_forwardVec = {};
static vec3_t s_rightVec   = {};
static vec3_t s_upVec      = {};

// World-to-clip transform for the frame (world geometry draws in world space).
static math::Mat4 s_viewProjMatrix = {};
static math::Mat4 s_weaponViewProjMatrix = {};

// View frustum side planes (left, right, bottom, top) for bounding-box culling.
static cplane_t s_frustum[4] = {};

// Wall texture animation frame (viewDef.time * 2, as in ref_gl).
static int s_textureAnimFrame = 0;

// Adaptive BSP-lighting controls, refreshed from archived cvars each frame.
// Large triangles are probed at most this many lightmap cells apart; within
// that grid only regions whose sampled light is non-linear keep subdividing.
static float s_lightMaxSamplesPerEdge = 8.0f;
static float s_lightErrorTolerance    = 5.0f;

// GS texture modulation happens after the palette lookup. Applying gamma only
// to that palette therefore leaves the light multiplier linear, unlike a
// display gamma curve over the final lit pixel. This small LUT applies the
// matching curve to BSP vertex light (0..128) without thousands of pow() calls
// per frame.
static float s_worldLightGammaTable[129];
static float s_worldLightGamma = -1.0f;

// Scratch for one decompressed cluster PVS, and for the two-cluster union used
// when the camera straddles a solid water boundary. Static: 8 KB each.
alignas(16) static u8 s_dvisPvs[MAX_MAP_LEAFS / 8];
alignas(16) static u8 s_fatPvs[MAX_MAP_LEAFS / 8];

// Textures that received surfaces this frame; DrawTextureChains draws and
// resets exactly these. Sized to the texture cache capacity.
constexpr int kMaxChainTextures = 1024;
static const tex::Texture * s_chainTextures[kMaxChainTextures];
static int s_chainTextureCount = 0;

// World surfaces with transparency (glass/water), chained back-to-front while
// walking the BSP. Collected so they stay out of the opaque chains; the pass
// that draws them lands with the alpha-blend milestone.
static const mod::ModelSurface * s_alphaSurfaces = nullptr;

// Triangle gather buffer: texture chains append here and flush through
// vu1::DrawTriangles when full. DrawTriangles is synchronous, so one buffer
// serves every batch in turn, referenced in place by the DMA chain.
constexpr int kScratchMaxVerts = 3072; // whole triangles (3 * 1024); 96 KB
alignas(16) static vu1::DrawVertex s_scratchVerts[kScratchMaxVerts];
static int s_scratchVertCount = 0;

// MD2 frames index shared positions separately from per-corner texture
// coordinates. Prepare each unique position/light value once per entity, then
// expand only the cheap indexed triangle corners into the DMA scratch buffer.
struct alignas(16) PreparedAliasVertex
{
    math::Vec4 pos;
    math::Vec4 color;
};
alignas(16) static PreparedAliasVertex s_preparedAliasVerts[MAX_VERTS];

// Performance counters for the frame, reset by RenderFrame and read through
// GetDrawStats() by the ps2_show_drawstats overlay.
static DrawStats s_drawStats = {};

// ------------------------------------------------------------------------------------------------
// Frame setup: camera matrices and frustum
// ------------------------------------------------------------------------------------------------

inline int SignBitsForPlane(const cplane_t & plane)
{
    // Sign bits are used for fast box-on-plane-side tests.
    int bits = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (plane.normal[i] < 0.0f)
        {
            bits |= (1 << i);
        }
    }
    return bits;
}

// Builds the four frustum side planes by rotating the view direction around
// the up/right axes by half the FOV (ref_gl's R_SetFrustum construction).
void SetUpFrustum(const refdef_t & viewDef)
{
    RotatePointAroundVector(s_frustum[0].normal, s_upVec,    s_forwardVec, -(90.0f - viewDef.fov_x * 0.5f));
    RotatePointAroundVector(s_frustum[1].normal, s_upVec,    s_forwardVec,  (90.0f - viewDef.fov_x * 0.5f));
    RotatePointAroundVector(s_frustum[2].normal, s_rightVec, s_forwardVec,  (90.0f - viewDef.fov_y * 0.5f));
    RotatePointAroundVector(s_frustum[3].normal, s_rightVec, s_forwardVec, -(90.0f - viewDef.fov_y * 0.5f));

    for (cplane_t & plane : s_frustum)
    {
        plane.type     = PLANE_ANYZ;
        plane.dist     = DotProduct(viewDef.vieworg, plane.normal);
        plane.signbits = static_cast<byte>(SignBitsForPlane(plane));
    }
}

// True when the box is completely outside the frustum and must not draw.
inline bool ShouldCullBBox(float * mins, float * maxs)
{
    for (cplane_t & plane : s_frustum)
    {
        if (BOX_ON_PLANE_SIDE(mins, maxs, &plane) == 2)
        {
            ++s_drawStats.boxesCulled;
            return true;
        }
    }
    return false;
}

void SetupFrame(const refdef_t & viewDef)
{
    ++s_frameCount;

    mod::SetLightStyles(viewDef.lightstyles);

    static const cvar_t * lightSubdivide =
        Cvar_Get("ps2_light_subdivide", "8", CVAR_ARCHIVE);
    static const cvar_t * lightError =
        Cvar_Get("ps2_light_error", "5", CVAR_ARCHIVE);
    s_lightMaxSamplesPerEdge = lightSubdivide->value;
    s_lightErrorTolerance = lightError->value;
    if (s_lightMaxSamplesPerEdge < 4.0f)  { s_lightMaxSamplesPerEdge = 4.0f; }
    if (s_lightMaxSamplesPerEdge > 16.0f) { s_lightMaxSamplesPerEdge = 16.0f; }
    if (s_lightErrorTolerance < 1.0f)  { s_lightErrorTolerance = 1.0f; }
    if (s_lightErrorTolerance > 24.0f) { s_lightErrorTolerance = 24.0f; }

    static const cvar_t * worldLightGammaCvar =
        Cvar_Get("ps2_world_light_gamma", "0.70", CVAR_ARCHIVE);
    float worldLightGamma = worldLightGammaCvar->value;
    if (worldLightGamma < 0.25f) { worldLightGamma = 0.25f; }
    if (worldLightGamma > 2.0f)  { worldLightGamma = 2.0f; }
    if (worldLightGamma != s_worldLightGamma)
    {
        for (int i = 0; i <= 128; ++i)
        {
            const float normalized = static_cast<float>(i) * (1.0f / 128.0f);
            s_worldLightGammaTable[i] =
                std::pow(normalized, worldLightGamma) * 128.0f;
        }
        s_worldLightGamma = worldLightGamma;
    }

    // Animated walls flip frames at 2 Hz of game time (as in ref_gl).
    s_textureAnimFrame = static_cast<int>(viewDef.time * 2.0f);

    // Camera basis vectors from the view angles.
    AngleVectors(viewDef.viewangles, s_forwardVec, s_rightVec, s_upVec);

    const math::Vec3 eye    = { viewDef.vieworg[0], viewDef.vieworg[1], viewDef.vieworg[2] };
    const math::Vec3 target = { eye.x + s_forwardVec[0], eye.y + s_forwardVec[1], eye.z + s_forwardVec[2] };
    const math::Vec3 up     = { s_upVec[0], s_upVec[1], s_upVec[2] };

    const math::Mat4 view = math::LookAt(eye, target, up);
    const math::Mat4 proj = math::PerspectiveProjection(
        math::DegToRad(viewDef.fov_y),
        static_cast<float>(viewDef.width) / static_cast<float>(viewDef.height),
        static_cast<float>(gs::Width()), static_cast<float>(gs::Height()),
        kZNear, kZFar);

    s_viewProjMatrix = view * proj;

    // View weapons need a smaller near plane so close railgun/BFG triangles
    // are clipped rather than rejected whole by the VU guard test. Match
    // ref_gl's RF_DEPTHHACK by remapping the complete weapon depth interval to
    // the nearest 30% of our reversed GS range: ndc z' = 0.3*z + 0.7*w.
    const math::Mat4 weaponProj = math::PerspectiveProjection(
        math::DegToRad(viewDef.fov_y),
        static_cast<float>(viewDef.width) / static_cast<float>(viewDef.height),
        static_cast<float>(gs::Width()), static_cast<float>(gs::Height()),
        kWeaponZNear, kZFar);
    math::Mat4 weaponDepthRange = math::Identity();
    weaponDepthRange.m[2][2] = 0.3f;
    weaponDepthRange.m[3][2] = 0.7f;
    s_weaponViewProjMatrix = view * weaponProj * weaponDepthRange;

    SetUpFrustum(viewDef);
}

// ------------------------------------------------------------------------------------------------
// PVS / visibility
// ------------------------------------------------------------------------------------------------

const mod::ModelLeaf * FindLeafNodeForPoint(const float * point, const mod::ModelInstance & model)
{
    PS2_AssertMsg(model.nodes != nullptr, "World model has no nodes!");

    const mod::ModelNode * node = model.nodes;
    for (;;)
    {
        if (node->contents != -1)
        {
            return reinterpret_cast<const mod::ModelLeaf *>(node);
        }

        const cplane_t * const plane = node->plane;
        const float d = DotProduct(point, plane->normal) - plane->dist;
        node = (d > 0.0f) ? node->children[0] : node->children[1];
    }
}

// Decompresses a cluster's RLE visibility row into 'out' (zero runs are
// length-encoded). A null 'in' (no vis data) decompresses to all-visible.
const u8 * DecompressModelVis(u8 * out, const u8 * in, const mod::ModelInstance & model)
{
    const auto * vis = static_cast<const dvis_t *>(model.vis);
    int row = (vis->numclusters + 7) >> 3;
    u8 * dst = out;

    if (in == nullptr)
    {
        while (row-- > 0)
        {
            *dst++ = 0xFF;
        }
        return out;
    }

    do
    {
        if (*in != 0)
        {
            *dst++ = *in++;
            continue;
        }

        int c = in[1];
        in += 2;
        while (c-- > 0)
        {
            *dst++ = 0;
        }
    } while (dst - out < row);

    return out;
}

// Returns the decompressed PVS row for 'cluster' (shared s_dvisPvs buffer, so
// don't hold on to it across calls).
const u8 * GetClusterPVS(int cluster, const mod::ModelInstance & model)
{
    if (cluster == kInvalidCluster || model.vis == nullptr)
    {
        std::memset(s_dvisPvs, 0xFF, sizeof(s_dvisPvs)); // All visible.
        return s_dvisPvs;
    }

    const auto * vis = static_cast<const dvis_t *>(model.vis);
    const u8 * compressed = static_cast<const u8 *>(model.vis) + vis->bitofs[cluster][DVIS_PVS];
    return DecompressModelVis(s_dvisPvs, compressed, model);
}

// Finds the clusters the camera sees from this frame. Two clusters when near a
// solid water surface, so crossing it doesn't draw wrong (checked by sampling a
// second leaf 16 units above/below the eye).
void SetUpViewClusters(const refdef_t & viewDef, const mod::ModelInstance & world)
{
    const mod::ModelLeaf * leaf = FindLeafNodeForPoint(viewDef.vieworg, world);

    s_oldViewCluster  = s_viewCluster;
    s_oldViewCluster2 = s_viewCluster2;
    s_viewCluster = s_viewCluster2 = leaf->cluster;

    vec3_t temp;
    VectorCopy(viewDef.vieworg, temp);
    temp[2] += (leaf->contents == 0) ? -16.0f : 16.0f;

    leaf = FindLeafNodeForPoint(temp, world);
    if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != s_viewCluster2))
    {
        s_viewCluster2 = leaf->cluster;
    }
}

// Stamps the leafs in the current clusters' PVS - and the node chains above
// them - with the new vis frame count. Skipped entirely while the camera stays
// in the same cluster(s), which is the common case.
void MarkLeaves(const mod::ModelInstance & world)
{
    if (s_oldViewCluster  == s_viewCluster  &&
        s_oldViewCluster2 == s_viewCluster2 &&
        s_viewCluster != kInvalidCluster)
    {
        return; // Same clusters as the previous frame; marks still valid.
    }

    ++s_visFrameCount;
    s_oldViewCluster  = s_viewCluster;
    s_oldViewCluster2 = s_viewCluster2;

    if (s_viewCluster == kInvalidCluster || world.vis == nullptr)
    {
        // Outside the map or no PVS data: mark everything visible.
        for (int i = 0; i < world.numLeafs; ++i)
        {
            world.leafs[i].visFrame = s_visFrameCount;
        }
        for (int i = 0; i < world.numNodes; ++i)
        {
            world.nodes[i].visFrame = s_visFrameCount;
        }
        return;
    }

    const u8 * vis = GetClusterPVS(s_viewCluster, world);

    // May have to combine two clusters because of solid water boundaries:
    if (s_viewCluster2 != s_viewCluster)
    {
        std::memcpy(s_fatPvs, vis, static_cast<size_t>((world.numLeafs + 7) / 8));
        vis = GetClusterPVS(s_viewCluster2, world);

        // Both buffers are 16-byte aligned, so OR them a word at a time.
        u32 * fat = static_cast<u32 *>(static_cast<void *>(s_fatPvs));
        const u32 * add = static_cast<const u32 *>(static_cast<const void *>(vis));

        const int words = (world.numLeafs + 31) / 32;
        for (int i = 0; i < words; ++i)
        {
            fat[i] |= add[i];
        }
        vis = s_fatPvs;
    }

    mod::ModelLeaf * leaf = world.leafs;
    for (int i = 0; i < world.numLeafs; ++i, ++leaf)
    {
        const int cluster = leaf->cluster;
        if (cluster == kInvalidCluster)
        {
            continue;
        }

        if (vis[cluster >> 3] & (1 << (cluster & 7)))
        {
            auto * node = reinterpret_cast<mod::ModelNode *>(leaf);
            do
            {
                if (node->visFrame == s_visFrameCount)
                {
                    break; // This branch is already marked up to the root.
                }
                node->visFrame = s_visFrameCount;
                node = node->parent;
            } while (node != nullptr);
        }
    }
}

// ------------------------------------------------------------------------------------------------
// World BSP walk and texture chains
// ------------------------------------------------------------------------------------------------

// Returns the texture a surface draws with this frame, following the animation
// chain for animated walls (torches, screens). Never null: the model loader
// substitutes the debug checkerboard for missing wall textures.
const tex::Texture * TextureAnimation(const mod::ModelTexInfo * texInfo, int frame)
{
    PS2_Assert(texInfo != nullptr && texInfo->texture != nullptr);

    if (texInfo->next == nullptr)
    {
        return texInfo->texture; // Not animated.
    }

    int c = frame % texInfo->numFrames;
    if (c < 0)
    {
        c += texInfo->numFrames;
    }
    while (c-- > 0)
    {
        texInfo = texInfo->next;
    }
    return texInfo->texture;
}

void ChainOpaqueSurface(mod::ModelSurface & surface, int animationFrame)
{
    ++s_drawStats.surfaces;
    const tex::Texture * texture =
        TextureAnimation(surface.texInfo, animationFrame);
    if (texture->textureChain == nullptr)
    {
        PS2_AssertMsg(s_chainTextureCount < kMaxChainTextures,
                      "Out of texture chain slots!");
        s_chainTextures[s_chainTextureCount++] = texture;
    }
    surface.textureChain  = texture->textureChain;
    texture->textureChain = &surface;
}

// Recursively marks and chains the visible world surfaces: walks the BSP
// front-to-back, culling nodes against the PVS marks and the view frustum,
// and threads each drawable surface onto its texture's chain so the next
// DrawTextureChains() call renders what was collected here.
void RecursiveWorldNode(const refdef_t & viewDef, const mod::ModelInstance & world, mod::ModelNode * node)
{
    if (node->contents == CONTENTS_SOLID)
    {
        return;
    }
    if (node->visFrame != s_visFrameCount)
    {
        return; // Not reachable from the current PVS cluster.
    }
    if (ShouldCullBBox(node->minmaxs, node->minmaxs + 3))
    {
        return; // Entirely outside the view frustum.
    }

    ++s_drawStats.nodesWalked;

    // Leaf: stamp its surfaces as drawable this frame.
    if (node->contents != -1)
    {
        auto * leaf = reinterpret_cast<mod::ModelLeaf *>(node);

        // Check for door-connected areas:
        if (viewDef.areabits != nullptr)
        {
            if (!(viewDef.areabits[leaf->area >> 3] & (1 << (leaf->area & 7))))
            {
                return; // Not visible.
            }
        }

        mod::ModelSurface ** mark = leaf->firstMarkSurface;
        for (int i = 0; i < leaf->numMarkSurfaces; ++i, ++mark)
        {
            (*mark)->visFrame = s_frameCount;
        }
        return;
    }

    // Decision node: find which side of its plane the camera is on.
    float dot;
    const cplane_t * const plane = node->plane;
    switch (plane->type)
    {
    case PLANE_X:
        dot = viewDef.vieworg[0] - plane->dist;
        break;
    case PLANE_Y:
        dot = viewDef.vieworg[1] - plane->dist;
        break;
    case PLANE_Z:
        dot = viewDef.vieworg[2] - plane->dist;
        break;
    default:
        dot = DotProduct(viewDef.vieworg, plane->normal) - plane->dist;
        break;
    }

    const int  side          = (dot >= 0.0f) ? 0 : 1;
    const bool cameraOnBack  = (side == 1);

    // Recurse down the camera side first (front-to-back order)...
    RecursiveWorldNode(viewDef, world, node->children[side]);

    // ...then chain this node's surfaces that face the camera...
    mod::ModelSurface * surf = world.surfaces + node->firstSurface;
    for (int i = 0; i < node->numSurfaces; ++i, ++surf)
    {
        if (surf->visFrame != s_frameCount)
        {
            continue; // Not in a visible leaf.
        }
        if (HasFlag(surf->flags, mod::SurfaceFlags::PlaneBack) != cameraOnBack)
        {
            continue; // Facing away from the camera.
        }

        const int texFlags = surf->texInfo->flags;
        if (texFlags & SURF_SKY)
        {
            // TODO: Sky surfaces feed the skybox bounds (skybox milestone).
            continue;
        }

        if (texFlags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
        {
            // Translucent/warped: kept for a later back-to-front alpha pass.
            surf->textureChain = s_alphaSurfaces;
            s_alphaSurfaces = surf;
            ++s_drawStats.surfacesAlpha;
            continue;
        }

        // Opaque: thread onto its texture's draw chain.
        ChainOpaqueSurface(*surf, s_textureAnimFrame);
    }

    // ...and finally recurse down the far side.
    RecursiveWorldNode(viewDef, world, node->children[side ^ 1]);
}

// ------------------------------------------------------------------------------------------------
// Triangle gathering and submission
// ------------------------------------------------------------------------------------------------

// Sends the gathered scratch triangles as one batch and empties the buffer.
inline void FlushScratch(const math::Mat4 & mvp, const tex::Texture & texture)
{
    if (s_scratchVertCount > 0)
    {
        ++s_drawStats.drawBatches;
        vu1::DrawTriangles(mvp, texture, s_scratchVerts, s_scratchVertCount);
        s_scratchVertCount = 0;
    }
}

// ------------------------------------------------------------------------------------------------
// Triangle clipping against the VU clip volume
//
// The VU microprogram does not clip: a triangle with any vertex outside its
// clip volume is rejected whole (ADC bit), so world triangles must be
// pre-clipped here on the EE against all six planes the VU judges - near and
// far (z is judged exactly), and the four guard-band side planes. The sides
// matter just as much as near: a floor polygon clipped at the near plane
// right under the camera lands at tiny w and enormous |x/w|, far outside any
// band the GS 12.4 coordinates could hold. Clipping runs in clip space - a
// vertex is inside while w-z >= 0 (near; also excludes everything behind the
// camera), w+z >= 0 (far), and G*w +/- x/y >= 0 (sides, G = the VU guard-band
// limit). Everything a vertex carries - world position, UVs, and the six
// distances themselves - is linear under a plane cut, so a split interpolates
// the whole ClipVertex with aligned vector lerps on VU0, no scalar float math.
// Whole-triangle-inside is the common case and skips all of it.
// ------------------------------------------------------------------------------------------------

constexpr int kNumClipPlanes = 6;
constexpr int kMaxLightSubdivideDepth = 6;

// Clip a hair early so the VU's judgement never flags a vertex this clipper
// just placed on the boundary (clip-space units, i.e. ~world units here).
constexpr float kClipEpsilon = 0.01f;

// Signed distances to the clip planes; >= 0 is inside. Held as two whole quadwords
// (six planes, two spare lanes) so the whole set interpolates with two vector lerps
// while the per-plane tests still read it as a plain float array.
union ClipDists
{
    math::Vec4 q[2];
    float f[8];
};

// Everything a clipped vertex carries, qword-aligned so a plane cut and the
// lighting tessellator can use vector lerps throughout.
struct alignas(16) ClipVertex
{
    math::Vec4 pos; // world position, w = 1
    math::Vec4 st;  // diffuse texture coords in xy; zw unused
    math::Vec4 lightmap; // atlas UV in xy; used to locate BSP light samples.
    math::Vec4 color; // GS modulation channels as floats in xyzw.
    ClipDists  d;
};
static_assert(sizeof(ClipVertex) == 96, "ClipVertex must be exactly six quadwords");

// Camera-independent output of adaptive BSP lighting. Positions, diffuse UVs
// and final vertex colours can be reused until one of the surface's own light
// styles changes; clip distances still have to be rebuilt for the current MVP.
struct alignas(16) CachedLitVertex
{
    math::Vec4 pos;
    math::Vec4 st;
    math::Vec4 color;
};
static_assert(sizeof(CachedLitVertex) == 48,
              "CachedLitVertex must be exactly three quadwords");

constexpr int kMaxCachedVertsPerTriangle =
    3 * (1 << kMaxLightSubdivideDepth);
static CachedLitVertex s_litBuildVerts[kMaxCachedVertsPerTriangle];
static int s_litBuildVertCount = 0;
static std::vector<const mod::ModelTriangle *> s_cachedLitTriangles;
static int s_litCacheBytes = 0;

void ClearLitTriangleCaches()
{
    for (const mod::ModelTriangle * triangle : s_cachedLitTriangles)
    {
        if (triangle->litCacheVertices != nullptr)
        {
            PS2_MemFree(
                triangle->litCacheVertices,
                static_cast<size_t>(triangle->litCacheCapacity) *
                    sizeof(CachedLitVertex),
                MEMTAG_MDL_WORLD);
        }
        triangle->litCacheVertices = nullptr;
        triangle->litCacheKey = 0;
        triangle->litCacheVertexCount = 0;
        triangle->litCacheCapacity = 0;
    }
    s_cachedLitTriangles.clear();
    s_litCacheBytes = 0;
}

// Sutherland-Hodgman pass of a convex polygon against one plane. 'out' must
// hold inCount + 1 vertexes. Returns the clipped vertex count.
int ClipAgainstPlane(const ClipVertex * in, const int inCount, ClipVertex * out, const int plane)
{
    int outCount = 0;
    for (int i = 0; i < inCount; ++i)
    {
        const ClipVertex & a = in[i];
        const ClipVertex & b = in[(i + 1 == inCount) ? 0 : i + 1];

        if (a.d.f[plane] >= 0.0f)
        {
            out[outCount++] = a;
        }
        if ((a.d.f[plane] >= 0.0f) != (b.d.f[plane] >= 0.0f)) // Edge crosses the plane.
        {
            const float t = a.d.f[plane] / (a.d.f[plane] - b.d.f[plane]);
            ClipVertex & o = out[outCount++];
            math::LerpTo(o.pos,    a.pos,    b.pos,    t);
            math::LerpTo(o.st,     a.st,     b.st,     t);
            math::LerpTo(o.lightmap, a.lightmap, b.lightmap, t);
            math::LerpTo(o.color,  a.color,  b.color,  t);
            math::LerpTo(o.d.q[0], a.d.q[0], b.d.q[0], t);
            math::LerpTo(o.d.q[1], a.d.q[1], b.d.q[1], t);
        }
    }
    return outCount;
}

inline u32 PackFloatColor(const math::Vec4 & color)
{
    return vu1::PackColorRGBA(
        static_cast<u32>(color.x + 0.5f),
        static_cast<u32>(color.y + 0.5f),
        static_cast<u32>(color.z + 0.5f),
        0x80);
}

inline void EmitScratchVertex(const ClipVertex & v)
{
    vu1::DrawVertex & dst = s_scratchVerts[s_scratchVertCount++];
    dst.x    = v.pos.x;
    dst.y    = v.pos.y;
    dst.z    = v.pos.z;
    dst.w    = 1.0f;
    dst.rgba = PackFloatColor(v.color);
    dst.s    = v.st.x;
    dst.t    = v.st.y;
    dst.q    = 1.0f;
}

constexpr float kLightmapAtlasSize = 128.0f;
constexpr float kMinLightSamplesPerEdge = 2.0f;

void UnpackLightColor(ClipVertex & vertex, u32 color)
{
    const u32 r =  color        & 0xFFu;
    const u32 g = (color >> 8)  & 0xFFu;
    const u32 b = (color >> 16) & 0xFFu;
    PS2_Assert(r <= 128 && g <= 128 && b <= 128);
    vertex.color = {
        s_worldLightGammaTable[r],
        s_worldLightGammaTable[g],
        s_worldLightGammaTable[b],
        128.0f
    };
}

void SampleVertexLight(ClipVertex & vertex, const mod::ModelSurface & surface)
{
    // model_load normalises against the original 128x128 atlas convention:
    // uv*128 = local sample coordinate + atlas offset + half-texel.
    const float sampleS = vertex.lightmap.x * kLightmapAtlasSize -
                          static_cast<float>(surface.light_s) - 0.5f;
    const float sampleT = vertex.lightmap.y * kLightmapAtlasSize -
                          static_cast<float>(surface.light_t) - 0.5f;
    UnpackLightColor(vertex, mod::SampleStaticLight(surface, sampleS, sampleT));
}

float LightEdgeLengthSq(const ClipVertex & a, const ClipVertex & b)
{
    const float ds = (a.lightmap.x - b.lightmap.x) * kLightmapAtlasSize;
    const float dt = (a.lightmap.y - b.lightmap.y) * kLightmapAtlasSize;
    return ds * ds + dt * dt;
}

ClipVertex LightMidpoint(const ClipVertex & a, const ClipVertex & b,
                         const mod::ModelSurface & surface)
{
    ClipVertex midpoint;
    math::LerpTo(midpoint.pos,      a.pos,      b.pos,      0.5f);
    math::LerpTo(midpoint.st,       a.st,       b.st,       0.5f);
    math::LerpTo(midpoint.lightmap, a.lightmap, b.lightmap, 0.5f);
    math::LerpTo(midpoint.d.q[0],   a.d.q[0],   b.d.q[0],   0.5f);
    math::LerpTo(midpoint.d.q[1],   a.d.q[1],   b.d.q[1],   0.5f);
    SampleVertexLight(midpoint, surface);
    return midpoint;
}

ClipVertex LightCentroid(const ClipVertex (&corners)[3],
                         const mod::ModelSurface & surface)
{
    // lerp(lerp(a,b,1/2),c,1/3) == (a+b+c)/3. Keep every interpolant in
    // lockstep so a centroid chosen for lighting is also a valid split probe
    // after clipping and projection.
    ClipVertex edgeMidpoint;
    ClipVertex centroid;
    math::LerpTo(edgeMidpoint.pos,      corners[0].pos,      corners[1].pos,      0.5f);
    math::LerpTo(edgeMidpoint.st,       corners[0].st,       corners[1].st,       0.5f);
    math::LerpTo(edgeMidpoint.lightmap, corners[0].lightmap, corners[1].lightmap, 0.5f);
    math::LerpTo(edgeMidpoint.d.q[0],   corners[0].d.q[0],   corners[1].d.q[0],   0.5f);
    math::LerpTo(edgeMidpoint.d.q[1],   corners[0].d.q[1],   corners[1].d.q[1],   0.5f);

    math::LerpTo(centroid.pos,      edgeMidpoint.pos,      corners[2].pos,      1.0f / 3.0f);
    math::LerpTo(centroid.st,       edgeMidpoint.st,       corners[2].st,       1.0f / 3.0f);
    math::LerpTo(centroid.lightmap, edgeMidpoint.lightmap, corners[2].lightmap, 1.0f / 3.0f);
    math::LerpTo(centroid.d.q[0],   edgeMidpoint.d.q[0],   corners[2].d.q[0],   1.0f / 3.0f);
    math::LerpTo(centroid.d.q[1],   edgeMidpoint.d.q[1],   corners[2].d.q[1],   1.0f / 3.0f);
    SampleVertexLight(centroid, surface);
    return centroid;
}

float MaxLightError(const math::Vec4 & actual, float expectedR,
                    float expectedG, float expectedB)
{
    float error = std::fabs(actual.x - expectedR);
    const float errorG = std::fabs(actual.y - expectedG);
    const float errorB = std::fabs(actual.z - expectedB);
    if (errorG > error) { error = errorG; }
    if (errorB > error) { error = errorB; }
    return error;
}

// Clips one already-lit triangle against the VU guard volume and appends the
// survivors to the VU scratch batch.
void SubmitWorldTriangle(const ClipVertex (&corners)[3], const math::Mat4 & mvp,
                         const tex::Texture & texture)
{
    int insidePerPlane[kNumClipPlanes] = {};
    for (const ClipVertex & corner : corners)
    {
        for (int p = 0; p < kNumClipPlanes; ++p)
        {
            insidePerPlane[p] += (corner.d.f[p] >= 0.0f);
        }
    }

    int insideTotal = 0;
    bool outsideAny = false;
    for (int p = 0; p < kNumClipPlanes; ++p)
    {
        insideTotal += insidePerPlane[p];
        outsideAny  |= (insidePerPlane[p] == 0);
    }

    if (outsideAny)
    {
        ++s_drawStats.trisCulled;
        return;
    }

    if (insideTotal == 3 * kNumClipPlanes)
    {
        ++s_drawStats.trisDrawn;
        if (s_scratchVertCount + 3 > kScratchMaxVerts)
        {
            FlushScratch(mvp, texture);
        }
        for (const ClipVertex & corner : corners)
        {
            EmitScratchVertex(corner);
        }
        return;
    }

    ++s_drawStats.trisClipped;
    ClipVertex bufferA[3 + kNumClipPlanes];
    ClipVertex bufferB[3 + kNumClipPlanes];

    const ClipVertex * in = corners;
    ClipVertex * out = bufferA;
    int count = 3;
    for (int p = 0; p < kNumClipPlanes && count >= 3; ++p)
    {
        if (insidePerPlane[p] == 3)
        {
            continue;
        }
        count = ClipAgainstPlane(in, count, out, p);
        in  = out;
        out = (out == bufferA) ? bufferB : bufferA;
    }
    if (count < 3)
    {
        return;
    }

    if (s_scratchVertCount + (count - 2) * 3 > kScratchMaxVerts)
    {
        FlushScratch(mvp, texture);
    }
    for (int v = 1; v < count - 1; ++v)
    {
        EmitScratchVertex(in[0]);
        EmitScratchVertex(in[v]);
        EmitScratchVertex(in[v + 1]);
    }
    s_drawStats.trisDrawn += count - 2;
}

u32 LitTriangleCacheKey(const mod::ModelSurface & surface)
{
    u32 key = mod::StaticLightStyleKey(surface);
    auto mixFloat = [&key](float value)
    {
        u32 bits;
        std::memcpy(&bits, &value, sizeof(bits));
        key ^= bits + 0x9E3779B9u + (key << 6) + (key >> 2);
    };
    mixFloat(s_lightMaxSamplesPerEdge);
    mixFloat(s_lightErrorTolerance);
    mixFloat(s_worldLightGamma);
    return key;
}

void AppendCachedTriangle(const ClipVertex (&corners)[3])
{
    PS2_Assert(s_litBuildVertCount + 3 <= kMaxCachedVertsPerTriangle);
    for (const ClipVertex & corner : corners)
    {
        CachedLitVertex & out = s_litBuildVerts[s_litBuildVertCount++];
        out.pos = corner.pos;
        out.st = corner.st;
        out.color = corner.color;
    }
}

// Bisects the longest lightmap-space edge and stores the resulting leaf
// triangles without camera-dependent clip data. A coarse maximum spacing makes
// sure large faces are probed; midpoint and centroid samples then retain detail
// around lamps and shadows. The expensive recursion and light sampling run only
// when this surface's relevant animated styles actually change.
void BuildCachedLitTriangle(const ClipVertex (&corners)[3],
                            const mod::ModelSurface & surface, int depth)
{
    float edgeLengthSq[3] = {
        LightEdgeLengthSq(corners[0], corners[1]),
        LightEdgeLengthSq(corners[1], corners[2]),
        LightEdgeLengthSq(corners[2], corners[0])
    };
    int longest = 0;
    if (edgeLengthSq[1] > edgeLengthSq[longest]) { longest = 1; }
    if (edgeLengthSq[2] > edgeLengthSq[longest]) { longest = 2; }

    if (surface.samples == nullptr || depth >= kMaxLightSubdivideDepth ||
        edgeLengthSq[longest] <=
            kMinLightSamplesPerEdge * kMinLightSamplesPerEdge)
    {
        AppendCachedTriangle(corners);
        return;
    }

    const int edgeA = longest;
    const int edgeB = (longest + 1) % 3;
    const int opposite = (longest + 2) % 3;
    const ClipVertex midpoint = LightMidpoint(corners[edgeA], corners[edgeB], surface);

    bool shouldSplit =
        edgeLengthSq[longest] >
        s_lightMaxSamplesPerEdge * s_lightMaxSamplesPerEdge;
    if (!shouldSplit)
    {
        const float midpointError = MaxLightError(
            midpoint.color,
            (corners[edgeA].color.x + corners[edgeB].color.x) * 0.5f,
            (corners[edgeA].color.y + corners[edgeB].color.y) * 0.5f,
            (corners[edgeA].color.z + corners[edgeB].color.z) * 0.5f);
        const ClipVertex centroid = LightCentroid(corners, surface);
        const float centroidError = MaxLightError(
            centroid.color,
            (corners[0].color.x + corners[1].color.x + corners[2].color.x) / 3.0f,
            (corners[0].color.y + corners[1].color.y + corners[2].color.y) / 3.0f,
            (corners[0].color.z + corners[1].color.z + corners[2].color.z) / 3.0f);
        shouldSplit = midpointError > s_lightErrorTolerance ||
                      centroidError > s_lightErrorTolerance;
    }

    if (!shouldSplit)
    {
        AppendCachedTriangle(corners);
        return;
    }

    const ClipVertex first[3]  = { corners[edgeA], midpoint, corners[opposite] };
    const ClipVertex second[3] = { midpoint, corners[edgeB], corners[opposite] };
    BuildCachedLitTriangle(first,  surface, depth + 1);
    BuildCachedLitTriangle(second, surface, depth + 1);
}

// Appends a polygon's adaptively lit triangles to the scratch buffer.
void GatherPolyTriangles(const mod::ModelPoly & poly, const mod::ModelSurface & surface,
                         const math::Mat4 & mvp, const tex::Texture & texture,
                         const u32 cacheKey)
{
    const int numTriangles = poly.numVerts - 2;
    for (int t = 0; t < numTriangles; ++t)
    {
        const mod::ModelTriangle & tri = poly.triangles[t];
        if (tri.vertexes[0] == tri.vertexes[1])
        {
            continue; // Degenerate placeholder left by failed triangulation.
        }

        if (tri.litCacheVertices == nullptr || tri.litCacheKey != cacheKey)
        {
            ++s_drawStats.lightCacheBuilds;
            ClipVertex sourceCorners[3] = {};
            for (int v = 0; v < 3; ++v)
            {
                const mod::PolyVertex & src = poly.vertexes[tri.vertexes[v]];
                ClipVertex & corner = sourceCorners[v];
                corner.pos = {
                    src.position.x, src.position.y, src.position.z, 1.0f
                };
                corner.st = {
                    src.texture_s, src.texture_t, 0.0f, 0.0f
                };
                corner.lightmap = {
                    src.lightmap_s, src.lightmap_t, 0.0f, 0.0f
                };
                SampleVertexLight(corner, surface);
            }

            s_litBuildVertCount = 0;
            BuildCachedLitTriangle(sourceCorners, surface, 0);
            PS2_Assert(s_litBuildVertCount >= 3 &&
                       (s_litBuildVertCount % 3) == 0);

            const bool firstAllocation = tri.litCacheCapacity == 0;
            if (s_litBuildVertCount > tri.litCacheCapacity)
            {
                if (tri.litCacheVertices != nullptr)
                {
                    s_litCacheBytes -=
                        static_cast<int>(tri.litCacheCapacity) *
                        static_cast<int>(sizeof(CachedLitVertex));
                    PS2_MemFree(
                        tri.litCacheVertices,
                        static_cast<size_t>(tri.litCacheCapacity) *
                            sizeof(CachedLitVertex),
                        MEMTAG_MDL_WORLD);
                }
                tri.litCacheVertices = PS2_MemAllocAligned(
                    16,
                    static_cast<size_t>(s_litBuildVertCount) *
                        sizeof(CachedLitVertex),
                    MEMTAG_MDL_WORLD);
                tri.litCacheCapacity = static_cast<u16>(s_litBuildVertCount);
                s_litCacheBytes +=
                    s_litBuildVertCount *
                    static_cast<int>(sizeof(CachedLitVertex));
            }
            if (firstAllocation)
            {
                s_cachedLitTriangles.push_back(&tri);
            }

            std::memcpy(
                tri.litCacheVertices,
                s_litBuildVerts,
                static_cast<size_t>(s_litBuildVertCount) *
                    sizeof(CachedLitVertex));
            tri.litCacheVertexCount = static_cast<u16>(s_litBuildVertCount);
            tri.litCacheKey = cacheKey;
        }
        else
        {
            ++s_drawStats.lightCacheHits;
        }

        const auto * cached =
            static_cast<const CachedLitVertex *>(tri.litCacheVertices);
        for (int first = 0; first < tri.litCacheVertexCount; first += 3)
        {
            ClipVertex corners[3] = {};
            for (int v = 0; v < 3; ++v)
            {
                const CachedLitVertex & src = cached[first + v];
                ClipVertex & corner = corners[v];
                corner.pos = src.pos;
                corner.st = src.st;
                corner.color = src.color;

                const math::Vec4 clip = math::Transform(corner.pos, mvp);
                const float gw = vu1::kGuardBandNdcLimit * clip.w;
                corner.d.f[0] = (clip.w - clip.z) - kClipEpsilon;
                corner.d.f[1] = (clip.w + clip.z) - kClipEpsilon;
                corner.d.f[2] = (gw - clip.x) - kClipEpsilon;
                corner.d.f[3] = (gw + clip.x) - kClipEpsilon;
                corner.d.f[4] = (gw - clip.y) - kClipEpsilon;
                corner.d.f[5] = (gw + clip.y) - kClipEpsilon;
            }
            SubmitWorldTriangle(corners, mvp, texture);
        }
    }
}

// Draws every texture chain built by RecursiveWorldNode and resets them.
void DrawTextureChains(const math::Mat4 & mvp)
{
    for (int i = 0; i < s_chainTextureCount; ++i)
    {
        const tex::Texture * texture = s_chainTextures[i];

        for (const mod::ModelSurface * surf = texture->textureChain; surf != nullptr; surf = surf->textureChain)
        {
            const u32 cacheKey = LitTriangleCacheKey(*surf);
            for (const mod::ModelPoly * poly = surf->polys; poly != nullptr; poly = poly->next)
            {
                if (poly->numVerts >= 3) // Need at least one triangle.
                {
                    GatherPolyTriangles(*poly, *surf, mvp, *texture, cacheKey);
                }
            }
        }
        FlushScratch(mvp, *texture);

        texture->textureChain = nullptr; // Reset for the next frame.
    }
    s_chainTextureCount = 0;
}

// ------------------------------------------------------------------------------------------------
// Alias (MD2) entity pass
//
// The interpolation follows the original ref_gl GL_DrawAliasFrameLerp path:
// frame byte positions are expanded by each frame's scale/translate, while the
// old entity origin is first expressed in the current entity's local axes.
// The indexed MD2 triangle/ST tables are used instead of the GL command strips;
// index_st is per triangle corner, so skin seams remain correct.
// ------------------------------------------------------------------------------------------------

math::Mat4 EntityModelMatrix(const entity_t & entity)
{
    // Row-vector equivalent of ref_gl's R_RotateForEntity. Alias pitch has the
    // historical sign correction applied by R_DrawAliasModel around that call.
    const math::Mat4 roll  = math::RotationX(math::DegToRad(-entity.angles[ROLL]));
    const math::Mat4 pitch = math::RotationY(math::DegToRad( entity.angles[PITCH]));
    const math::Mat4 yaw   = math::RotationZ(math::DegToRad( entity.angles[YAW]));
    const math::Mat4 move  = math::Translation(entity.origin[0], entity.origin[1], entity.origin[2]);
    return roll * pitch * yaw * move;
}

math::Mat4 BrushModelMatrix(const entity_t & entity)
{
    // R_DrawBrushModel temporarily negates pitch and roll before calling the
    // shared OpenGL entity transform. Preserve that Quake II convention while
    // expressing it in our row-vector matrix order.
    const math::Mat4 roll  = math::RotationX(math::DegToRad( entity.angles[ROLL]));
    const math::Mat4 pitch = math::RotationY(math::DegToRad(-entity.angles[PITCH]));
    const math::Mat4 yaw   = math::RotationZ(math::DegToRad( entity.angles[YAW]));
    const math::Mat4 move  =
        math::Translation(entity.origin[0], entity.origin[1], entity.origin[2]);
    return roll * pitch * yaw * move;
}

math::Vec3 BrushCameraLocal(const entity_t & entity, const refdef_t & viewDef)
{
    const math::Vec3 relative = {
        viewDef.vieworg[0] - entity.origin[0],
        viewDef.vieworg[1] - entity.origin[1],
        viewDef.vieworg[2] - entity.origin[2]
    };
    if (entity.angles[0] == 0.0f &&
        entity.angles[1] == 0.0f &&
        entity.angles[2] == 0.0f)
    {
        return relative;
    }

    vec3_t angles = {
        entity.angles[0], entity.angles[1], entity.angles[2]
    };
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    AngleVectors(angles, forward, right, up);
    return {
         relative.x * forward[0] + relative.y * forward[1] + relative.z * forward[2],
        -(relative.x * right[0]   + relative.y * right[1]   + relative.z * right[2]),
         relative.x * up[0]      + relative.y * up[1]      + relative.z * up[2]
    };
}

bool CullBrushEntity(const entity_t & entity, const mod::ModelInstance & model)
{
    float mins[3];
    float maxs[3];
    const bool rotated =
        entity.angles[0] != 0.0f ||
        entity.angles[1] != 0.0f ||
        entity.angles[2] != 0.0f;
    if (rotated)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            mins[axis] = entity.origin[axis] - model.radius;
            maxs[axis] = entity.origin[axis] + model.radius;
        }
    }
    else
    {
        mins[0] = entity.origin[0] + model.mins.x;
        mins[1] = entity.origin[1] + model.mins.y;
        mins[2] = entity.origin[2] + model.mins.z;
        maxs[0] = entity.origin[0] + model.maxs.x;
        maxs[1] = entity.origin[1] + model.maxs.y;
        maxs[2] = entity.origin[2] + model.maxs.z;
    }
    return ShouldCullBBox(mins, maxs);
}

void DrawBrushModel(const entity_t & entity, const mod::ModelInstance & model,
                    const refdef_t & viewDef)
{
    PS2_Assert(model.type == mod::ModelType::Brush);
    if (model.numModelSurfaces <= 0 ||
        (entity.flags & RF_TRANSLUCENT) != 0)
    {
        return; // Translucent brush entities need the later sorted alpha pass.
    }
    if (CullBrushEntity(entity, model))
    {
        return;
    }

    PS2_Assert(model.surfaces != nullptr);
    PS2_Assert(model.firstModelSurface >= 0);
    PS2_Assert(model.firstModelSurface + model.numModelSurfaces <= model.numSurfaces);
    PS2_Assert(s_scratchVertCount == 0 && s_chainTextureCount == 0);

    const math::Vec3 cameraLocal = BrushCameraLocal(entity, viewDef);
    mod::ModelSurface * surface =
        model.surfaces + model.firstModelSurface;
    for (int i = 0; i < model.numModelSurfaces; ++i, ++surface)
    {
        const cplane_t & plane = *surface->plane;
        const float dot =
            cameraLocal.x * plane.normal[0] +
            cameraLocal.y * plane.normal[1] +
            cameraLocal.z * plane.normal[2] - plane.dist;
        const bool planeBack =
            HasFlag(surface->flags, mod::SurfaceFlags::PlaneBack);
        const bool facing = planeBack
            ? dot < -mod::kBackFaceEpsilon
            : dot >  mod::kBackFaceEpsilon;
        if (!facing)
        {
            continue;
        }

        const int texFlags = surface->texInfo->flags;
        if ((texFlags & SURF_SKY) != 0)
        {
            continue; // Sky bounds are still a later renderer milestone.
        }
        if ((texFlags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) != 0)
        {
            ++s_drawStats.surfacesAlpha;
            continue;
        }
        ChainOpaqueSurface(*surface, entity.frame);
    }

    const math::Mat4 mvp = BrushModelMatrix(entity) * s_viewProjMatrix;
    DrawTextureChains(mvp);
}

const tex::Texture & AliasSkin(const entity_t & entity, const mod::ModelInstance & model)
{
    if (entity.skin != nullptr)
    {
        return *reinterpret_cast<const tex::Texture *>(entity.skin);
    }

    int skinIndex = entity.skinnum;
    if (skinIndex < 0 || skinIndex >= mod::kMaxMD2Skins || model.skins[skinIndex] == nullptr)
    {
        skinIndex = 0;
    }
    return (model.skins[skinIndex] != nullptr) ? *model.skins[skinIndex] : tex::DebugTexture();
}

// TEX0 describes each texture dimension as a power-of-two exponent. The GS
// therefore normalises ST coordinates against this rounded-up extent, not the
// image's actual (often NPOT) width/height. Quake II model skins commonly use
// sizes such as 288x195, so dividing their pixel STs by the real dimensions
// samples far outside the uploaded image. Match draw_log2/TEX0 by rounding up.
int GSTextureExtent(int imageDimension)
{
    PS2_Assert(imageDimension > 0);

    int extent = 1;
    while (extent < imageDimension)
    {
        extent <<= 1;
    }
    return extent;
}

template<typename T>
const T * MD2DataAt(const dmdl_t & md2, int byteOffset)
{
    PS2_Assert(byteOffset >= 0 && (byteOffset % static_cast<int>(alignof(T))) == 0);
    const auto * bytes = reinterpret_cast<const u8 *>(&md2);
    const void * aligned = __builtin_assume_aligned(bytes + byteOffset, alignof(T));
    return static_cast<const T *>(aligned);
}

math::Vec3 AliasModelLight(const entity_t & entity, const refdef_t & viewDef)
{
    math::Vec3 light;
    if ((entity.flags & RF_FULLBRIGHT) != 0)
    {
        light = { 1.0f, 1.0f, 1.0f };
    }
    else
    {
        const mod::ModelInstance * world = mod::GetWorldModel();
        light = (world != nullptr)
            ? mod::SampleWorldLight(*world, { entity.origin[0], entity.origin[1], entity.origin[2] })
            : math::Vec3{ 1.0f, 1.0f, 1.0f };
    }

    // Match ref_gl's dynamic part of R_LightPoint. Static BSP samples already
    // include ps2_light_scale, so apply the same gl_modulate-equivalent scale
    // to transient lights before combining them with the entity's base light.
    static const cvar_t * lightScaleCvar =
        Cvar_Get("ps2_light_scale", "1.0", CVAR_ARCHIVE);
    float lightScale = lightScaleCvar->value;
    if (lightScale < 0.0f) { lightScale = 0.0f; }
    if (lightScale > 8.0f) { lightScale = 8.0f; }

    if (viewDef.dlights != nullptr)
    {
        for (int i = 0; i < viewDef.num_dlights; ++i)
        {
            const dlight_t & dynamicLight = viewDef.dlights[i];
            const float dx = entity.origin[0] - dynamicLight.origin[0];
            const float dy = entity.origin[1] - dynamicLight.origin[1];
            const float dz = entity.origin[2] - dynamicLight.origin[2];
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float contribution =
                (dynamicLight.intensity - distance) * (lightScale / 256.0f);
            if (contribution > 0.0f)
            {
                light.x += contribution * dynamicLight.color[0];
                light.y += contribution * dynamicLight.color[1];
                light.z += contribution * dynamicLight.color[2];
            }
        }
    }

    if ((entity.flags & RF_MINLIGHT) != 0 &&
        light.x <= 0.1f && light.y <= 0.1f && light.z <= 0.1f)
    {
        light = { 0.1f, 0.1f, 0.1f };
    }

    if ((entity.flags & RF_GLOW) != 0)
    {
        const float pulse = 0.1f * std::sin(viewDef.time * 7.0f);
        const float minR = light.x * 0.8f;
        const float minG = light.y * 0.8f;
        const float minB = light.z * 0.8f;
        light.x += pulse;
        light.y += pulse;
        light.z += pulse;
        if (light.x < minR) { light.x = minR; }
        if (light.y < minG) { light.y = minG; }
        if (light.z < minB) { light.z = minB; }
    }

    if ((viewDef.rdflags & RDF_IRGOGGLES) != 0 &&
        (entity.flags & RF_IR_VISIBLE) != 0)
    {
        light = { 1.0f, 0.0f, 0.0f };
    }
    return light;
}

void UpdatePlayerLightLevel(const refdef_t & viewDef)
{
    if ((viewDef.rdflags & RDF_NOWORLDMODEL) != 0)
    {
        return;
    }

    const mod::ModelInstance * world = mod::GetWorldModel();
    if (world == nullptr)
    {
        return;
    }

    math::Vec3 light = mod::SampleWorldLight(
        *world, { viewDef.vieworg[0], viewDef.vieworg[1], viewDef.vieworg[2] });

    // R_LightPoint also includes transient lights. Use the camera position
    // here: this value is sent back in usercmd_t and server-side FindTarget
    // rejects players at light_level <= 5 as effectively invisible.
    if (viewDef.dlights != nullptr)
    {
        for (int i = 0; i < viewDef.num_dlights; ++i)
        {
            const dlight_t & dl = viewDef.dlights[i];
            const float dx = viewDef.vieworg[0] - dl.origin[0];
            const float dy = viewDef.vieworg[1] - dl.origin[1];
            const float dz = viewDef.vieworg[2] - dl.origin[2];
            const float add = (dl.intensity - std::sqrt(dx * dx + dy * dy + dz * dz)) /
                              256.0f;
            if (add > 0.0f)
            {
                light.x += add * dl.color[0];
                light.y += add * dl.color[1];
                light.z += add * dl.color[2];
            }
        }
    }

    float level = 150.0f * std::max(light.x, std::max(light.y, light.z));
    if (level < 0.0f)   { level = 0.0f; }
    if (level > 255.0f) { level = 255.0f; }

    // Match ref_gl's R_SetLightLevel. CL_CreateCmd copies this cvar into the
    // user command byte, and p_client stores it on the player edict for AI.
    static cvar_t * lightLevel = Cvar_Get("r_lightlevel", "0", 0);
    lightLevel->value = level;
    s_drawStats.playerLightLevel = static_cast<int>(level + 0.5f);
}

math::Vec4 AliasVertexColor(const math::Vec3 & modelLight,
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

void DrawAliasModel(const entity_t & entity, const mod::ModelInstance & model,
                    const refdef_t & viewDef)
{
    PS2_Assert(model.type == mod::ModelType::AliasMD2);
    PS2_Assert(model.hunkBase != nullptr);

    const auto * md2 = static_cast<const dmdl_t *>(model.hunkBase);

    int frameIndex = entity.frame;
    if (frameIndex < 0 || frameIndex >= md2->num_frames)
    {
        frameIndex = 0;
    }

    int oldFrameIndex = entity.oldframe;
    if (oldFrameIndex < 0 || oldFrameIndex >= md2->num_frames)
    {
        oldFrameIndex = frameIndex;
    }

    float backLerp = entity.backlerp;
    if (backLerp < 0.0f) { backLerp = 0.0f; }
    if (backLerp > 1.0f) { backLerp = 1.0f; }
    const float frontLerp = 1.0f - backLerp;

    const auto * frame = MD2DataAt<daliasframe_t>(
        *md2, md2->ofs_frames + (frameIndex * md2->framesize));
    const auto * oldFrame = MD2DataAt<daliasframe_t>(
        *md2, md2->ofs_frames + (oldFrameIndex * md2->framesize));

    // GL_DrawAliasFrameLerp's origin interpolation, in model-local axes.
    vec3_t delta = {
        entity.oldorigin[0] - entity.origin[0],
        entity.oldorigin[1] - entity.origin[1],
        entity.oldorigin[2] - entity.origin[2]
    };
    vec3_t angles = { entity.angles[0], entity.angles[1], entity.angles[2] };
    vec3_t axis[3];
    AngleVectors(angles, axis[0], axis[1], axis[2]);

    float move[3] = {
         DotProduct(delta, axis[0]),
        -DotProduct(delta, axis[1]),
         DotProduct(delta, axis[2])
    };
    float frontScale[3];
    float backScale[3];
    for (int i = 0; i < 3; ++i)
    {
        move[i] = backLerp * (move[i] + oldFrame->translate[i])
                + frontLerp * frame->translate[i];
        frontScale[i] = frontLerp * frame->scale[i];
        backScale[i]  = backLerp  * oldFrame->scale[i];
    }

    const auto * triangles = MD2DataAt<dtriangle_t>(*md2, md2->ofs_tris);
    const auto * stVerts   = MD2DataAt<dstvert_t>(*md2, md2->ofs_st);

    const float invGsSkinW = 1.0f / static_cast<float>(GSTextureExtent(md2->skinwidth));
    const float invGsSkinH = 1.0f / static_cast<float>(GSTextureExtent(md2->skinheight));
    const tex::Texture & texture = AliasSkin(entity, model);
    const math::Mat4 & viewProj =
        ((entity.flags & RF_DEPTHHACK) != 0)
            ? s_weaponViewProjMatrix
            : s_viewProjMatrix;
    const bool clipViewWeapon = (entity.flags & RF_DEPTHHACK) != 0;
    const math::Mat4 mvp = EntityModelMatrix(entity) * viewProj;
    const math::Vec3 modelLight = AliasModelLight(entity, viewDef);
    const float shadeAngle = math::DegToRad(-entity.angles[YAW]);
    constexpr float kInvSqrt2 = 0.70710678118f;
    const math::Vec3 shadeVector = {
        std::cos(shadeAngle) * kInvSqrt2,
        std::sin(shadeAngle) * kInvSqrt2,
        kInvSqrt2
    };

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
        out.color =
            AliasVertexColor(modelLight, shadeVector, v.lightnormalindex);
    }
    s_drawStats.aliasUniqueVerts += md2->num_xyz;
    s_drawStats.aliasCorners += md2->num_tris * 3;

    PS2_Assert(s_scratchVertCount == 0);
    if (clipViewWeapon)
    {
        for (int t = 0; t < md2->num_tris; ++t)
        {
            ClipVertex corners[3] = {};
            for (int corner = 0; corner < 3; ++corner)
            {
                const int vertexIndex = triangles[t].index_xyz[corner];
                const int stIndex     = triangles[t].index_st[corner];
                PS2_Assert(vertexIndex >= 0 && vertexIndex < md2->num_xyz);
                PS2_Assert(stIndex >= 0 && stIndex < md2->num_st);

                const PreparedAliasVertex & prepared =
                    s_preparedAliasVerts[vertexIndex];
                const dstvert_t & st = stVerts[stIndex];
                ClipVertex & out = corners[corner];
                out.pos = prepared.pos;
                out.color = prepared.color;
                // MD2 glcmds sample texel centres. The GS extent is rounded
                // up for NPOT skins, hence pixel coordinates divide by it.
                out.st = {
                    (static_cast<float>(st.s) + 0.5f) * invGsSkinW,
                    (static_cast<float>(st.t) + 0.5f) * invGsSkinH,
                    0.0f,
                    0.0f
                };

                // MD2s used to rely on the VU's whole-triangle guard rejection.
                // That hid the missing near-plane clipping until view weapons
                // received their proper, smaller depth-hack projection: a
                // vertex close to w=0 then survived and exploded after the
                // perspective divide. Feed view weapons through the same
                // six-plane EE clipper as the BSP world, interpolating position,
                // texture coordinates and lighting at every cut.
                const math::Vec4 clip = math::Transform(out.pos, mvp);
                const float gw = vu1::kGuardBandNdcLimit * clip.w;
                out.d.f[0] = (clip.w - clip.z) - kClipEpsilon;
                out.d.f[1] = (clip.w + clip.z) - kClipEpsilon;
                out.d.f[2] = (gw - clip.x) - kClipEpsilon;
                out.d.f[3] = (gw + clip.x) - kClipEpsilon;
                out.d.f[4] = (gw - clip.y) - kClipEpsilon;
                out.d.f[5] = (gw + clip.y) - kClipEpsilon;
            }
            SubmitWorldTriangle(corners, mvp, texture);
        }
    }
    else
    {
        for (int t = 0; t < md2->num_tris; ++t)
        {
            if (s_scratchVertCount + 3 > kScratchMaxVerts)
            {
                FlushScratch(mvp, texture);
            }
            for (int corner = 0; corner < 3; ++corner)
            {
                const int vertexIndex = triangles[t].index_xyz[corner];
                const int stIndex     = triangles[t].index_st[corner];
                PS2_Assert(vertexIndex >= 0 && vertexIndex < md2->num_xyz);
                PS2_Assert(stIndex >= 0 && stIndex < md2->num_st);

                const PreparedAliasVertex & prepared =
                    s_preparedAliasVerts[vertexIndex];
                const dstvert_t & st = stVerts[stIndex];
                vu1::DrawVertex & out =
                    s_scratchVerts[s_scratchVertCount++];
                out.x = prepared.pos.x;
                out.y = prepared.pos.y;
                out.z = prepared.pos.z;
                out.w = 1.0f;
                out.rgba = PackFloatColor(prepared.color);
                out.s = (static_cast<float>(st.s) + 0.5f) * invGsSkinW;
                out.t = (static_cast<float>(st.t) + 0.5f) * invGsSkinH;
                out.q = 1.0f;
            }
            ++s_drawStats.trisDrawn;
        }
    }
    FlushScratch(mvp, texture);
}

void DrawSpriteModel(const entity_t & entity, const mod::ModelInstance & model)
{
    PS2_Assert(model.type == mod::ModelType::Sprite);
    PS2_Assert(model.hunkBase != nullptr);

    const auto * sprite = static_cast<const dsprite_t *>(model.hunkBase);
    if (sprite->numframes <= 0)
    {
        return;
    }

    int frameIndex = entity.frame % sprite->numframes;
    if (frameIndex < 0)
    {
        frameIndex += sprite->numframes;
    }
    const dsprframe_t & frame = sprite->frames[frameIndex];
    const tex::Texture & texture =
        (model.skins[frameIndex] != nullptr) ? *model.skins[frameIndex]
                                             : tex::DebugTexture();

    const math::Vec3 origin = { entity.origin[0], entity.origin[1], entity.origin[2] };
    const math::Vec3 right  = { s_rightVec[0], s_rightVec[1], s_rightVec[2] };
    const math::Vec3 up     = { s_upVec[0], s_upVec[1], s_upVec[2] };
    const float left   = -static_cast<float>(frame.origin_x);
    const float rightEdge = static_cast<float>(frame.width - frame.origin_x);
    const float bottom = -static_cast<float>(frame.origin_y);
    const float top    = static_cast<float>(frame.height - frame.origin_y);

    const math::Vec3 corners[4] = {
        origin + up * bottom + right * left,
        origin + up * top    + right * left,
        origin + up * top    + right * rightEdge,
        origin + up * bottom + right * rightEdge
    };
    const float maxS = static_cast<float>(frame.width) /
                       static_cast<float>(GSTextureExtent(texture.width));
    const float maxT = static_cast<float>(frame.height) /
                       static_cast<float>(GSTextureExtent(texture.height));
    constexpr int indices[6] = { 0, 1, 2, 0, 2, 3 };
    const float texS[4] = { 0.0f, 0.0f, maxS, maxS };
    const float texT[4] = { maxT, 0.0f, 0.0f, maxT };
    const bool translucent = (entity.flags & RF_TRANSLUCENT) != 0;
    float alpha = translucent ? entity.alpha : 1.0f;
    if (alpha < 0.0f) { alpha = 0.0f; }
    if (alpha > 1.0f) { alpha = 1.0f; }
    const u32 color = vu1::PackColorRGBA(
        128, 128, 128, static_cast<u32>(alpha * 128.0f + 0.5f));

    PS2_Assert(s_scratchVertCount == 0);
    for (int i = 0; i < 6; ++i)
    {
        const int corner = indices[i];
        vu1::DrawVertex & out = s_scratchVerts[s_scratchVertCount++];
        out.x = corners[corner].x;
        out.y = corners[corner].y;
        out.z = corners[corner].z;
        out.w = 1.0f;
        out.rgba = color;
        out.s = texS[corner];
        out.t = texT[corner];
        out.q = 1.0f;
    }

    s_drawStats.trisDrawn += 2;
    ++s_drawStats.drawBatches;
    vu1::DrawTriangles(s_viewProjMatrix, texture, s_scratchVerts,
                       s_scratchVertCount, translucent);
    s_scratchVertCount = 0;
}

void RenderEntities(const refdef_t & viewDef)
{
    static const cvar_t * s_skipEntities = Cvar_Get("ps2_skip_entities", "0", 0);
    if (s_skipEntities->value != 0.0f)
    {
        return;
    }

    for (int i = 0; i < viewDef.num_entities; ++i)
    {
        const entity_t & entity = viewDef.entities[i];
        if (entity.model == nullptr || (entity.flags & (RF_BEAM | RF_VIEWERMODEL)) != 0)
        {
            continue;
        }

        const auto * model = reinterpret_cast<const mod::ModelInstance *>(entity.model);
        if (model->type == mod::ModelType::Brush)
        {
            DrawBrushModel(entity, *model, viewDef);
        }
        else if (model->type == mod::ModelType::AliasMD2)
        {
            DrawAliasModel(entity, *model, viewDef);
        }
        else if (model->type == mod::ModelType::Sprite)
        {
            DrawSpriteModel(entity, *model);
        }
    }
}

void RenderParticles(const refdef_t & viewDef)
{
    if (viewDef.particles == nullptr || viewDef.num_particles <= 0)
    {
        return;
    }

    const tex::Texture & texture = tex::ParticleTexture();
    const math::Vec3 camera = {
        viewDef.vieworg[0], viewDef.vieworg[1], viewDef.vieworg[2]
    };
    const math::Vec3 forward = {
        s_forwardVec[0], s_forwardVec[1], s_forwardVec[2]
    };
    const math::Vec3 baseUp = {
        s_upVec[0] * 1.5f, s_upVec[1] * 1.5f, s_upVec[2] * 1.5f
    };
    const math::Vec3 baseRight = {
        s_rightVec[0] * 1.5f, s_rightVec[1] * 1.5f, s_rightVec[2] * 1.5f
    };

    PS2_Assert(s_scratchVertCount == 0);
    for (int i = 0; i < viewDef.num_particles; ++i)
    {
        const particle_t & particle = viewDef.particles[i];
        if (particle.alpha <= 0.0f)
        {
            continue;
        }
        if (s_scratchVertCount + 3 > kScratchMaxVerts)
        {
            ++s_drawStats.drawBatches;
            vu1::DrawTriangles(s_viewProjMatrix, texture, s_scratchVerts,
                               s_scratchVertCount, true);
            s_scratchVertCount = 0;
        }

        const math::Vec3 origin = {
            particle.origin[0], particle.origin[1], particle.origin[2]
        };
        const float distance = math::Dot(origin - camera, forward);
        const float scale = (distance < 20.0f) ? 1.0f
                                               : 1.0f + distance * 0.004f;
        const math::Vec3 points[3] = {
            origin,
            origin + baseUp * scale,
            origin + baseRight * scale
        };

        const u32 palette = global_palette[particle.color & 0xFF];
        const u32 r = palette & 0xFFu;
        const u32 g = (palette >> 8) & 0xFFu;
        const u32 b = (palette >> 16) & 0xFFu;
        float alpha = particle.alpha;
        if (alpha > 1.0f) { alpha = 1.0f; }
        const u32 packedColor = vu1::PackColorRGBA(
            r, g, b, static_cast<u32>(alpha * 128.0f + 0.5f));

        constexpr float texS[3] = { 0.0625f, 1.0f, 0.0625f };
        constexpr float texT[3] = { 0.0625f, 0.0625f, 1.0f };
        for (int corner = 0; corner < 3; ++corner)
        {
            vu1::DrawVertex & out = s_scratchVerts[s_scratchVertCount++];
            out.x = points[corner].x;
            out.y = points[corner].y;
            out.z = points[corner].z;
            out.w = 1.0f;
            out.rgba = packedColor;
            out.s = texS[corner];
            out.t = texT[corner];
            out.q = 1.0f;
        }
        ++s_drawStats.trisDrawn;
    }

    if (s_scratchVertCount != 0)
    {
        ++s_drawStats.drawBatches;
        vu1::DrawTriangles(s_viewProjMatrix, texture, s_scratchVerts,
                           s_scratchVertCount, true);
        s_scratchVertCount = 0;
    }
}

// ------------------------------------------------------------------------------------------------
// World model pass
// ------------------------------------------------------------------------------------------------

void RenderWorldModel(const refdef_t & viewDef)
{
    static const cvar_t * s_skipWorld = Cvar_Get("ps2_skip_world", "0", 0);

    s_alphaSurfaces = nullptr;

    if (viewDef.rdflags & RDF_NOWORLDMODEL)
    {
        return; // Menu/loading screens render no world.
    }
    if (s_skipWorld->value != 0.0f)
    {
        return; // Debug: skip the world pass entirely.
    }

    const mod::ModelInstance * world = mod::GetWorldModel();
    PS2_AssertMsg(world != nullptr, "RenderFrame without a world model!");

    SetUpViewClusters(viewDef, *world);
    MarkLeaves(*world);
    RecursiveWorldNode(viewDef, *world, world->nodes);
    DrawTextureChains(s_viewProjMatrix);
}

} // namespace

// ------------------------------------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------------------------------------

void BeginRegistration()
{
    ClearLitTriangleCaches();
    s_drawStats = {};

    // New map: forget the previous map's clusters so the first frame re-marks.
    s_viewCluster     = kInvalidCluster;
    s_viewCluster2    = kInvalidCluster;
    s_oldViewCluster  = kInvalidCluster;
    s_oldViewCluster2 = kInvalidCluster;
}

const DrawStats & GetDrawStats()
{
    return s_drawStats;
}

void RenderFrame(const refdef_t & viewDef)
{
    PS2_Assert(viewDef.width > 0 && viewDef.height > 0);

    s_drawStats = {};

    // Alpha.12 deliberately releases the previous renderer world before the
    // integrated server reads the next BSP. SCR_UpdateScreen can still request
    // a loading-plaque frame in that short interval. Skip the complete 3D pass
    // until CL_PrepRefresh registers the new world: old entity model pointers
    // are invalid too, so checking only inside RenderWorldModel is insufficient.
    if ((viewDef.rdflags & RDF_NOWORLDMODEL) == 0 &&
        mod::GetWorldModel() == nullptr)
    {
        return;
    }

    timing::Stamp phaseStart = timing::Now();
    SetupFrame(viewDef);
    s_drawStats.setupMicros = timing::ElapsedMicros(phaseStart);

    phaseStart = timing::Now();
    RenderWorldModel(viewDef);
    s_drawStats.worldMicros = timing::ElapsedMicros(phaseStart);

    phaseStart = timing::Now();
    RenderEntities(viewDef);
    s_drawStats.entityMicros = timing::ElapsedMicros(phaseStart);

    phaseStart = timing::Now();
    RenderParticles(viewDef);
    s_drawStats.particleMicros = timing::ElapsedMicros(phaseStart);
    s_drawStats.lightCacheBytes = s_litCacheBytes;
    UpdatePlayerLightLevel(viewDef);

    // Later milestones continue here: translucent surfaces/entities, sky,
    // water and dynamic world lights.
}

} // namespace ps2::view
