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
- [x] Gamepad demo benchmark: three matching demo1 passes validated on Alpha.79
- [~] Per-pass renderer cost summary for choosing optimizations (Alpha.80)
- [x] Matched world dynamic light ON/OFF benchmark (Alpha.93; six matching passes confirmed, +5.11 ms/frame light cost on user's setup)
- [x] Gamepad-accessible live mipmapping/format diagnostics for BSP artifact isolation
- [x] Textured BSP world geometry
- [x] Animated MD2 enemies, corpses and first-person weapons
- [x] NTSC/PAL framebuffer selection
- [~] VIDEO controls, archived settings, 240p/480i/480p output and scaled 240p UI (Alpha.103 weapon/UI and supported modes accepted; 480p hardware unverified)
- [~] Texture streaming within the PS2's 4 MB GS VRAM
- [~] Adaptive BSP vertex/lightmap lighting with depth-biased source-triangle crack sealing
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
- [x] Dynamic world lighting: user confirmed projectile illumination on Alpha.88
- [x] Dynamic world light cost reduction (Alpha.94: source-triangle rejection and sparse mask iteration; blaster accepted, ON36.81 vs36.03FPS)
- [x] Reuse exact dynamic colours of shared world vertices (Alpha.96 ~58% reuse; Alpha.97 isolated cache test +0.23ms/frame,37.46 vs37.13FPS)
- [x] Reduce duplicate dynamic-light bounds selection (Alpha.98; ON38.08FPS, lightcost3.64ms, walls/doors accepted)
- [~] Reuse surface-plane light masks across normal/crack-seal passes (Alpha.99; runtime comparison pending)
- [x] Detailed world-light selection/subdivision/vertex-colour profiling (Alpha.95; user ON/OFF details supplied, selection and colour are main diagnostic costs)
- [x] RF_BEAM laser tubes: user confirmed visibility on Alpha.88
- [x] Underwater appearance and damage colour blend: user accepted Alpha.89
- [~] Separate slime/lava, pickup and powerup blend validation
- [x] MD2 colour shells (RF_SHELL_*), including powerup/protection effects (Alpha.90; user confirmed protection and medic resurrection effects in Alpha.91)
- [x] SURF_FLOWING texture scroll on non-turbulent opaque/transparent surfaces (Alpha.90; user accepted ps2flow check in Alpha.91)
- [ ] Optional ref_gl projected model shadows (gl_shadows)
- [x] Sprite entities (Alpha.92: clipped submission and cutout/alpha fixture accepted by user)
- [x] Particles
- [x] Opaque brush entities such as moving doors and platforms
- [~] Sky surfaces and skybox
- [x] Transparent BSP surfaces such as Base 3 glass, validated on PCSX2 and PS2
- [x] Translucent entity alpha (Alpha.92: no depth writes, TGA alpha normalization and fixture accepted by user)
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
- [ ] Implement the streamed PS2 ADPCM music path described below
- [ ] Validate audio during level transitions and sustained gameplay

### Music playback plan

The original CD-audio tracks will not be bundled with the port. Music support
will use user-supplied files and preserve Quake II's logical CD track numbers,
so maps and cinematics can request the same music without requiring a physical
disc or CDDA emulation.

- [ ] Define a documented user-data layout such as
  `baseq2/music/track02.adp` and map Quake II CD track numbers to those files
- [ ] Add an offline conversion workflow based on PS2SDK `ps2adpcm`; document
  accepted source audio, target sample rate, stereo handling and loop metadata
- [ ] Stream PS2 ADPCM from `host:`/USB through IOP/SPU2 double buffers instead
  of loading a complete track into the 2 MB SPU2 sound RAM
- [ ] Keep music decoding and playback off the EE gameplay/rendering path;
  extend or add a small IOP-side streaming module if the static `audsrv` ADPCM
  API cannot sustain refillable buffers
- [ ] Add music volume, stop, pause/resume, track changes and level-transition
  cleanup without disturbing the gameplay sound-effect stream
- [ ] Measure I/O refill time, underruns, IOP RAM, SPU2 RAM and frame pacing on
  PCSX2 and a retail PS2 before enabling music by default
- [ ] Document that soundtrack files must be supplied by the user and are not
  included in releases

Initial target: reliable 22.05 kHz stereo (or an equivalent lower-bandwidth
profile selected by measurement), with 44.1 kHz stereo treated as a later
quality option rather than a requirement for the first implementation.

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
- [x] MD2 phase/count benchmark, including view weapon (Alpha.104; stable three-run profile received)
- [x] Reuse indexed MD2 clipping distances/colours and emit inside triangles directly (Alpha.105; visuals accepted, Tris/clip3.78->2.87ms)
- [~] Reuse indexed MD2 texture coordinates in opaque/clipped paths (Alpha.106; Tris/clip2.87->2.83ms, visual acceptance pending)
- [~] Reuse exact shared positions during cached BSP clipping (Alpha.107;40.27FPS/24.83ms, World11.57ms, explicit visual acceptance pending)
- [x] Separate World BSP/PVS, Sky, Geometry and Submit timings (Alpha.108;1.07/0.30/9.26/1.03ms)
- [x] Split World Geometry into preparation/lighting, clip/emission, prefetch and seal preparation (Alpha.109;6.97/8.84/0.21/1.58ms, diagnostic overhead)
- [~] Reject cached BSP triangles on common outside planes before full record expansion (Alpha.110; differential tests passed, runtime pending)
- [x] Measure cached BSP early-rejection coverage (Alpha.111;0.45% of all,0.49% of eligible)
- [x] Measure cached BSP packed-inside emitter coverage (Alpha.112;89.6% of all,98.78% of eligible)
- [ ] Read only BSP cache masks and emit at consecutive offsets (Alpha.113 experiment regressed:40.00FPS/World11.84ms; reverted in114)
- [x] Confirm recovery after restoring Alpha.112 renderer (Alpha.114;40.33FPS/24.80ms, World11.64ms)
- [x] Split BSP plane checks, vertex emission and remaining clipping work (Alpha.115;6.11/5.19/10.21ms, heavy diagnostic overhead)
- [x] Sample BSP root operations1/32 with rotating offset and full counters (Alpha.116; user confirms about62.48 samples from1999.61 roots/frame)
- [x] Empty nested timer reference (Alpha.117;6.66us/trial, residual~4.00us vs real~5.20us)
- [x] Separate Planes/Emit root sampling (Alpha.118; balanced~31.25 roots/category/frame;3.04/2.60us, empty1.32us)
- [ ] Fixed six-plane mask classification (Alpha.119 did not improve:39.74vs11839.83FPS; reverted120)
- [~] Distant static-light quality toggle (Alpha.120; full39.71 vs reduced41.88FPS, preliminary visual acceptance; full default,512/384 hysteresis)
- [x] Batched VU0 MD2 clipping transforms (Alpha.121;42.05vs12041.88FPS, preliminary console visuals accepted)
- [~] Distant dynamic-light subdivision quality toggle (Alpha.122; flash detail full/reduced; console speed/visual pending)
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
  VIF1 DMA waits, texture-upload `FINISH` waits and framebuffer-clear waits.
  The current PROFILE samples put total `VUWait` near 0.2-0.45 ms, so the
  riskier pass-level asynchronous packet staging is deferred until the larger
  EE-side world/entity costs have been reduced
- [~] Replace per-`DrawTriangles` `FLUSH + Wait` submission with larger
  pass-level VIF chains and wait only before a buffer or referenced range is
  actually reused. Alpha.63 copies caller vertices into a 128-byte-aligned
  3072-vertex staging area, stores constants inline and combines compatible
  draws until staging/packet capacity or a PATH/texture ordering boundary;
  supplied PCSX2 captures rendered correctly with `VIFchain` reduced from the
  Alpha.62 baseline 70/74/89 to 19/13/29; retail-PS2 validation is pending
- [~] Extend the existing VU1 double-buffered chunks to two or three EE-side
  packet buffers so EE preparation overlaps VU1 work and GS rasterization.
  Alpha.64 uses two bounded 3072-vertex staging/packet buffers and waits before
  reusing the in-flight one or at explicit ordering boundaries. Supplied PCSX2
  captures rendered correctly; the closest outdoor comparison reduced `VUWait`
  from 232 to 148 us at the same 13 chains. Retail-PS2 validation is pending
- [~] Reduce DMA tag count and report chain count, QW transferred, average
  vertices per chain and wait time in the profiling build. Alpha.62 caches the
  invariant GIF/GS state in both VU1 double-buffer halves during a draw and
  adds `VIFqw`, `VUvert` and `VUstate` counters; later chunks refresh only the
  batch header and changing draw tag. Supplied PCSX2 captures reported
  `VIFqw` values of 1911/2500/3107 for light/outdoor/heavy Base1 scenes, about
  25-34% below the previous payload, with correct rendering but no material
  FPS change. Alpha.63 then combined compatible draws: supplied PCSX2 captures
  reported `VIFchain` 19/13/29 versus 69/65/82 `Batches`, with correct rendering

Completion criterion: ordinary opaque passes no longer block after every draw,
DMA reference buffers meet the documented alignment, and measured VIF/VU/GS
wait time falls without rendering corruption on PCSX2 or real hardware.

### P2 - Draw sorting and texture transfer scheduling

- [x] Chain opaque BSP surfaces by texture before drawing
- [~] Sort all opaque world/entity draws by texture, lightmap and render state
  to reduce GS register changes and uploads. World BSP surfaces are already
  texture-chained. Alpha.65's additional resolved-texture entity sort was
  rejected after PCSX2 measured identical original/sorted runs (`3/3`, `11/11`,
  `12/12`), so the existing client order is retained without a second sort
- [ ] Submit transparent surfaces in a separate order-correct pass
- [~] Build a visible-frame texture working set before geometry submission;
  upload missing textures in batches before any dependent VU1 `XGKICK`.
  Alpha.66 prepares a bounded draw-order prefix of opaque BSP textures, up to
  32 misses and only while no current-frame allocation would be evicted
- [~] Keep texture transfers and PATH1 geometry correctly ordered without a GS
  `FINISH` after every individual texture. Alpha.66 drains preceding PATH1,
  emits the prepared PATH3 transfers in one chain and waits for one final
  `FINISH`; overflow and dynamic textures retain the proven synchronous path
- [~] Track uploads, evictions, texture switches, GS waits and state changes in
  the profiling build. Existing `TexUp`, `TexDMA`, `VRAMwait` and `VRAMsync`
  pair with the VRAM panel's per-image `Uploads` count. In Alpha.66, `TexUp`
  counts DMA upload batches, so it should fall below `Uploads` when prefetching
  combines multiple images

Completion criterion: texture and state-change counts drop, texture transfer
waits are amortized across a pass, and the former black-strip/stale-page bugs do
not return under zero-free-VRAM stress.

### P3 - BSP visibility and culling

- [x] Use Quake II PVS data to mark visible BSP leaves and parent nodes
- [x] Cache per-view-cluster visibility work and invalidate it only when the
  camera crosses the relevant BSP boundary
- [~] Add more aggressive frustum, backface, entity and bounding-box culling;
  world BSP nodes, brush entities and MD2 entities now have conservative
  frustum tests before their expensive render preparation. Alpha.54 PROFILE
  validation reduced `MD2Vert` from 1346 to 330, `MD2Corner` from 7452 to 1707
  and `Ent us` from 8330 to 4589 when looking away from several enemies, while
  FPS rose from 31 to 43 with no edge popping. Alpha.55 also rejects individual
  world faces outside the frustum even when their parent BSP node intersects
  it. Alpha.61 propagates the remaining plane mask through the BSP tree, so
  descendants do not retest planes already containing their parent. Supplied
  Alpha.61 PCSX2 tests completed without missing geometry and reported
  `BoxPlane` values of 572/1117/810 across the three Base1 scenes.
- [~] Avoid lighting, transforming or batching surfaces rejected by visibility;
  alpha.55 performs a conservative per-face check before texture/alpha chaining
  and exposes the independent PROFILE `SurfCull` counter. PCSX2 validation
  reached 8-112 rejected faces in the supplied Base1 views without geometry
  popping. Alpha.61 also skips face-bounds reconstruction throughout BSP
  branches already wholly inside the frustum and exposes `BoxPlane`/`SurfBBox`;
  the same Alpha.61 scenes reported `SurfBBox` values of 238/306/366.
- [~] Cache adaptive BSP tessellation independently from animated lightstyle
  colours, updating cached vertex colours without rebuilding the topology
- [~] Compact retained BSP-lighting vertices so heavy scenes fit more reusable
  topology inside the fixed cache budget
- [~] Profile a moderately coarser adaptive light grid before committing any
  quality default to release builds
- [~] Retain the coarse PROFILE light grid on smooth BSP surfaces while locally
  refining high-contrast lamp gradients; validate `LitFine`, `LitKB` and
  `LitBuild` together so the quality fix stays inside the persistent cache
- [x] Pack retained lightmap UVs to UNORM16, then remove them from the cache
  entirely by reconstructing them from world position; retained BSP vertices
  are now 24 bytes and PROFILE tests stay inside the fixed cache budget
- [~] Retain the coarse PROFILE light grid on ordinary surfaces and use a local
  `4/3` grid in detected high-contrast lamp regions; depth 7-8 was rejected
  after testing showed higher cache use without fixing the cross-surface edge
- [ ] Cache reusable surface lists for common cluster/area combinations where
  memory cost is lower than repeated BSP traversal cost

Completion criterion: heavy scenes submit substantially fewer surfaces and
vertices, with no visible popping, missing doors or broken moving brush models.

### P4 - Texture residency and GS page behaviour

- [x] Measure texture upload churn, evictions, VRAM waits and repeated uploads
  per frame. Alpha.67's supplied captures reported `E/R/S` of `22/22/15`,
  `21/21/7` and `34/34/22`, proving that all sampled uploads restored evicted
  textures and many victims had already been touched in the current frame
- [~] Keep frequently used HUD, weapon, particle and common world textures in
  stable VRAM slots where practical. Alpha.70 begins bounded soft retention of
  small recently used Pics (HUD, font and particle dot); weapon/world retention
  remains open. The cap is 256 KB or one quarter of the heap, and demand
  allocation can still reclaim retained ranges. Supplied Alpha.70 Base1 views
  reduced uploads from 40/40/62 to 32/34/47 at unchanged 20/20/15 FPS; the
  heavy view also contains fewer triangles, limiting direct comparison
- [~] Reuse texture allocations across frames and reduce allocator
  fragmentation during level transitions. Alpha.68's exact monotonic LRU was
  rejected after stable sequential scans increased uploads from `22/21/34` to
  `41/41/61`; Alpha.69 retains the serial only as a fallback behind a known-use
  opaque-world eviction plan
- [~] Group world draws by resident texture/lightmap and upload only when the
  required generation is not already present. Alpha.69 uses the complete unique
  BSP chain order to preserve nearer future residents and evict textures absent
  from the remaining world pass, or the farthest future use when unavoidable
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

Alpha.74 reached map three but hit a gameplay-time 256 KB image allocation
failure with only 799,384 bytes free / 133,704 largest chunk. Validate Alpha.75
direct RGB16 sky decode and pre-sky lighting-cache reclaim before resuming
VRAM optimization. The exact failed texture is not identified by the report.

Transition failure takes precedence over VRAM optimization: the user reports
Base1 -> Base2 fails after long exploration but succeeds after a quick run.
Alpha.73 confirms fragmentation: 12,582,584 bytes free, largest chunk 2,611,976
against a 5,063,632-byte request. Validate Alpha.74's segmented world fallback
after long Base1 exploration and repeated Base1 -> Base2 -> Base3 transitions;
old world/alias resources were already freed. GitHub releases remain paused.
Alpha.74 PROFILE CI and sanitized BSP/VRAM host tests passed; the local ELF is
ready. GitHub push/CI is authorized, while release publication awaits results.

1. Keep alpha.54 PROFILE as the culling baseline: repeat the same camera pairs
   after each renderer optimization and compare FPS, `Ent us`, `MD2Vert`,
   `MD2Corner`, `BoxCull` and visual popping.
2. Use the recorded alpha.62 Base1 captures as the VIF baseline: light,
   outdoor and heavy scenes produced `VIFchain` 70/74/89, `VIFqw`
   1911/2500/3107 and `VUstate` 93/112/128 with correct rendering.
3. Keep Alpha.72 sky viewport clipping: supplied views reduced sky transfers
   to 3 faces / 384 KiB each and the user noticed no sky problems. Total uploads
   are 32/28/58 at unchanged 20/20/15 FPS. Investigate contiguous allocation:
   reproduce excess disconnected victims in host tests before selecting victim
   spans. Heavy-frame 37 Wall / 10 Pic reloads and 476 us VRAMwait remain open.
   Preserve prefetch pins, bounded retention and GS/PATH synchronization;
   broader NTSC/PAL and rotating-sky validation remains outstanding.
4. Revisit audio streaming after frame pacing is more stable; retain LOW
   11025 Hz as the nonblocking sound-effect baseline until then. Implement
   music separately as user-supplied, double-buffered PS2 ADPCM streamed by
   IOP/SPU2 rather than mixed or decoded on the EE.
5. Recheck multi-level transitions after the next optimization pass, including
   a longer Base 1 -> Base 2 -> Base 3 session on PCSX2 and real hardware.
6. Continue P3 by rejecting invisible surfaces before lighting and batching;
   then use the resulting PROFILE measurements to choose between further P3
   work and P2 texture/batch scheduling.

Update this file whenever priorities, milestone status or completion criteria
change. Record completed user-visible work in `CHANGELOG` in the same commit.
