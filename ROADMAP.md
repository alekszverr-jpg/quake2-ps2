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
- [~] Validate the corrected portable channel mixer, attenuation and looping through the PS2 backend
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

## Performance and optimization program

This section is the implementation plan for Milestone 4. Changes should be
profiled on PCSX2 first and periodically validated on a retail PS2. Visual
correctness, deterministic level loading and campaign stability take priority
over synthetic peak frame rate.

### P0 - Measurement and release baseline

- [ ] Add repeatable benchmark scenes for a light BSP room, a heavy combat
  scene, water/particles and a high-entity outdoor scene
- [ ] Record CPU time, VU wait time, texture DMA/upload time, visible surfaces,
  triangles, batches and minimum FPS for each benchmark
- [ ] Add a release configuration without permanent draw, memory, texture and
  audio diagnostics; keep a gamepad-selectable profiling build
- [ ] Establish regression limits for frame time, EE RAM, IOP RAM and GS VRAM

Completion criterion: the same camera positions and encounters produce a
comparable PCSX2/PS2 profile, and release builds do not pay for disabled
diagnostic formatting or rendering.

### P1 - VU1 geometry and lighting

- [ ] Move BSP vertex transformation and lighting from the EE to VU1
- [ ] Move MD2 interpolation, transformation and vertex lighting to VU1
- [ ] Double-buffer VU1 input/output so EE scene preparation overlaps VU1 work
- [ ] Keep a guarded EE fallback until BSP and every MD2 render flag match

Completion criterion: BSP and MD2 output remains visually equivalent while EE
geometry time and VU1 idle time are measurably reduced in the benchmark scenes.

### P2 - DMA/VIF batching and draw submission

- [ ] Build larger DMA/VIF batches instead of submitting small surface/model
  packets independently
- [ ] Sort opaque draws by texture, lightmap and render state to reduce GS state
  changes and texture uploads
- [ ] Submit transparent surfaces in a separate order-correct pass
- [ ] Track batch count, average vertices per batch and state changes in the
  profiling build

Completion criterion: batch/state-change counts drop without reintroducing
weapon, particle, glass, water or world-geometry corruption.

### P3 - BSP visibility and culling

- [ ] Precompute reusable visible BSP surface sets from Quake II PVS data
- [ ] Cache per-view-cluster visibility work and invalidate it only when the
  camera crosses the relevant BSP boundary
- [ ] Add more aggressive frustum, backface, entity and bounding-box culling
- [ ] Avoid lighting, transforming or batching surfaces rejected by visibility

Completion criterion: heavy scenes submit substantially fewer surfaces and
vertices, with no visible popping, missing doors or broken moving brush models.

### P4 - Texture residency and VRAM traffic

- [ ] Measure texture upload churn, evictions, VRAM waits and repeated uploads
  per frame
- [ ] Keep frequently used HUD, weapon, particle and common world textures in
  stable VRAM slots where practical
- [ ] Reuse texture allocations across frames and reduce allocator
  fragmentation during level transitions
- [ ] Group world draws by resident texture/lightmap and upload only when the
  required generation is not already present

Completion criterion: common gameplay frames perform few or no redundant
uploads, zero-free-VRAM stress remains artifact-free, and level transitions do
not accumulate stale texture allocations.

### P5 - Specialized effect paths

- [ ] Add a compact particle path with shared state and batched billboard data
- [ ] Add a simplified turbulent water path with a bounded subdivision cost
- [ ] Consolidate glass, sprites and other transparent surfaces into a minimal
  state-change path
- [ ] Add quality limits for particles and translucent layers that degrade
  gracefully in unusually heavy scenes

Completion criterion: water, particles and transparency remain visually stable
while their combined EE time, batches and GS state changes are reduced.

### P6 - Memory and sustained-play validation

- [ ] Reuse per-level world/model work buffers instead of retaining temporary
  allocations between maps
- [ ] Validate Base 1 -> Base 2 -> Base 3 and other multi-level routes without
  EE allocation failures
- [ ] Run extended combat/audio sessions while tracking EE, IOP and VRAM high
  water marks
- [ ] Define a retail-PS2 memory reserve that must remain available before a
  new map is accepted

Completion criterion: repeated map transitions and long sessions complete
without memory exhaustion, and the retail release maintains its reserved
memory margin.

### Recommended implementation order

1. P0 profiling/release baseline
2. P2 batching and P3 culling for lower-risk CPU wins
3. P4 stable texture residency and reduced VRAM churn
4. P1 BSP VU1 path, followed by the MD2 VU1 path
5. P5 specialized particle, water and transparency paths
6. P6 full campaign, transition and sustained-play validation

## Milestone 5 - Test release

Target: a documented build that another user can install and test without
development tools.

- [ ] Package the ELF without copyrighted Quake II data
- [ ] Document PCSX2 and real-console installation
- [ ] Provide default controller bindings and troubleshooting
- [ ] Publish known issues and a hardware compatibility list
- [ ] Tag and archive the first public test release

## Immediate priorities

1. Validate alpha.44 safe recovery from archived HIGH settings and quantify
   any remaining 11025 Hz underrun chatter without blocking the game thread.
2. Repeat the audio smoke test on a real PS2 and check attenuation, stereo
   positioning, looping, distortion, minimum queue depth and FPS regression.
3. Use the queue low-water mark to decide whether hot effects
   should remain in the EE software mixer or move to cached SPU2 ADPCM voices.
4. Continue optimizing the remaining MD2/entity EE path after renderer and
   audio completeness work is stable.
5. Recheck multi-level transitions after the next optimization pass, including
   a longer Base 1 -> Base 2 -> Base 3 session on PCSX2 and real hardware.
6. Revisit VU1 overlap and texture churn if later profiles show their
   currently small wait times growing.

Update this file whenever priorities, milestone status or completion criteria
change. Record completed user-visible work in `CHANGELOG` in the same commit.
