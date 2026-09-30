#include <cassert>
#include <cstdio>

static bool s_registrationActive;
static int phase;
class ModelCache {
public:
    int m_regSequence = 1;
    int worldSequence = 1;
    int aliasSequence = 1;
    int spriteSequence = 1;
    bool world = true, alias = true, sprite = true, inlineModel = true;
    void PurgeLevelModels();
    void PurgeWorldModel() { world = false; inlineModel = false; }
    void EndRegistration() {
        if (worldSequence != m_regSequence) world = false;
        if (aliasSequence != m_regSequence) alias = false;
        if (spriteSequence != m_regSequence) sprite = false;
    }
};
static ModelCache cache;
namespace ps2 {
namespace view {
void BeginRegistration() {
    assert(s_registrationActive && phase == 0);
    assert(cache.world); // Release world-backed lighting before its owner.
    phase = 1;
}
}
namespace mod {
void PurgeLevelModels() {
    assert(phase == 1);
    cache.PurgeLevelModels();
    assert(!cache.world && !cache.inlineModel && !cache.alias && !cache.sprite);
    phase = 2;
}
}
namespace tex {
void BeginRegistration() {
    assert(s_registrationActive && phase == 2);
    phase = 3;
}
}
}
#include "purge.inc"
int main() {
    PS2_PurgeLevelRendererMemory();
    assert(phase == 3 && s_registrationActive);
    const int sequence = cache.m_regSequence;
    cache.PurgeLevelModels(); // Client registration may repeat the purge.
    assert(cache.m_regSequence == sequence + 1);
    assert(!cache.world && !cache.alias && !cache.sprite && !cache.inlineModel);
    std::puts("Pre-BSP level purge lifecycle passed");
}
