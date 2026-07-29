/* ================================================================================================
 * File: model.cpp
 * Brief: Quake 2 3D model format caching. Loading itself lives in model_load.cpp;
 *        this file owns the pool of loaded models, the name lookup, and the
 *        level registration lifecycle (mirrors the TextureCache in texture.cpp).
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/renderer/model.h"
#include "ps2/renderer/model_load.h"
#include "ps2/renderer/texture.h"
#include "ps2/small_pool.h"
#include "ps2/hash.h"
#include "ps2/common.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

extern "C" {
    #include "common/q_files.h" // IDBSPHEADER / dsprite_t / dmdl_t / MAX_SKINNAME
}

namespace ps2::mod {
namespace {

// Extra debug printing for cache hits / evictions.
constexpr bool kVerboseModelCache = false;

static float s_lightStyleRgb[MAX_LIGHTSTYLES][3];
static bool  s_lightStylesInitialized = false;

void ResetLightStyles()
{
    for (int style = 0; style < MAX_LIGHTSTYLES; ++style)
    {
        s_lightStyleRgb[style][0] = 1.0f;
        s_lightStyleRgb[style][1] = 1.0f;
        s_lightStyleRgb[style][2] = 1.0f;
    }
    s_lightStylesInitialized = true;
}

float StaticLightScale()
{
    // Equivalent to the original renderer's gl_modulate=1. Texture intensity
    // is applied separately by the lit palette in gs.cpp, as ref_gl does.
    // Archived and read live so a map reload rebuilds all samples consistently.
    static const cvar_t * scale = Cvar_Get("ps2_light_scale", "1.0", CVAR_ARCHIVE);
    if (scale->value < 0.0f) { return 0.0f; }
    if (scale->value > 8.0f) { return 8.0f; }
    return scale->value;
}

PS2MemTag MemTagForType(ModelType type)
{
    switch (type)
    {
    case ModelType::Brush    : return MEMTAG_MDL_WORLD;
    case ModelType::Sprite   : return MEMTAG_MDL_SPRITE;
    case ModelType::AliasMD2 : return MEMTAG_MDL_ALIAS;
    }
    return MEMTAG_MDL_WORLD; // Unreachable; keeps GCC's -Wreturn-type happy.
}

// Owns the model pool and the name lookup. Internal singleton (s_cache);
// the module API at the bottom of the file is the public face.
class ModelCache final
{
public:
    void Init();

    void BeginRegistration(const char * mapName);
    void EndRegistration();

    const ModelInstance * Find(const char * name);
    const ModelInstance * WorldModel() { return m_worldModel; }

private:
    const ModelInstance * LoadModel(const char * name);
    void LoadWorldModel(const char * mapName);

    const ModelInstance * FindInlineModel(const char * name);
    void SetUpInlineModels(ModelInstance & world);

    void ReferenceAllTextures(ModelInstance & mdl);
    void Unload(u16 slot);

    // A level references the world plus a few hundred entity/sprite models.
    // Fixed-size pool: running out is a Sys_Error telling you to bump this.
    static constexpr u32 kMaxModels = 512;
    using ModelPool = SmallPool<ModelInstance, kMaxModels>;

    ModelPool m_modelPool;

    // Inline (*N) brush submodels of the current map. They alias the world
    // model's geometry and are set up on each world load, so they live outside
    // the pool and are never looked up by name. Bounds submodels per map.
    static constexpr u32 kMaxInlineModels = 256;
    ModelInstance m_inlineModels[kMaxInlineModels] = {};

    // Lookup: FNV-1a hash of the model path -> pool slot.
    std::unordered_map<u64, u16> m_lookup;

    // Level load/change cycle counter; models stamped with an older value are
    // the ones EndRegistration() frees.
    u32 m_regSequence = 1;

    // Currently loaded world map (a pointer into m_modelPool).
    const ModelInstance * m_worldModel = nullptr;
};

void ModelCache::Init()
{
    m_modelPool.Init(); // One-shot; asserts if called twice.
    m_lookup.reserve(kMaxModels);
    Com_Printf("Model cache initialised.\n");
}

const ModelInstance * ModelCache::Find(const char * const name)
{
    PS2_Assert(name != nullptr && *name != '\0');

    // Inline models come from the world's submodels, not the pool.
    if (name[0] == '*')
    {
        return FindInlineModel(name);
    }

    const auto it = m_lookup.find(HashStr64(name));
    if (it != m_lookup.end())
    {
        ModelInstance & mdl = m_modelPool.Slot(it->second);

        // 64-bit FNV-1a collisions are vanishingly rare, but verify the name.
        PS2_AssertMsg(std::strcmp(mdl.name, name) == 0, "Model lookup hash collision!");

        if (kVerboseModelCache)
        {
            Com_DPrintf("Model '%s' already in cache.\n", name);
        }

        mdl.regSequence = m_regSequence; // Still referenced this cycle.
        ReferenceAllTextures(mdl);       // Keep its textures alive too.
        return &mdl;
    }

    return LoadModel(name);
}

const ModelInstance * ModelCache::LoadModel(const char * const name)
{
    void * fileData = nullptr;
    const int fileLen = FS_LoadFile(name, &fileData);
    if (fileData == nullptr || fileLen < static_cast<int>(sizeof(u32)))
    {
        Com_Printf("WARNING: Unable to load model '%s'! Failed to open file.\n", name);
        if (fileData != nullptr) { FS_FreeFile(fileData); }
        return nullptr;
    }

    // The first 4 bytes identify the format.
    const u32 id = *static_cast<const u32 *>(fileData);
    ModelType type;
    switch (id)
    {
    case IDBSPHEADER    : type = ModelType::Brush;    break;
    case IDSPRITEHEADER : type = ModelType::Sprite;   break;
    case IDALIASHEADER  : type = ModelType::AliasMD2; break;
    default :
        Com_Printf("ERROR: ModelCache: Unknown file id (0x%X) for '%s'!\n", id, name);
        FS_FreeFile(fileData);
        return nullptr;
    }

    const u16 slot = m_modelPool.Alloc();
    if (slot == ModelPool::kInvalidIndex)
    {
        Sys_Error("Out of model cache slots for '%s'! Bump ModelCache::kMaxModels (%u).", name, kMaxModels);
    }

    ModelInstance & mdl = m_modelPool.Slot(slot);
    std::snprintf(mdl.name, sizeof(mdl.name), "%s", name);
    mdl.type        = type;
    mdl.regSequence = m_regSequence;

    bool ok = false;
    switch (type)
    {
    case ModelType::Brush :
        ok = LoadBrushModel(mdl, fileData, fileLen);
        if (ok) { SetUpInlineModels(mdl); }
        break;
    case ModelType::Sprite :
        ok = LoadSpriteModel(mdl, fileData, fileLen);
        break;
    case ModelType::AliasMD2 :
        ok = LoadAliasMD2Model(mdl, fileData, fileLen);
        break;
    }

    FS_FreeFile(fileData);

    if (!ok)
    {
        Unload(slot); // Frees any hunk and returns the slot to the pool.
        return nullptr;
    }

    m_lookup.emplace(HashStr64(name), slot);

    if (kVerboseModelCache)
    {
        Com_DPrintf("Loaded model '%s'.\n", name);
    }
    return &mdl;
}

const ModelInstance * ModelCache::FindInlineModel(const char * const name)
{
    const int idx = std::atoi(name + 1);
    if (idx < 1 || idx >= static_cast<int>(kMaxInlineModels) ||
        m_worldModel == nullptr || idx >= m_worldModel->numSubModels)
    {
        Com_Printf("ERROR: ModelCache: Bad inline model number (%i) or null world model.\n", idx);
        return nullptr;
    }
    return &m_inlineModels[idx];
}

void ModelCache::SetUpInlineModels(ModelInstance & world)
{
    if (world.numSubModels > static_cast<int>(kMaxInlineModels))
    {
        Sys_Error("Map '%s' has too many submodels (%i)! Bump ModelCache::kMaxInlineModels (%u).",
                  world.name, world.numSubModels, kMaxInlineModels);
    }

    for (int i = 0; i < world.numSubModels; ++i)
    {
        const SubModelInfo & sm = world.subModels[i];
        ModelInstance & inl = m_inlineModels[i];

        // Alias the world's geometry, then override the per-submodel bounds and
        // surface/node range. Inline models never own the hunk (the world does),
        // so clear it to avoid a double free.
        inl = world;
        inl.hunkBase = nullptr;
        inl.hunkSize = 0;
        inl.isInline = true;

        inl.firstModelSurface = sm.firstFace;
        inl.numModelSurfaces  = sm.numFaces;
        inl.firstNode         = sm.headNode;
        inl.mins              = sm.mins;
        inl.maxs              = sm.maxs;
        inl.radius            = sm.radius;

        // Quake 2's on-disk dmodel_t carries no leaf count, and inline models
        // never walk the leaf array; only the world's LoadLeafs count matters.
        inl.numLeafs = 0;

        if (inl.firstNode >= world.numNodes)
        {
            Sys_Error("Inline model %i of '%s' has a bad first node!", i, world.name);
        }

        // Submodel 0 is the world itself; fold its ranges back into the world.
        // numLeafs stays untouched: the world keeps the full count from
        // LoadLeafs, or MarkLeaves would have no leafs to stamp visible.
        if (i == 0)
        {
            world.firstModelSurface = sm.firstFace;
            world.numModelSurfaces  = sm.numFaces;
            world.firstNode         = sm.headNode;
            world.mins              = sm.mins;
            world.maxs              = sm.maxs;
            world.radius            = sm.radius;
        }
    }
}

void ModelCache::ReferenceAllTextures(ModelInstance & mdl)
{
    switch (mdl.type)
    {
    case ModelType::Brush:
        // Re-stamp the wall textures so EndRegistration keeps them (no reload).
        for (int i = 0; i < mdl.numTexInfos; ++i)
        {
            if (mdl.texInfos[i].texture != nullptr)
            {
                tex::TouchTexture(*mdl.texInfos[i].texture);
            }
        }
        break;

    case ModelType::Sprite:
        {
            const auto * sprite = static_cast<const dsprite_t *>(mdl.hunkBase);
            for (int i = 0; i < sprite->numframes; ++i)
            {
                mdl.skins[i] = tex::Find(sprite->frames[i].name, tex::ImageType::Sprite);
            }
            break;
        }

    case ModelType::AliasMD2:
        {
            const auto * md2 = static_cast<const dmdl_t *>(mdl.hunkBase);
            for (int i = 0; i < md2->num_skins; ++i)
            {
                const char * skinName = reinterpret_cast<const char *>(md2) + md2->ofs_skins + (i * MAX_SKINNAME);
                mdl.skins[i] = tex::Find(skinName, tex::ImageType::Skin);
            }
            mdl.numFrames = md2->num_frames;
            break;
        }
    }
}

void ModelCache::Unload(u16 slot)
{
    ModelInstance & mdl = m_modelPool.Slot(slot);
    if (mdl.hunkBase != nullptr)
    {
        PS2_MemFree(mdl.hunkBase, mdl.hunkSize, MemTagForType(mdl.type));
    }
    m_modelPool.Free(slot); // Zeroes the slot.
}

void ModelCache::BeginRegistration(const char * const mapName)
{
    PS2_Assert(mapName != nullptr && *mapName != '\0');

    // Bump first, so everything found or loaded this cycle is stamped current
    // and survives EndRegistration().
    ++m_regSequence;
    LoadWorldModel(mapName);
}

void ModelCache::LoadWorldModel(const char * const mapName)
{
    char fullName[MAX_QPATH];
    std::snprintf(fullName, sizeof(fullName), "maps/%s.bsp", mapName);

    // Free the old map up front if we are switching to a different one. This
    // guarantees the world's inline models are rebuilt against fresh geometry.
    if (m_worldModel != nullptr && std::strcmp(m_worldModel->name, fullName) != 0)
    {
        if (kVerboseModelCache)
        {
            Com_DPrintf("Unloading current map '%s'...\n", m_worldModel->name);
        }

        const auto it = m_lookup.find(HashStr64(m_worldModel->name));
        if (it != m_lookup.end())
        {
            Unload(it->second);
            m_lookup.erase(it);
        }
        m_worldModel = nullptr;
    }

    const ModelInstance * const world = Find(fullName);
    if (world == nullptr)
    {
        Sys_Error("ModelCache: Unable to load level map '%s'!", fullName);
    }
    m_worldModel = world;
}

void ModelCache::EndRegistration()
{
    // Free the models this cycle no longer references.
    int freedCount = 0;
    for (auto it = m_lookup.begin(); it != m_lookup.end(); )
    {
        ModelInstance & mdl = m_modelPool.Slot(it->second);
        if (mdl.regSequence == m_regSequence)
        {
            ++it;
            continue;
        }

        if (kVerboseModelCache)
        {
            Com_DPrintf("Freeing unused model '%s'\n", mdl.name);
        }

        if (&mdl == m_worldModel)
        {
            m_worldModel = nullptr;
        }

        Unload(it->second);
        it = m_lookup.erase(it);
        ++freedCount;
    }

    if (freedCount > 0)
    {
        Com_DPrintf("Model cache: freed %d unused models.\n", freedCount);
    }
}

static ModelCache s_cache;

bool RecursiveLightPoint(const ModelInstance & world, const ModelNode * node,
                         const Vec3 & start, const Vec3 & end, Vec3 & light)
{
    if (node == nullptr || node->contents != -1)
    {
        return false;
    }

    const cplane_s & plane = *node->plane;
    const Vec3 normal = { plane.normal[0], plane.normal[1], plane.normal[2] };
    const float front = math::Dot(start, normal) - plane.dist;
    const float back  = math::Dot(end, normal) - plane.dist;
    const int side = front < 0.0f;

    if ((back < 0.0f) == (side != 0))
    {
        return RecursiveLightPoint(world, node->children[side], start, end, light);
    }

    const float fraction = front / (front - back);
    const Vec3 mid = start + (end - start) * fraction;

    if (RecursiveLightPoint(world, node->children[side], start, mid, light))
    {
        return true;
    }

    for (int i = 0; i < node->numSurfaces; ++i)
    {
        const ModelSurface & surface = world.surfaces[node->firstSurface + i];
        if (HasFlag(surface.flags, SurfaceFlags::DrawSky) ||
            HasFlag(surface.flags, SurfaceFlags::DrawTurb))
        {
            continue;
        }

        const ModelTexInfo & texInfo = *surface.texInfo;
        const float s = mid.x * texInfo.vecs[0][0] +
                        mid.y * texInfo.vecs[0][1] +
                        mid.z * texInfo.vecs[0][2] + texInfo.vecs[0][3];
        const float t = mid.x * texInfo.vecs[1][0] +
                        mid.y * texInfo.vecs[1][1] +
                        mid.z * texInfo.vecs[1][2] + texInfo.vecs[1][3];
        const float ds = s - static_cast<float>(surface.textureMins[0]);
        const float dt = t - static_cast<float>(surface.textureMins[1]);
        if (ds < 0.0f || dt < 0.0f ||
            ds > static_cast<float>(surface.extents[0]) ||
            dt > static_cast<float>(surface.extents[1]))
        {
            continue;
        }

        if (surface.samples == nullptr)
        {
            light = { 0.0f, 0.0f, 0.0f };
            return true;
        }

        const u32 packed = SampleStaticLight(surface, ds * (1.0f / 16.0f),
                                             dt * (1.0f / 16.0f));
        light = {
            static_cast<float>( packed        & 0xFFu) * (1.0f / 128.0f),
            static_cast<float>((packed >> 8)  & 0xFFu) * (1.0f / 128.0f),
            static_cast<float>((packed >> 16) & 0xFFu) * (1.0f / 128.0f)
        };
        return true;
    }

    return RecursiveLightPoint(world, node->children[side ^ 1], mid, end, light);
}

} // namespace

// ------------------------------------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------------------------------------

u32 SampleStaticLight(const ModelSurface & surface, float sampleS, float sampleT)
{
    if (!s_lightStylesInitialized)
    {
        ResetLightStyles();
    }

    if (surface.samples == nullptr)
    {
        return 0x80808080u; // No light data: fullbright, like R_BuildLightMap.
    }

    const int width  = (surface.extents[0] >> 4) + 1;
    const int height = (surface.extents[1] >> 4) + 1;
    const int texels = width * height;

    if (sampleS < 0.0f) { sampleS = 0.0f; }
    if (sampleT < 0.0f) { sampleT = 0.0f; }
    const float maxS = static_cast<float>(width  - 1);
    const float maxT = static_cast<float>(height - 1);
    if (sampleS > maxS) { sampleS = maxS; }
    if (sampleT > maxT) { sampleT = maxT; }

    const int s0 = static_cast<int>(std::floor(sampleS));
    const int t0 = static_cast<int>(std::floor(sampleT));
    const int s1 = (s0 + 1 < width)  ? s0 + 1 : s0;
    const int t1 = (t0 + 1 < height) ? t0 + 1 : t0;
    const float fracS = sampleS - static_cast<float>(s0);
    const float fracT = sampleT - static_cast<float>(t0);

    float lightRgb[3] = {};
    int styleCount = 0;
    const float lightScale = StaticLightScale();
    for (int style = 0; style < kMaxLightmaps && surface.styles[style] != 255; ++style)
    {
        const u8 * map = surface.samples + style * texels * 3;
        const u8 * c00 = map + (t0 * width + s0) * 3;
        const u8 * c10 = map + (t0 * width + s1) * 3;
        const u8 * c01 = map + (t1 * width + s0) * 3;
        const u8 * c11 = map + (t1 * width + s1) * 3;

        for (int channel = 0; channel < 3; ++channel)
        {
            const float top = static_cast<float>(c00[channel]) +
                              (static_cast<float>(c10[channel]) - static_cast<float>(c00[channel])) * fracS;
            const float bottom = static_cast<float>(c01[channel]) +
                                 (static_cast<float>(c11[channel]) - static_cast<float>(c01[channel])) * fracS;
            const int styleIndex = surface.styles[style];
            lightRgb[channel] += (top + (bottom - top) * fracT) * lightScale *
                                 s_lightStyleRgb[styleIndex][channel];
        }
        ++styleCount;
    }

    if (styleCount == 0)
    {
        return 0x80808080u;
    }

    // Match R_BuildLightMap: preserve hue when an accumulated channel exceeds
    // full intensity by normalising all channels together.
    float maxLight = lightRgb[0];
    if (lightRgb[1] > maxLight) { maxLight = lightRgb[1]; }
    if (lightRgb[2] > maxLight) { maxLight = lightRgb[2]; }
    if (maxLight > 255.0f)
    {
        const float normalize = 255.0f / maxLight;
        lightRgb[0] *= normalize;
        lightRgb[1] *= normalize;
        lightRgb[2] *= normalize;
    }

    const u32 r = static_cast<u32>(lightRgb[0] * (128.0f / 255.0f) + 0.5f);
    const u32 g = static_cast<u32>(lightRgb[1] * (128.0f / 255.0f) + 0.5f);
    const u32 b = static_cast<u32>(lightRgb[2] * (128.0f / 255.0f) + 0.5f);
    return r | (g << 8) | (b << 16) | (0x80u << 24);
}

u32 StaticLightStyleKey(const ModelSurface & surface)
{
    if (!s_lightStylesInitialized)
    {
        ResetLightStyles();
    }

    constexpr u32 kFnvOffset = 2166136261u;
    constexpr u32 kFnvPrime  = 16777619u;
    u32 hash = kFnvOffset;
    auto mix = [&hash](u32 value)
    {
        for (int byte = 0; byte < 4; ++byte)
        {
            hash ^= (value >> (byte * 8)) & 0xFFu;
            hash *= kFnvPrime;
        }
    };

    float lightScale = StaticLightScale();
    u32 scaleBits;
    std::memcpy(&scaleBits, &lightScale, sizeof(scaleBits));
    mix(scaleBits);

    for (int slot = 0; slot < kMaxLightmaps && surface.styles[slot] != 255; ++slot)
    {
        const int style = surface.styles[slot];
        mix(static_cast<u32>(style));
        for (int channel = 0; channel < 3; ++channel)
        {
            u32 valueBits;
            std::memcpy(&valueBits, &s_lightStyleRgb[style][channel], sizeof(valueBits));
            mix(valueBits);
        }
    }
    return hash;
}

void SetLightStyles(const lightstyle_t * styles)
{
    if (styles == nullptr)
    {
        ResetLightStyles();
        return;
    }

    for (int style = 0; style < MAX_LIGHTSTYLES; ++style)
    {
        s_lightStyleRgb[style][0] = styles[style].rgb[0];
        s_lightStyleRgb[style][1] = styles[style].rgb[1];
        s_lightStyleRgb[style][2] = styles[style].rgb[2];
    }
    s_lightStylesInitialized = true;
}

Vec3 SampleWorldLight(const ModelInstance & world, const Vec3 & point)
{
    if (world.type != ModelType::Brush || world.lightData == nullptr ||
        world.nodes == nullptr || world.firstNode < 0 ||
        world.firstNode >= world.numNodes)
    {
        return { 1.0f, 1.0f, 1.0f };
    }

    const Vec3 end = { point.x, point.y, point.z - 2048.0f };
    Vec3 light = {};
    if (!RecursiveLightPoint(world, world.nodes + world.firstNode, point, end, light))
    {
        return { 0.0f, 0.0f, 0.0f };
    }
    return light;
}

void Init()
{
    s_cache.Init();
}

void BeginRegistration(const char * mapName)
{
    s_cache.BeginRegistration(mapName);
}

void EndRegistration()
{
    s_cache.EndRegistration();
}

const ModelInstance * Find(const char * name)
{
    return s_cache.Find(name);
}

const ModelInstance * GetWorldModel()
{
    return s_cache.WorldModel();
}

} // namespace ps2::mod
