/* ================================================================================================
 * File: gs.cpp
 * Brief: Double-buffered Graphics Synthesizer front-end. See gs.h.
 *
 *  Modelled on the ps2sdk libdraw "font"/"cube" samples: two 32-bit framebuffers
 *  in VRAM, one displayed while the other is drawn, using the two GS drawing
 *  contexts. draw_setup_environment programs each context so screen coordinates
 *  are direct top-left pixels.
 *
 *  Frame structure: BeginFrame() clears color and depth immediately (its own
 *  DMA transfer). 2D and 3D then draw in any order. 2D primitives accumulate
 *  into a deferred "pending batch" (always-pass z-test, so it lands on top);
 *  the first primitive after a flush opens it lazily. The batch is flushed to
 *  the GS - sent and waited on - automatically at each 2D->3D boundary (the
 *  VU1 path calls FlushPending2D() before drawing over PATH1, so its triangles
 *  land under any 2D issued afterwards) and once more by EndFrame(). Flushing
 *  at the boundary also keeps the deferred draws' textures resident: they are
 *  consumed before a later 3D upload can evict the VRAM they sample.
 *
 *  Textures stream on first bind into the VRAM left over after the
 *  framebuffers and z-buffer, managed by vram.cpp. While a texture
 *  is resident, binding it is just a TEX0/TEX1 register write - no DMA upload,
 *  no pipeline flush. When the heap fills, the least-recently-bound textures
 *  are evicted; uploads over reused VRAM first sync the GS so queued draws
 *  keep sampling the old texels, not the new ones.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include "ps2/renderer/gs.h"
#include "ps2/renderer/render_packet.h"
#include "ps2/renderer/timing.h"
#include "ps2/renderer/texture.h"
#include "ps2/renderer/vram.h"
#include "ps2/builtin/builtin.h" // global_palette

#include <dma.h>
#include <gs_psm.h>
#include <graph.h>
#include <kernel.h> // SyncDCache
#include <draw.h>
#include <draw2d.h>
#include <draw_buffers.h>
#include <draw_sampling.h>
#include <cmath>

namespace ps2::gs {
namespace {

constexpr int kWidth      = 640;
constexpr int kNtscHeight = 448;
constexpr int kPalHeight  = 512;

// Quake world textures are approximately one texel per world unit. With a
// 640-pixel view, that reaches a roughly 1:1 texel/pixel ratio a few hundred
// units from the camera. The GS formula is log2(1/Q) + K and our Q is 1/viewZ,
// so K=-8 keeps nearby surfaces at level 0 and advances about one mip per
// distance doubling instead of immediately clamping everything to level 3.
constexpr float kMipmapLodBias = -8.0f;

// Selected from the console ROM region during Init. PAL exposes 64 more
// active lines; rendering an NTSC-height buffer there leaves a black strip.
static int s_height = kNtscHeight;

// Per-frame packet headroom. Worst observed 2D load is a full console of text
// (~2200 glyphs at 4 qwords each); 32K qwords (512 KB) leaves ample margin.
constexpr int kPacketQwords = 32768;

// Scratch packet for synchronous texture uploads (DMA chain tags only; the
// pixel data is referenced in place).
constexpr int kTexUploadQwords = 128;

// The color+depth clear, sent as its own transfer at the top of each frame.
constexpr int kClearQwords = 128;

static framebuffer_t s_frame[2];
static zbuffer_t     s_zbuffer;

static RenderPacket s_framePacket[2];   // double-buffered per-frame packets
static RenderPacket s_texUploadPacket;  // scratch packet for texture uploads
static RenderPacket s_clearPacket;      // per-frame color+depth clear
static TimingStats s_timingStats = {};

static int s_drawCtx   = 1; // which framebuffer/context we render into this frame
static int s_packetIdx = 0; // which frame packet is being filled

static bool s_frameStarted = false;
static bool s_in2D         = false;

// Screen clean color. Distinctive dark blue.
static u8 s_clear[3] = { 0x20, 0x20, 0x38 };

// Set when a VRAM allocation evicted a texture: draws already queued (or still
// rasterising) may reference the freed range, so the next upload must sync the
// GS first. Sticky until a GS-idle point - a block freed early in the frame
// can be handed out later without a new eviction.
static bool s_vramReuseHazard = false;

// Texture bound in the current 2D section.
static const tex::Texture * s_currentTex = nullptr;

// The global-palette CLUT: Quake's shared 8-bit palette, uploaded once at Init
// to a fixed VRAM spot (16x16 PSMCT32 image = 4 blocks) that every Palette8
// texture's TEX0 points at. s_clutData holds the entries in the GS's CSM1
// arrangement: within each 32-entry group the two middle 8-entry blocks swap
// (index bits 3 and 4 exchange).
alignas(16) static u32 s_clutData[256];
alignas(16) static u32 s_litClutData[256];
static vram::Address s_clutVramAddr    = vram::Address::Invalid;
static vram::Address s_litClutVramAddr = vram::Address::Invalid;

u8 GammaChannel(u8 value, float gamma)
{
    const float corrected =
        255.0f * std::pow((static_cast<float>(value) + 0.5f) / 255.5f, gamma) + 0.5f;
    if (corrected <= 0.0f) { return 0; }
    if (corrected >= 255.0f) { return 255; }
    return static_cast<u8>(corrected);
}

u32 AdjustPaletteColor(u32 color, float gamma, float intensity)
{
    const auto channel = [gamma, intensity](u32 value) -> u32 {
        float scaled = static_cast<float>(value) * intensity;
        if (scaled > 255.0f) { scaled = 255.0f; }
        return GammaChannel(static_cast<u8>(scaled), gamma);
    };

    const u32 r = channel( color        & 0xFFu);
    const u32 g = channel((color >> 8)  & 0xFFu);
    const u32 b = channel((color >> 16) & 0xFFu);
    return r | (g << 8) | (b << 16) | (color & 0xFF000000u);
}

// Pixel stride the texture occupies VRAM with (the TEX0 TBW and transfer DBW).
// 8-bit textures must use a multiple of 128 (TBW must be even for PSMT8/4);
// other formats use their width as-is.
inline int TextureStridePixels(int width, int psm)
{
    if (psm == GS_PSM_8)
    {
        return (width + 127) & ~127;
    }
    return width;
}

inline int MipDimension(int base, int level)
{
    const int dimension = base >> level;
    return dimension > 0 ? dimension : 1;
}

inline int TextureVramWords(const tex::Texture & texture, int psm)
{
    int words = 0;
    for (int level = 0; level < texture.mipLevels; ++level)
    {
        words += vram::TextureFootprintWords(MipDimension(texture.storageWidth, level),
                                             MipDimension(texture.storageHeight, level), psm);
    }
    return words;
}

inline RenderPacket & FramePacket()
{
    return s_framePacket[s_packetIdx];
}

} // namespace

// ------------------------------------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------------------------------------

int Width()  { return kWidth; }
int Height() { return s_height; }

int CurrentContext()
{
    return s_drawCtx;
}

int DepthTestMethod()
{
    return static_cast<int>(s_zbuffer.method);
}

void SetClearColor(u8 r, u8 g, u8 b)
{
    s_clear[0] = r;
    s_clear[1] = g;
    s_clear[2] = b;
}

void Init()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    s_height = (graph_get_region() == GRAPH_MODE_PAL) ? kPalHeight : kNtscHeight;

    // Two 32-bit framebuffers.
    // TODO: Consider more compact framebuffer formats to leave more vram for textures (RGB16?).
    s_frame[0].width   = kWidth;
    s_frame[0].height  = static_cast<unsigned int>(s_height);
    s_frame[0].mask    = 0;
    s_frame[0].psm     = GS_PSM_32;
    s_frame[0].address = static_cast<unsigned int>(graph_vram_allocate(kWidth, s_height, GS_PSM_32, GRAPH_ALIGN_PAGE));

    s_frame[1]         = s_frame[0];
    s_frame[1].address = static_cast<unsigned int>(graph_vram_allocate(kWidth, s_height, GS_PSM_32, GRAPH_ALIGN_PAGE));

    // Z-buffer for the 3D world; larger depth = closer (the projection maps the
    // near plane to 0xFFFF), hence GREATER_EQUAL. A 16-bit z-buffer preserves
    // as much VRAM as possible for textures. It must be the *signed* format:
    // the GS pairs
    // PSMCT32 color with the Z32/Z24/Z16S column - plain Z16 only works with
    // CT16 color buffers.
    s_zbuffer.enable  = DRAW_ENABLE;
    s_zbuffer.method  = ZTEST_METHOD_GREATER_EQUAL;
    s_zbuffer.mask    = 0;
    s_zbuffer.zsm     = GS_ZBUF_16S;
    s_zbuffer.address = static_cast<unsigned int>(graph_vram_allocate(kWidth, s_height, GS_ZBUF_16S, GRAPH_ALIGN_PAGE));

    // Two fixed palette CLUTs: UI/effects use gamma only, while world/model
    // textures use ref_gl's intensity preprocessing as well. The streamed
    // texture heap begins after both, rounded to a page.
    const int clutVramAddr =
        graph_vram_allocate(16, 16, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    const int litClutVramAddr =
        graph_vram_allocate(16, 16, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    vram::Init((litClutVramAddr + ArrayLength(s_litClutData) + 2047) & ~2047);
    s_clutVramAddr = vram::Address(clutVramAddr);
    s_litClutVramAddr = vram::Address(litClutVramAddr);

    // Display framebuffer 0 first; auto-detects NTSC/PAL.
    graph_initialize(static_cast<int>(s_frame[0].address), kWidth, s_height, GS_PSM_32, 0, 0);

    s_framePacket[0].Init(kPacketQwords);
    s_framePacket[1].Init(kPacketQwords);
    s_texUploadPacket.Init(kTexUploadQwords);
    s_clearPacket.Init(kClearQwords);

    // Program both drawing contexts: context 0 -> frame 0, context 1 -> frame 1.
    // The environment defaults texture wrapping to CLAMP; Quake's DrawTileClear
    // addresses texels in screen space and needs REPEAT.
    texwrap_t wrap;
    wrap.horizontal = WRAP_REPEAT;
    wrap.vertical   = WRAP_REPEAT;
    wrap.minu = wrap.maxu = 0;
    wrap.minv = wrap.maxv = 0;

    RenderPacket & pkt = s_framePacket[0];
    pkt.SetupEnvironment(0, s_frame[0], s_zbuffer);
    pkt.TextureWrapping(0, wrap);
    pkt.SetupEnvironment(1, s_frame[1], s_zbuffer);
    pkt.TextureWrapping(1, wrap);
    pkt.Finish();

    pkt.SendNormal();
    dma_wait_fast();
    draw_wait_finish();

    // Build and upload the two global-palette CLUTs (they never change during
    // a run). ref_gl defaults texture intensity to 2.0. A 0.70 gamma default
    // compensates for the PS2 output path without requiring console input;
    // both are archived startup settings for future menu exposure.
    const cvar_t * gammaCvar =
        Cvar_Get("ps2_gamma", "0.70", CVAR_ARCHIVE);
    const cvar_t * intensityCvar =
        Cvar_Get("ps2_texture_intensity", "2.0", CVAR_ARCHIVE);
    float gamma = gammaCvar->value;
    float intensity = intensityCvar->value;
    if (gamma < 0.25f) { gamma = 0.25f; }
    if (gamma > 3.0f) { gamma = 3.0f; }
    if (intensity < 1.0f) { intensity = 1.0f; }
    if (intensity > 4.0f) { intensity = 4.0f; }

    // CSM1 swaps entry index bits 3 and 4 - the arrangement the GS reads.
    for (int i = 0; i < ArrayLength(s_clutData); ++i)
    {
        const int csm1 = (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
        s_clutData[csm1] = AdjustPaletteColor(global_palette[i], gamma, 1.0f);
        s_litClutData[csm1] =
            AdjustPaletteColor(global_palette[i], gamma, intensity);
    }

    // The EE just populated this buffer through its data cache. PCSX2 observes
    // those writes directly, but the real GIF DMA reads main memory and would
    // otherwise upload stale (usually zero/black) palette entries.
    SyncDCache(s_clutData, s_clutData + ArrayLength(s_clutData));
    SyncDCache(s_litClutData, s_litClutData + ArrayLength(s_litClutData));

    RenderPacket & upload = s_texUploadPacket;
    upload.Reset();
    upload.TextureTransfer(s_clutData, 16, 16, GS_PSM_32, s_clutVramAddr, 64);
    upload.TextureTransfer(s_litClutData, 16, 16, GS_PSM_32,
                           s_litClutVramAddr, 64);
    upload.TextureFlush();

    upload.SendChain();
    dma_wait_fast();

    s_drawCtx   = 1;
    s_packetIdx = 0;
}

vram::Address GlobalClutAddress()
{
    return s_clutVramAddr;
}

vram::Address LitClutAddress()
{
    return s_litClutVramAddr;
}

void BeginFrame()
{
    PS2_AssertMsg(!s_frameStarted, "BeginFrame: frame already started!");
#if PS2_PROFILE
    s_timingStats = {};
#endif
    s_frameStarted = true;

    s_packetIdx ^= 1;

    // The clear goes out immediately as its own transfer instead of riding the
    // deferred 2D packet: the VU1 3D world arrives over PATH1 mid-frame and
    // must land on an already-cleared framebuffer. The z=0 sprite with an
    // ALLPASS z-test clears color and depth in one pass (0 = farthest).
    RenderPacket & clear = s_clearPacket;
    clear.Reset();

    draw_disable_blending(); // draw_clear must overwrite, never blend
    clear.DisableTests(s_drawCtx, s_zbuffer);
    clear.Clear(s_drawCtx,
                0.0f, 0.0f,
                static_cast<float>(kWidth), static_cast<float>(s_height),
                static_cast<int>(s_clear[0]), static_cast<int>(s_clear[1]), static_cast<int>(s_clear[2]));
    clear.EnableTests(s_drawCtx, s_zbuffer); // restore the real z-test for the 3D world
    clear.Finish();

    clear.SendNormal();
    dma_wait_fast();
    draw_wait_finish();

    // The GS is idle now, so nothing queued can reference reused VRAM anymore.
    s_vramReuseHazard = false;
    vram::BeginFrame();
}

const TimingStats & GetTimingStats()
{
    return s_timingStats;
}

// Opens the pending 2D batch on demand: the first 2D primitive after a flush
// (or after BeginFrame) lands here. Cheap no-op once the batch is already open.
static void Ensure2D()
{
    PS2_AssertMsg(s_frameStarted, "2D draw outside Begin/EndFrame!");
    if (s_in2D)
    {
        return;
    }
    s_in2D       = true;
    s_currentTex = nullptr; // the TEX0 dedupe state is per 2D batch

    // The 2D overlay accumulates here and goes out at the next flush, after any
    // 3D drawn so far: always-pass z-test so it lands on top.
    RenderPacket & pkt = FramePacket();
    pkt.Reset();
    pkt.DisableTests(s_drawCtx, s_zbuffer);
}

void FlushPending2D()
{
    if (!s_in2D)
    {
        return; // nothing accumulated since the last flush
    }
    s_in2D = false;

    RenderPacket & pkt = FramePacket();
    pkt.Finish();

    dma_wait_fast();
    pkt.SendNormal();
    draw_wait_finish();

    s_vramReuseHazard = false; // GS idle again
}

bool In2DMode()
{
    return s_in2D;
}

void FillRect(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a)
{
    Ensure2D();

    RenderPacket & pkt = FramePacket();
    pkt.EnsureSpace(64);

    rect_t rect;
    rect.v0.x = static_cast<float>(x);
    rect.v0.y = static_cast<float>(y);
    rect.v0.z = 0u;
    rect.v1.x = static_cast<float>(x + w);
    rect.v1.y = static_cast<float>(y + h);
    rect.v1.z = 0u;
    rect.color.r = r;
    rect.color.g = g;
    rect.color.b = b;
    rect.color.q = 1.0f;

    if (a == 255)
    {
        // Fully opaque: plain overwrite.
        draw_disable_blending();
        rect.color.a = 0x80;
        pkt.RectFilled(s_drawCtx, rect);
    }
    else
    {
        // Translucent (fade screen and friends). GS alpha is 0..0x80 = 0..1.
        draw_enable_blending();
        rect.color.a = static_cast<u8>(a >> 1);

        // The GS is slow on very large polygons; libdraw recommends strips for
        // near-fullscreen fills.
        if (w >= kWidth / 2)
        {
            pkt.RectFilledStrips(s_drawCtx, rect);
        }
        else
        {
            pkt.RectFilled(s_drawCtx, rect);
        }
        draw_disable_blending();
    }
}

// The GS may still be drawing - or hold queued draws that will sample - VRAM
// about to be overwritten by an upload into evicted space: flush anything
// queued and wait for the GS to go idle first. Inside the 2D section the frame
// packet itself carries the FINISH; otherwise a bare FINISH rides the scratch
// packet (VU1 batches are synchronous, but their DMA completing does not mean
// the GS has finished rasterizing them).
static void SyncGsBeforeVramReuse()
{
#if PS2_PROFILE
    const timing::Stamp waitStart = timing::Now();
#endif
    if (s_in2D)
    {
        RenderPacket & pkt = FramePacket();
        pkt.Finish();

        dma_wait_fast();
        pkt.SendNormal();
        draw_wait_finish();

        pkt.Reset(); // GS registers persist; keep accumulating into the same packet
    }
    else
    {
        RenderPacket & pkt = s_texUploadPacket;
        pkt.Reset();
        pkt.Finish();

        dma_wait_fast();
        pkt.SendNormal();
        draw_wait_finish();
    }
#if PS2_PROFILE
    s_timingStats.vramStallMicros += timing::ElapsedMicros(waitStart);
#endif
    s_vramReuseHazard = false;
}

void EnsureTextureResident(const tex::Texture & texture)
{
    PS2_Assert(texture.type != tex::ImageType::Null && texture.pixels != nullptr);

    const int psm    = tex::GsPsm(texture.format);
    const int stride = TextureStridePixels(texture.storageWidth, psm);

    if (texture.vramAddr != tex::Texture::kNotResident)
    {
        if (!texture.dirtyPixels)
        {
            vram::Touch(texture); // protect from eviction until the next frame
            return;
        }

        // Dynamic texture with rewritten pixels: re-upload over its own block.
        // Draws queued earlier this frame would sample the new texels instead
        // of the ones they were issued with - drain the GS first. (Its block
        // is still owned, so the eviction/reuse hazard does not apply here.)
        const bool boundThisFrame = vram::BoundThisFrame(texture);
        vram::Touch(texture);
        if (boundThisFrame)
        {
            SyncGsBeforeVramReuse();
        }
    }
    else
    {
        const int sizeWords = TextureVramWords(texture, psm);

        bool evicted = false;
        const vram::Address addr = vram::Allocate(texture, sizeWords, &evicted);

        s_vramReuseHazard |= evicted;
        if (s_vramReuseHazard)
        {
            SyncGsBeforeVramReuse();
        }

        texture.vramAddr = addr;

        // Fill the libdraw descriptor used when binding. The stride (TEX0's TBW)
        // differs from the width for narrow 8-bit textures; the page-grid footprint
        // already covers the rounding.
        texture.texbuf.address         = static_cast<unsigned int>(addr);
        texture.texbuf.width           = static_cast<unsigned int>(stride);
        texture.texbuf.psm             = static_cast<unsigned int>(psm);
        texture.texbuf.info.width      = draw_log2(static_cast<unsigned int>(texture.storageWidth));
        texture.texbuf.info.height     = draw_log2(static_cast<unsigned int>(texture.storageHeight));
        texture.texbuf.info.components = static_cast<unsigned char>(tex::GsComponents(texture.components));
        texture.texbuf.info.function   = static_cast<unsigned char>(tex::GsFunction(texture.function));

        // MIPTBP1 addresses are in 64-word GS blocks and widths are in
        // 64-pixel TBW units. All levels share one allocator extent, packed
        // after level 0 in page-granular footprints.
        vram::Address mipAddr = vram::Address(
            static_cast<int>(addr) + vram::TextureFootprintWords(
                texture.storageWidth, texture.storageHeight, psm));
        int  mipAddresses[3] = {};
        char mipWidths[3] = {};
        for (int level = 1; level < texture.mipLevels; ++level)
        {
            const int mipWidth  = MipDimension(texture.storageWidth, level);
            const int mipHeight = MipDimension(texture.storageHeight, level);
            mipAddresses[level - 1] = static_cast<int>(mipAddr) >> 6;
            mipWidths[level - 1] = static_cast<char>(TextureStridePixels(mipWidth, psm) >> 6);
            mipAddr = vram::Address(static_cast<int>(mipAddr) +
                                    vram::TextureFootprintWords(mipWidth, mipHeight, psm));
        }
        texture.mipmap.address1 = mipAddresses[0];
        texture.mipmap.width1   = mipWidths[0];
        texture.mipmap.address2 = mipAddresses[1];
        texture.mipmap.width2   = mipWidths[1];
        texture.mipmap.address3 = mipAddresses[2];
        texture.mipmap.width3   = mipWidths[2];

        Com_DPrintf("VRAM: uploaded '%s' (%dx%d -> %dx%d, %d KB)\n", texture.name,
                    texture.width, texture.height, texture.storageWidth,
                    texture.storageHeight, sizeWords * 4 / 1024);
    }

    if (texture.dirtyPixels)
    {
        // The CPU just wrote these pixels; part of them may still sit in the
        // data cache, and SendChain only writes back the chain-tag buffer, not
        // REF'd data - flush the range or the GS reads stale texels. Built-ins
        // are never dirty (the ELF loader wrote them) and skip this.
        void * pixels = const_cast<void *>(texture.pixels);
        SyncDCache(pixels, static_cast<u8 *>(pixels) + texture.pixelBytes);
        texture.dirtyPixels = false;
    }

    // Synchronous GS upload; the chain references the pixels in EE RAM.
    // TODO: TextureTransfer has no EnsureSpace - revisit the 128-qword scratch
    // packet if large streamed assets ever exceed its chain-tag headroom.
    RenderPacket & pkt = s_texUploadPacket;
    pkt.Reset();
    const u8 * mipPixels = static_cast<const u8 *>(texture.pixels);
    vram::Address mipAddr = texture.vramAddr;
    for (int level = 0; level < texture.mipLevels; ++level)
    {
        const int mipWidth  = MipDimension(texture.storageWidth, level);
        const int mipHeight = MipDimension(texture.storageHeight, level);
        const int mipStride = TextureStridePixels(mipWidth, psm);

        pkt.TextureTransfer(mipPixels, mipWidth, mipHeight, psm, mipAddr, mipStride);

        mipPixels += mipWidth * mipHeight * tex::BytesPerTexel(texture.format);
        mipAddr = vram::Address(static_cast<int>(mipAddr) +
                                vram::TextureFootprintWords(mipWidth, mipHeight, psm));
    }
    pkt.TextureFlush();

    pkt.SendChain();
#if PS2_PROFILE
    const timing::Stamp uploadStart = timing::Now();
#endif
    dma_wait_fast();

    // Waiting for GIF DMA alone is insufficient here. A following VU1 XGKICK
    // arrives through higher-priority PATH1 and may start sampling while the
    // GS is still executing this PATH3 host-to-local transfer. Under VRAM
    // churn that exposed old page contents as vertical strips (or black blocks
    // with mipmapping disabled).
    //
    // draw_texture_transfer builds a complete DMA chain, so a FINISH packet
    // appended to that packet after TextureFlush is unreachable. Send FINISH
    // separately, after the upload chain's DMA has been consumed; PATH3 order
    // then guarantees the GS completes every preceding host-to-local transfer
    // before EnsureTextureResident returns to the VU1 draw path.
    pkt.Reset();
    pkt.Finish();
    pkt.SendNormal();
    dma_wait_fast();
    draw_wait_finish();
#if PS2_PROFILE
    s_timingStats.textureUploadMicros += timing::ElapsedMicros(uploadStart);
#endif

    vram::NoteTextureUpload(); // for the debug overlay's per-frame upload count
}

void ReleaseTexture(const tex::Texture & texture)
{
    // Never leave the TEX0 dedupe pointing at a released texture: a rebind in
    // the same 2D section must go through EnsureTextureResident again, and the
    // cache may recycle the slot for a different image entirely.
    if (s_currentTex == &texture)
    {
        s_currentTex = nullptr;
    }

    if (texture.vramAddr == tex::Texture::kNotResident)
    {
        return;
    }

    vram::Free(texture);

    // Queued or in-flight draws may still sample the freed range; the next
    // upload that lands there must sync the GS first, same as an eviction.
    s_vramReuseHazard = true;
}

void SetTextureFor2D(const tex::Texture & texture)
{
    PS2_Assert(texture.type != tex::ImageType::Null && texture.pixels != nullptr);
    Ensure2D();

    if (&texture == s_currentTex && !texture.dirtyPixels)
    {
        return; // already bound (and made resident); EE RAM pixels not dirty.
    }

    EnsureTextureResident(texture);
    s_currentTex = &texture;

    RenderPacket & pkt = FramePacket();
    pkt.EnsureSpace(16);

    lod_t lod;
    const bool mipped = texture.mipLevels > 1;
    lod.calculation   = mipped ? LOD_FORMULAIC : LOD_USE_K;
    lod.max_level     = static_cast<unsigned char>(texture.mipLevels - 1);
    lod.mag_filter    = static_cast<unsigned char>(tex::GsMagFilter(texture.magFilter));
    lod.min_filter    = static_cast<unsigned char>(
        mipped ? LOD_MIN_LINE_MIPMAP_NEAR : tex::GsMinFilter(texture.minFilter));
    lod.mipmap_select = LOD_MIPMAP_REGISTER;
    lod.l             = 0;
    lod.k             = mipped ? kMipmapLodBias : 0.0f;

    clutbuffer_t clut;
    if (texture.format == tex::PixelFormat::Palette8)
    {
        // Reload the on-chip CLUT cache from the global palette on every bind:
        // cheap (1 KB) at the 2D path's bind rate. TODO: CLUT_COMPARE_CBP0
        // skips redundant reloads - worthwhile once world textures bind per-surface.
        clut.address      = static_cast<unsigned int>(s_clutVramAddr);
        clut.psm          = GS_PSM_32;
        clut.storage_mode = CLUT_STORAGE_MODE1;
        clut.start        = 0;
        clut.load_method  = CLUT_LOAD;
    }
    else
    {
        // Not palettized; the CLUT slots stay empty.
        clut.address      = 0;
        clut.psm          = 0;
        clut.storage_mode = CLUT_STORAGE_MODE1;
        clut.start        = 0;
        clut.load_method  = CLUT_NO_LOAD;
    }

    texbuffer_t texbuf = texture.texbuf; // libdraw wants a mutable pointer

    pkt.TextureSampling(s_drawCtx, lod);
    pkt.TextureBuffer(s_drawCtx, texbuf, clut);
    if (mipped)
    {
        mipmap_t mipmap = texture.mipmap;
        pkt.TextureMipmap1(s_drawCtx, mipmap);
    }
}

void DrawTexturedRect(int x, int y, int w, int h,
                      int u0, int v0, int u1, int v1, u8 brightness)
{
    PS2_AssertMsg(s_currentTex != nullptr, "DrawTexturedRect without SetTextureFor2D!");
    PS2_AssertMsg(s_in2D, "DrawTexturedRect without an open 2D batch!");

    RenderPacket & pkt = FramePacket();
    pkt.EnsureSpace(8);

    texrect_t rect;
    rect.v0.x = static_cast<float>(x);
    rect.v0.y = static_cast<float>(y);
    rect.v0.z = 0u;
    rect.t0.u = static_cast<float>(u0);
    rect.t0.v = static_cast<float>(v0);
    rect.v1.x = static_cast<float>(x + w);
    rect.v1.y = static_cast<float>(y + h);
    rect.v1.z = 0u;
    rect.t1.u = static_cast<float>(u1);
    rect.t1.v = static_cast<float>(v1);

    // Modulate: 0x80 = 1.0, so 'brightness' 128 leaves texels unchanged. Vertex
    // alpha 0x80 likewise preserves texel alpha, which the alpha test then uses
    // to cut out transparent texels (e.g. the console font background).
    rect.color.r = brightness;
    rect.color.g = brightness;
    rect.color.b = brightness;
    rect.color.a = 0x80;
    rect.color.q = 1.0f;

    draw_disable_blending();
    pkt.RectTextured(s_drawCtx, rect);
}

void EndFrame()
{
    PS2_AssertMsg(s_frameStarted, "EndFrame without BeginFrame!");
    s_frameStarted = false;

    // Send whatever 2D accumulated since the last flush (the HUD/console overlay
    // in the common case) so it lands on top before the buffer is displayed.
    FlushPending2D();

    graph_wait_vsync();
    graph_set_framebuffer_filtered(static_cast<int>(s_frame[s_drawCtx].address),
                                   static_cast<int>(s_frame[s_drawCtx].width),
                                   static_cast<int>(s_frame[s_drawCtx].psm), 0, 0);

    s_drawCtx ^= 1; // draw into the other buffer next frame

    vram::EndFrame();
}

} // namespace ps2::gs
