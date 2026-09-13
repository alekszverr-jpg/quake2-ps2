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
    std::puts("VRAM retention, expiry, budget, prefetch safety and fallback: PASS");
}
