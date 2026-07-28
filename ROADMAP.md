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
- [~] Real PS2 boot from a FAT32 USB drive through uLaunchELF
- [x] Menus, HUD, console and gamepad controls
- [x] Textured BSP world geometry
- [x] Animated MD2 enemies, corpses and first-person weapons
- [x] NTSC/PAL framebuffer selection
- [~] Texture streaming within the PS2's 4 MB GS VRAM
- [ ] Lighting
- [ ] Sound

## Milestone 0 - Stable hardware test build

Target: the first level can be played repeatedly on PCSX2 and a real PS2
without crashes or severe rendering corruption.

- [~] Validate cold boot and repeated boot through uLaunchELF
- [x] Validate full-height PAL output and CRT safe area
- [x] Validate WAL mipmaps and minification filtering on real hardware
- [ ] Make diagnostic overlays optional and disabled by default
- [ ] Run a complete first-level playthrough on real hardware
- [ ] Record a small compatibility matrix: console region, video mode,
  USB device and launch method

## Milestone 1 - Renderer completeness

Target: levels visually match the original software/OpenGL renderer closely
enough for normal gameplay.

- [ ] BSP lightmaps and static lighting
- [ ] MD2 vertex lighting using Quake II normal tables
- [ ] Dynamic lights and light styles
- [ ] Sprite entities and particles
- [ ] Brush entities such as moving doors and platforms
- [ ] Sky surfaces and skybox
- [ ] Transparent surfaces and entity alpha
- [ ] Turbulent water/lava/slime surfaces
- [ ] Weapon depth-range and view-model render flags
- [x] WAL mipmaps and stable minification filtering
- [ ] Frustum/entity culling and renderer performance pass

## Milestone 2 - Audio

Target: gameplay sound effects and ambient audio work on real hardware.

- [ ] Select and initialise a PS2 audio backend
- [ ] Upload/cache Quake II sound samples in IOP/SPU2 memory
- [ ] Implement channel mixing, attenuation and looping sounds
- [ ] Add music playback or document the selected replacement strategy
- [ ] Validate audio during level transitions and sustained gameplay

## Milestone 3 - Gameplay and persistence

Target: the single-player campaign is functionally completable.

- [ ] Validate every base-game map and level transition
- [ ] Save/load support on a writable PS2 storage target
- [ ] Configuration persistence and controller settings
- [ ] Cinematic and intermission validation
- [ ] Complete rendering for gameplay effects and projectiles
- [ ] Error handling for missing/corrupt game data

## Milestone 4 - Performance and memory

Target: stable frame pacing with no memory exhaustion during normal campaign
play on a retail console.

- [ ] Establish frame-time budgets for EE, VU1, GS and USB loading
- [ ] Reduce texture upload churn and VRAM fragmentation
- [ ] Add release builds with assertions/diagnostics disabled
- [ ] Profile model interpolation, BSP traversal and clipping
- [ ] Validate long sessions for EE RAM, IOP RAM and VRAM leaks
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

1. Implement BSP lightmaps.
2. Start the PS2 audio backend.
3. Run a complete first-level real-hardware stability test.
4. Make diagnostic overlays optional and disabled by default.

Update this file whenever priorities, milestone status or completion criteria
change. Record completed user-visible work in `CHANGELOG` in the same commit.
