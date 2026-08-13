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
- [~] Add release builds with permanent profiling/diagnostics disabled
- [x] Profile and optimize first-pass model interpolation and BSP lighting
- [~] Validate long sessions for EE RAM, IOP RAM and VRAM leaks
- [ ] Target a stable 30 FPS minimum, with 60 FPS where practical

## Performance and optimization program

This section is the implementation plan for Milestone 4. Changes should be
profiled on PCSX2 first and periodically validated on a retail PS2. Visual
correctness, deterministic level loading and campaign stability take priority
over synthetic peak frame rate.

The ordering below also incorporates the practical recommendations from Sony's
`PS2 Programming Optimisations` material: keep EE, VIF/VU1 and GS working in
parallel; align large DMA reference data to an 8-QW (128-byte) boundary; reduce
DMA-tag and state-change overhead; batch texture transfers; and optimize GS
texture-page behaviour before adding more complex MFIFO scheduling.

### P0 - Measurement and release baseline

- [ ] Add repeatable benchmark scenes for a light BSP room, a heavy combat
  scene, water/particles and a high-entity outdoor scene
- [ ] Record CPU time, VU wait time, texture DMA/upload time, visible surfaces,
  triangles, batches and minimum FPS for each benchmark
- [x] Add a release configuration without permanent draw, memory, texture and
  renderer timing diagnostics; keep a gamepad-selectable profiling build
- [ ] Move the remaining audio status polling behind the profile build once the
  streaming backend no longer needs it for stability testing
- [ ] Establish regression limits for frame time, EE RAM, IOP RAM and GS VRAM

Completion criterion: the same camera positions and encounters produce a
comparable PCSX2/PS2 profile, and release builds do not pay for disabled
diagnostic formatting or rendering.

### P1 - DMA/VIF submission pipeline

- [~] Align large dynamic vertex arrays, VIF reference data and persistent DMA
  buffers to 8 QW / 128 bytes; retain 16-byte alignment for small inline data
- [~] Measure the current full synchronization points: per-draw VIF `FLUSH`,
  VIF1 DMA waits, texture-upload `FINISH` waits and framebuffer-clear waits
- [ ] Replace per-`DrawTriangles` `FLUSH + Wait` submission with larger
  pass-level VIF chains and wait only before a buffer or referenced range is
  actually reused
- [ ] Extend the existing VU1 double-buffered chunks to two or three EE-side
  packet buffers so EE preparation overlaps VU1 work and GS rasterization
- [ ] Reduce DMA tag count and report chain count, QW transferred, average
  vertices per chain and wait time in the profiling build

Completion criterion: ordinary opaque passes no longer block after every draw,
DMA reference buffers meet the documented alignment, and measured VIF/VU/GS
wait time falls without rendering corruption on PCSX2 or real hardware.

### P2 - Draw sorting and texture transfer scheduling

- [x] Chain opaque BSP surfaces by texture before drawing
- [ ] Sort all opaque world/entity draws by texture, lightmap and render state
  to reduce GS register changes and uploads
- [ ] Submit transparent surfaces in a separate order-correct pass
- [ ] Build a visible-frame texture working set before geometry submission;
  upload missing textures in batches before any dependent VU1 `XGKICK`
- [ ] Keep texture transfers and PATH1 geometry correctly ordered without a GS
  `FINISH` after every individual texture
- [ ] Track uploads, evictions, texture switches, GS waits and state changes in
  the profiling build

Completion criterion: texture and state-change counts drop, texture transfer
waits are amortized across a pass, and the former black-strip/stale-page bugs do
not return under zero-free-VRAM stress.

### P3 - BSP visibility and culling

- [x] Use Quake II PVS data to mark visible BSP leaves and parent nodes
- [x] Cache per-view-cluster visibility work and invalidate it only when the
  camera crosses the relevant BSP boundary
- [ ] Add more aggressive frustum, backface, entity and bounding-box culling
- [ ] Avoid lighting, transforming or batching surfaces rejected by visibility
- [~] Cache adaptive BSP tessellation independently from animated lightstyle
  colours, updating cached vertex colours without rebuilding the topology
- [~] Compact retained BSP-lighting vertices so heavy scenes fit more reusable
  topology inside the fixed cache budget
- [~] Profile a moderately coarser adaptive light grid before committing any
  quality default to release builds
- [~] Retain the coarse PROFILE light grid on smooth BSP surfaces while locally
  refining high-contrast lamp gradients; validate `LitFine`, `LitKB` and
  `LitBuild` together so the quality fix stays inside the persistent cache
- [~] Pack retained lightmap UVs to UNORM16 and carry locally detected lamp
  refinement through one child level, improving both cache headroom and the
  remaining high-contrast interpolation boundary
- [~] Reconstruct cached lightmap UVs from world position, retain 24-byte BSP
  vertices and spend the recovered cache capacity on a `4/3` grid restricted
  to detected high-contrast lamp regions
- [ ] Cache reusable surface lists for common cluster/area combinations where
  memory cost is lower than repeated BSP traversal cost

Completion criterion: heavy scenes submit substantially fewer surfaces and
vertices, with no visible popping, missing doors or broken moving brush models.

### P4 - Texture residency and GS page behaviour

- [ ] Measure texture upload churn, evictions, VRAM waits and repeated uploads
  per frame
- [ ] Keep frequently used HUD, weapon, particle and common world textures in
  stable VRAM slots where practical
- [ ] Reuse texture allocations across frames and reduce allocator
  fragmentation during level transitions
- [ ] Group world draws by resident texture/lightmap and upload only when the
  required generation is not already present
- [ ] Keep related mip levels in predictable nearby GS pages and measure texture
  page reload sensitivity in the benchmark scenes
- [ ] Identify large projected surfaces or extreme texture-coordinate ranges
  that cause repeated GS texture-page misses; subdivide only when measurement
  shows a net gain

Completion criterion: common gameplay frames perform few or no redundant
uploads, zero-free-VRAM stress remains artifact-free, and level transitions do
not accumulate stale texture allocations.

### P5 - VU1 geometry and lighting

- [ ] Move BSP vertex transformation and lighting from the EE to VU1
- [ ] Move MD2 interpolation, transformation and vertex lighting to VU1
- [ ] Compress geometry input with VIF unpack formats where precision permits,
  reducing EE RAM traffic and QW transferred per vertex
- [ ] Schedule VU instructions so load/store work overlaps vector arithmetic
- [ ] Keep a guarded EE fallback until BSP and every MD2 render flag match

Completion criterion: BSP and MD2 output remains visually equivalent while EE
geometry time, input bandwidth and VU1 idle time are measurably reduced.

### P6 - Specialized effect paths

- [ ] Add a compact particle path with shared state and batched billboard data
- [ ] Add a simplified turbulent water path with a bounded subdivision cost
- [ ] Consolidate glass, sprites and other transparent surfaces into a minimal
  state-change path
- [ ] Add quality limits for particles and translucent layers that degrade
  gracefully in unusually heavy scenes

Completion criterion: water, particles and transparency remain visually stable
while their combined EE time, batches and GS state changes are reduced.

### P7 - Advanced MFIFO and PATH scheduling

- [ ] Consider scratchpad-staged DMA only after pass-level batching profiles
  show main-memory packet generation or bus contention remains significant
- [ ] Prototype MFIFO buffering only if VU1/GS stalls still prevent useful EE
  overlap after P1-P5
- [ ] Evaluate GIF intermittent mode for large batched PATH3 texture transfers
  while VIF1/VU1 prepare PATH1 geometry
- [ ] Keep the synchronous proven path selectable until texture/geometry
  ordering survives PCSX2 and retail-PS2 stress tests

Completion criterion: an advanced path is retained only if it improves measured
frame time and never permits VU1 geometry to sample incomplete or reused texture
pages. Otherwise the simpler pass-batched path remains the release default.

### P8 - Memory and sustained-play validation

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
2. P1 DMA alignment, larger chains and removal of per-draw waits
3. P2 draw sorting/pre-upload scheduling and P3 culling
4. P4 stable texture residency and GS texture-page improvements
5. P5 BSP VU1 path, followed by the MD2 VU1 path
6. P6 specialized particle, water and transparency paths
7. P7 scratchpad/MFIFO/GIF intermittent experiments only if profiles justify them
8. P8 full campaign, transition and sustained-play validation

## Milestone 5 - Test release

Target: a documented build that another user can install and test without
development tools.

- [ ] Package the ELF without copyrighted Quake II data
- [ ] Document PCSX2 and real-console installation
- [ ] Provide default controller bindings and troubleshooting
- [ ] Publish known issues and a hardware compatibility list
- [ ] Tag and archive the first public test release

## Immediate priorities

1. Compare alpha.45 RELEASE and PROFILE at the same camera positions in a light
   room, a heavy combat scene and a water/particle scene; record minimum FPS and
   the PROFILE timing counters.
2. Start P1 by inventorying every VIF1/GIF wait and aligning large referenced
   vertex/DMA buffers to 128 bytes before changing synchronization behaviour.
3. Combine compatible world submissions into larger VIF chains, then remove
   per-draw waits only where texture and PATH ordering remain proven safe.
4. Revisit audio streaming after frame pacing is more stable; retain LOW
   11025 Hz as the nonblocking baseline until then.
5. Recheck multi-level transitions after the next optimization pass, including
   a longer Base 1 -> Base 2 -> Base 3 session on PCSX2 and real hardware.
6. Use the resulting PROFILE measurements to choose between P2 texture/batch
   scheduling and P3 culling as the next highest-impact stage.

Update this file whenever priorities, milestone status or completion criteria
change. Record completed user-visible work in `CHANGELOG` in the same commit.
