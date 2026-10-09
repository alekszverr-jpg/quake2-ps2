#pragma once
/* Base-game weapon sounds, including client-selected random muzzle sounds.
 * Register once per map before S_EndRegistration. No expansion-only assets.
 * Existing caches are reused; level purge still frees PCM before the next map.
 */
static const char * const ps2_weapon_sounds[] = {
    "weapons/blastf1a.wav",
    "weapons/hyprbu1a.wav",
    "weapons/hyprbl1a.wav",
    "weapons/hyprbf1a.wav",
    "weapons/hyprbd1a.wav",
    "weapons/shotgf1b.wav",
    "weapons/shotgr1b.wav",
    "weapons/sshotf1b.wav",
    "weapons/machgf1b.wav",
    "weapons/machgf2b.wav",
    "weapons/machgf3b.wav",
    "weapons/machgf4b.wav",
    "weapons/machgf5b.wav",
    "weapons/chngnu1a.wav",
    "weapons/chngnl1a.wav",
    "weapons/chngnd1a.wav",
    "weapons/grenlf1a.wav",
    "weapons/grenlr1b.wav",
    "weapons/grenlb1b.wav",
    "weapons/hgrent1a.wav",
    "weapons/hgrena1b.wav",
    "weapons/hgrenc1b.wav",
    "weapons/hgrenb1a.wav",
    "weapons/hgrenb2a.wav",
    "weapons/rockfly.wav",
    "weapons/rocklf1a.wav",
    "weapons/rocklr1b.wav",
    "weapons/railgf1a.wav",
    "weapons/rg_hum.wav",
    "weapons/bfg__f1y.wav",
    "weapons/bfg__l1a.wav",
    "weapons/bfg__x1b.wav",
    "weapons/bfg_hum.wav",
    "misc/lasfly.wav"
};
