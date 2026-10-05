// Host tests compile the actual allocator, replacing only SDK-facing headers.
#include "ps2/renderer/vram.cpp"
#include <cstdio>
using namespace ps2;
constexpr int page = 2048;

void Upload(tex::Texture & texture, int pages)
{
    bool evicted = false;
    texture.vramAddr = vram::Allocate(texture, pages * page, &evicted);
    vram::NoteTextureUpload(texture);
}

int main()
{
    assert(!vram::EventLoggingEnabled());
    Cvar_Get("ps2_vram_log","0",0)->value=1;
    assert(vram::EventLoggingEnabled());
    Cvar_Get("ps2_vram_log","0",0)->value=0;
    vram::Init(1024 * 1024 - 32 * page);
    vram::BeginFrame();
    // A small HUD image absent from the world plan survives a world miss,
    // even when the alternative is a future world texture.
    tex::Texture hud, world, miss;
    hud.type = tex::ImageType::Pic;
    Upload(hud, 2);
    Upload(world, 30);
    vram::BeginFrame();
    const tex::Texture * plan[] = { &miss, &world };
    vram::BeginPlannedTextureUses(plan, 2);
    Upload(miss, 2);
    assert(hud.vramAddr != tex::Texture::kNotResident);
    assert(world.vramAddr == tex::Texture::kNotResident);
    vram::EndPlannedTextureUses();
    vram::Free(hud); vram::Free(world); vram::Free(miss);

    // Successful prefetch uses the same retention policy for older residents.
    Upload(hud, 2); Upload(world, 30);
    vram::BeginFrame();
    bool prefetchEvicted = false;
    miss.vramAddr = vram::TryAllocateForPrefetch(miss, 2 * page, &prefetchEvicted);
    assert(prefetchEvicted && miss.vramAddr != tex::Texture::kNotResident);
    assert(hud.vramAddr != tex::Texture::kNotResident);
    assert(world.vramAddr == tex::Texture::kNotResident);
    vram::Free(hud); vram::Free(world); vram::Free(miss);

    // Large menu Pics are not retained merely because they have UI type.
    Upload(hud, 4); Upload(world, 28);
    vram::BeginFrame();
    Upload(miss, 4);
    assert(hud.vramAddr == tex::Texture::kNotResident);
    assert(world.vramAddr != tex::Texture::kNotResident);
    vram::Free(hud); vram::Free(world); vram::Free(miss);

    // A no-longer-used HUD image loses retention after one intervening frame.
    Upload(hud, 2); Upload(world, 30);
    vram::BeginFrame(); vram::BeginFrame();
    Upload(miss, 2);
    assert(hud.vramAddr == tex::Texture::kNotResident);
    assert(world.vramAddr != tex::Texture::kNotResident);
    vram::Free(hud); vram::Free(world); vram::Free(miss);

    // The budget protects only 8 of this heap's 32 pages. Later small Pics
    // stay evictable instead of converting all UI allocations into pins.
    tex::Texture pics[16];
    for (auto & pic : pics) { pic.type = tex::ImageType::Pic; Upload(pic, 2); }
    vram::BeginFrame();
    Upload(miss, 2);
    for (int i = 0; i < 4; ++i) assert(pics[i].vramAddr != tex::Texture::kNotResident);
    assert(pics[4].vramAddr == tex::Texture::kNotResident);
    for (auto & pic : pics) vram::Free(pic);
    vram::Free(miss);

    // Prefetch failure leaves pinned residency and counters unchanged.
    Upload(hud, 2); Upload(world, 30);
    const auto before = vram::GetStats();
    bool evicted = true;
    assert(vram::TryAllocateForPrefetch(miss, 2 * page, &evicted) == vram::Address::Invalid);
    assert(!evicted);
    assert(vram::GetStats().evictionsThisFrame == before.evictionsThisFrame);
    assert(hud.vramAddr != tex::Texture::kNotResident);
    assert(world.vramAddr != tex::Texture::kNotResident);
    // Retention is soft: a full-heap demand must still be satisfiable.
    Upload(miss, 32);
    assert(hud.vramAddr == tex::Texture::kNotResident);
    assert(world.vramAddr == tex::Texture::kNotResident);
    vram::Free(miss);
    assert(vram::GetStats().freeWords == 32 * page);
#if PS2_PROFILE
    // Active sky survives a later demand miss without becoming a hard pin.
    // Small Pics retain their stronger existing preference.
    tex::Texture sky, sky2;
    sky.type=sky2.type=tex::ImageType::Sky;
    Upload(sky,8); Upload(world,24);
    vram::BeginFrame(); vram::Touch(sky);
    vram::BeginPlannedTextureUses(plan,2);
    Upload(miss,2);
    assert(sky.vramAddr!=tex::Texture::kNotResident);
    assert(world.vramAddr==tex::Texture::kNotResident);
    vram::EndPlannedTextureUses();
    Upload(world,32); // safety fallback even when sky is retained
    assert(sky.vramAddr==tex::Texture::kNotResident);
    vram::Free(world); vram::Free(miss);
    // Stale/unseen sky is ordinary LRU on the next frame.
    Upload(sky,8); Upload(world,24); vram::BeginFrame(); Upload(miss,2);
    assert(sky.vramAddr==tex::Texture::kNotResident);
    assert(world.vramAddr!=tex::Texture::kNotResident);
    vram::Free(sky); vram::Free(world); vram::Free(miss);
    // One-third budget protects only one64KiB sky face in this256KiB heap.
    Upload(sky,8); Upload(sky2,8); Upload(world,16);
    Upload(miss,2);
    assert(sky.vramAddr!=tex::Texture::kNotResident);
    assert(sky2.vramAddr==tex::Texture::kNotResident);
    vram::Free(sky); vram::Free(sky2); vram::Free(world); vram::Free(miss);
    // UI wins over retained sky when there is no ordinary victim.
    Upload(hud,2); Upload(sky,8); Upload(miss,22);
    vram::Free(miss); Upload(world,22); vram::Free(world);
    Upload(miss,24); // free22pages cannot fit, sky must yield before HUD
    assert(hud.vramAddr!=tex::Texture::kNotResident);
    assert(sky.vramAddr==tex::Texture::kNotResident);
    vram::Free(hud); vram::Free(sky); vram::Free(miss);
    assert(vram::GetStats().freeWords==32*page);
    // Count first/dirty uploads separately from eviction reloads, attribute
    // them to their type and active phase, and reset everything next frame.
    vram::BeginFrame();
    tex::Texture sample;
    sample.type = tex::ImageType::Skin;
    sample.pixelBytes = 12345;
    vram::SetUploadPhase(vram::UploadPhase::Entities);
    vram::NoteTextureUpload(sample);
    vram::NoteTextureUpload(sample); // A dirty update has no eviction mark.
    sample.evictedSinceUpload = true;
    vram::NoteTextureUpload(sample);
    auto counters = vram::GetStats();
    const auto & skin = counters.uploadsByType[static_cast<int>(tex::ImageType::Skin)];
    assert(skin.images == 3 && skin.reloads == 1 && skin.bytes == 37035);
    assert(counters.uploadsByPhase[static_cast<int>(vram::UploadPhase::Entities)] == 3);
    assert(counters.uploadsThisFrame == 3 && counters.reloadsThisFrame == 1);
    vram::BeginFrame();
    counters = vram::GetStats();
    for (const auto & type : counters.uploadsByType)
        assert(type.images == 0 && type.reloads == 0 && type.bytes == 0);
    for (int phase : counters.uploadsByPhase) assert(phase == 0);
    vram::NoteTextureUpload(sample);
    assert(vram::GetStats().uploadsByPhase[0] == 1);
#endif
    std::puts("VRAM retention, safety, fallback and upload accounting: PASS");
}
