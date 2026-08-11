# Quake II PS2 Roadmap

The goal is a stable, fully playable single-player Quake II port for a real
PlayStation 2, built entirely with open-source PS2DEV tooling.

Status markers:

- `[x]` complete and tested
- `[~]` implemented but still being validated
- `[ ]` not implemented

## Current baseline

- [x] Reproducible `quake2.elf` build in GitHub Actions
- [x] PCSX2 boot from `host:` with `baseq2` beside the ELF
- [x] Real PS2 boot from a FAT32 USB drive through uLaunchELF
- [x] Menus, HUD, console and gamepad controls
- [x] Gamepad-accessible campaign map selector for renderer testing
- [x] Gamepad-accessible live mipmapping/format diagnostics for BSP artifact isolation
- [x] Textured BSP world geometry
- [x] Animated MD2 enemies, corpses and first-person weapons
- [x] NTSC/PAL framebuffer selection
- [~] Texture streaming within the PS2's 4 MB GS VRAM
- [~] Adaptive BSP vertex/lightmap lighting
- [x] MD2 vertex lighting
- [~] Sound effects through the stock mixer and PS2 `audsrv`

## Milestone 0 - Stable hardware test build

Target: the first level can be played repeatedly on PCSX2 and a real PS2
without crashes or severe rendering corruption.

- [~] Validate cold boot and repeated boot through uLaunchELF
- [x] Validate full-height PAL output and CRT safe area
- [x] Validate WAL mipmaps and minification filtering on real hardware
- [x] Make diagnostic overlays optional and disabled by default
- [ ] Run a complete first-level playthrough on real hardware
- [ ] Record a small compatibility matrix: console region, video mode,
  USB device and launch method

## Milestone 1 - Renderer completeness

Target: levels visually match the original software/OpenGL renderer closely
enough for normal gameplay.

- [~] BSP lightmaps and adaptive static lighting
- [x] MD2 vertex lighting using Quake II normal tables
- [x] Animated light styles
- [x] Dynamic entity lights
- [~] Sprite entities
- [x] Particles
- [x] Opaque brush entities such as moving doors and platforms
- [~] Sky surfaces and skybox
- [x] Transparent BSP surfaces such as Base 3 glass, validated on PCSX2 and PS2
- [~] Translucent entity alpha
- [x] Turbulent water/lava/slime surfaces, validated in PCSX2
- [x] Weapon depth-range and view-model render flags
- [x] WAL mipmaps and stable minification filtering
- [x] Power-of-two GS storage for NPOT WAL textures, validated on `bunk1`
- [ ] Frustum/entity culling and renderer performance pass

## Milestone 2 - Audio

Target: gameplay sound effects and ambient audio work on real hardware.

- [~] Select and initialise a PS2 audio backend (`audsrv` PCM stream)
- [ ] Upload/cache Quake II sound samples in IOP/SPU2 memory
- [~] Validate stock channel mixing, attenuation and looping through the PS2 backend
- [ ] Add music playback or document the selected replacement strategy
- [ ] Validate audio during level transitions and sustained gameplay

## Milestone 3 - Gameplay and persistence

Target: the single-player campaign is functionally completable.

- [ ] Validate every base-game map and level transition
- [x] Validate enemy visual acquisition and close-range attack behaviour
- [ ] Save/load support on a writable PS2 storage target
- [ ] Configuration persistence and controller settings
- [ ] Cinematic and intermission validation
- [ ] Complete rendering for gameplay effects and projectiles
- [ ] Error handling for missing/corrupt game data

## Milestone 4 - Performance and memory

Target: stable frame pacing with no memory exhaustion during normal campaign
play on a retail console.

- [x] Establish initial frame-time budgets for EE, VU1, GS and texture loading
- [ ] Reduce texture upload churn and VRAM fragmentation
- [~] Validate GS completion barriers for streamed textures under zero-free-VRAM churn
- [ ] Add release builds with assertions/diagnostics disabled
- [x] Profile and optimize first-pass model interpolation and BSP lighting
- [~] Validate long sessions for EE RAM, IOP RAM and VRAM leaks
- [ ] Target a stable 30 FPS minimum, with 60 FPS where practical

## Milestone 5 - Test release

Target: a documented build that another user can install and test without
development tools.

- [ ] Package the ELF without copyrighted Quake II data
- [ ] Document PCSX2 and real-console installation
- [ ] Provide default controller bindings and troubleshooting
- [ ] Publish known issues and a hardware compatibility list
- [ ] Tag and archive the first public test release

## Immediate priorities

1. Use the alpha.39 gamepad audio status and generated tone to isolate the
   silent alpha.38 output between IOP startup, `audsrv` format and PCM queueing.
2. Repeat the audio smoke test on a real PS2 and watch for IOP-module startup
   failures, underruns or a measurable frame-rate regression.
3. Add audio timing/underrun diagnostics, then decide whether hot effects
   should remain in the EE software mixer or move to cached SPU2 ADPCM voices.
4. Continue optimizing the remaining MD2/entity EE path after renderer and
   audio completeness work is stable.
5. Recheck multi-level transitions after the next optimization pass, including
   a longer Base 1 -> Base 2 -> Base 3 session on PCSX2 and real hardware.
6. Revisit VU1 overlap and texture churn if later profiles show their
   currently small wait times growing.

Update this file whenever priorities, milestone status or completion criteria
change. Record completed user-visible work in `CHANGELOG` in the same commit.
