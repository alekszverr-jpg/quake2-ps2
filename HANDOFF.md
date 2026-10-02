# Development handoff

This file is the durable context for continuing development in a new Codex
task. Read it together with [ROADMAP.md](ROADMAP.md) and [CHANGELOG](CHANGELOG)
before changing renderer, audio or memory-management code.

## Repository checkpoint

- Development worktree: `C:\Users\user\.codex\worktrees\cb03\quake2-ps2`
- Local testing project: `C:\Users\user\Documents\quake2-ps2`
- Worktree branch: `codex/alpha70-small-pic-retention`; Alpha.103 changes are on fork main
- Fork used for pushes and releases:
  `https://github.com/alekszverr-jpg/quake2-ps2.git`
- Read-only upstream reference:
  `https://github.com/glampert/quake2-ps2.git`
- Current source/test version: `0.1.0-alpha.103` (PROFILE CI and host tests passed; wide-FOV weapon/runtime UI checks pending)
- Current implementation commit: `5faee7f`
  (`Keep view weapon visible across supported VIDEO field of view settings`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.72`
- Alpha.103 local PROFILE ELF SHA-256 (7,779,856 bytes):
  `15514638491A6C68C3FD5A12C880C81401CCD32CFDE8C79432AC44CC1A5FD496`
- CI `36975727522` passed all host sanitizer tests, map validation and PROFILE build for `5faee7f`.
  Both root ELF copies above match the downloaded CI artifact. The Alpha.72 release
  contains only `quake2-profile.elf`.

Local test builds may advance; no GitHub release is currently requested. This handoff-only
checkpoint does not advance `VERSION`.

## Alpha.103 view weapon at menu FOV values

User reported missing weapon after102. Read test config only targeted values:
fov100, ps2_video_mode1, hand0. Confirmed client CL_AddViewWeapon explicitly
returned forps->fov>90, regardless of output resolution. Remove only thatlegacy
wide-view suppression; retaincl_gun0 and missingmodel exits. No renderer changes.
Tests extract actualproduction CL_AddViewWeapon, verify submission for70/80/90/
100/110/120, model/animation flags and disabled/missingmodels. Config unchanged.
CI36975727522 passed allsanitizer/map/PROFILE checks; installed7,779,856-byte
ELF inbothroots matchingdownload/hash andAlpha.103 string. cl_ents GNU89
syntax check passed. Runtime pending weaponvisible at100..120 in240p andothermodes. User102screenshot
shows readableFPS/HUD; no explicit full240pUI/480p/hardware acceptance yet.

## Alpha.102 native240p UI correction

User101 screenshots now show valid HUD/font palettes, but characters missing
half their rows and HUD too low. Replace low mode UI logical640x448->320x224;
physical GS remains640x224. UiX multiplies2, UiY now1: eight font rows retained,
HUD/pics become native height instead of decimated. Physical pixel aspect
compensated horizontally. Ref viddef uses UiWidth/UiHeight; GS 3D clear,
projection/offset/scissor retain640x224. Cameraaspect320/224 equals old640/448.
Bottom yb HUD anchors get8-line safe area only at320x224. Game menu x32 instead
of halfwidth, VIDEO labels/hints shortened to fit40chars; long status clipped
withellipsis; benchmark title shorter and x0 inlowmode. Most benchmark rows
already fit40columns; do not rescale font to4rows again.
Tests exercise extracted actualDrawGlyph against mode coordinate mapping,
retained8-row glyph heights, whitespace/top clipping, native menu labels and
previous frame/depth-page tests. GNU89 syntax check passed for changed client
C files (existing64-bit host pointer-cast warnings inentitycmpfnc only).
CI36974957048 passed all host sanitizer tests/map validation/PROFILE build.
Installed7,779,844-byte ELF inbothroots matchingartifact hash andAlpha.102/menu
strings. Runtime pending240p Game/VIDEO/HUD/crosshair/
console and480i/480p controls. User has not reported101480p result yet.

## Alpha.101 correction after first VIDEO runtime feedback

User screenshots:240p world visible but HUD/crosshair/FPS text black rectangles;
480p also black UI, small centred image on1920x953 screenshot. Not hardware-tested.
Confirmed root cause: Z16S storage pages64x64, width640=10pages per row.
Linear graph_vram_allocate(height224/480) under-reserved10240words=40KiB;
last page row overlaps palettes/texture heap. AllocateDepthBuffer now rounds
storage height64 and passesGS_PSMZ_16S (complete storage PSM, not register ZSM).
Active height remains224/480; clear/scissor/projection unchanged. Host test
extracts actual allocator, emulatesSDKlinearallocation and enumerates all
pixel-page addresses for224/448/480/512. Old undercount shown, auto/480i size
unchanged. Set480p FFMD FRAME following official gsKit modetest:
https://github.com/ps2dev/gsKit/blob/master/examples/modetest/modetest.c
Small centred480p output may include emulator scaling; no claim of reproduced
geometry/viewport corruption. Asked user whether geometry wrong or only fields.
CI36972968419 passed all host sanitizer checks/map validation/PROFILE build.
Installed7,777,464-byte ELF in both roots, matching CI SHA and embeddedAlpha.101.
Runtime confirmation pending after repaired build, including HUD/font/crosshair
and480p dimensions. Existing 240p logical UI scaling retained.

## Alpha.100 VIDEO and output modes

User changed scope from lighting optimization to the blank VIDEO menu and
requested 240p/480i/480p with correct UI scaling. VIDEO uses stock client menu
controls, FOV70..120, world gamma1.2..0.3, world dlights, mipmapping, polyblend,
FPS, defaults and Back. Cvars archived; dirty exit calls CL_WriteConfiguration.
Changing mipmapping clears the diagnostic forced mip level. Do not expose
viewsize until 3D subviewport rendering is supported; current world projection
fills the complete GS buffer. Texture gamma/intensity remain startup-only.
ps2_video_mode:0 auto (old PAL512/NTSC448),1 NTSC noninterlaced224 active,
2 NTSC interlaced448/filter,3 HDTV480p centred640 wide. Requires full restart;
other settings live. UI uses640x448 logical canvas in224p; GS Filled/TexturedRect
maps Y by0.5, so menus/HUD/console/cinematics/fades share one coordinate system.
World projection/VU offset/clear/scissor retain physical Height(). Progressive
scanout uses circuit2 without interlaced flicker filter; 480p DISPLAY window
centred within libgraph720 timing and restricted640 pixels to avoid overread.
PS2SDK graph mode source consulted for signal/display constants:
https://github.com/ps2dev/ps2sdk/blob/master/ee/graph/src/graph_mode.c
CI36971605863 passed, downloaded ELF installed in both roots, matching hashes
and embedded version/menu strings checked. Full menu.c GNU89 syntax check passed.
No hardware/emulator runtime claimed before user validation.
Host tests production menu sliders/spins, callback/config wiring and mode/UI
mapping. Runtime tests pending: VIDEO controls/defaults/persistence; restart
for each video mode; HUD/console/main menu/benchmark/crosshair and water flashes
in240p; 480p display geometry; compare auto to previous accepted build.

Alpha.99 latest user compare ON38.05FPS/26.28ms/world12.20ms;
OFF44.14FPS/22.66ms/world8.76ms; lightcost3.62ms. ProfileSelect2.28ms each,
plane tests259.12 each versus411.99 (~37% fewer); split1.06..1.07ms,
colour1.01ms. Surface memoization reduces work but overall gain versus98 is
within measurement variation. No claimed FPS win; do not add more benchmark
modes while finishing VIDEO.

## Alpha.87 campaign archive repair accepted

2026-10-02 checkpoint: user accepted Alpha.98 blaster light on walls and doors.
Alpha.98 normal comparison summary ON38.08FPS/26.26ms/world12.18ms;
OFF44.20FPS/22.62ms/world8.74ms; lightcost3.64ms. Relative Alpha.96 ON37.45FPS,
world12.61ms/cost4.10ms, OFF44.23FPS/8.72ms: ON~+1.7%, cost~-11%, controlstable.
ProfileONSelect2.29/2.30/2.30ms, Split1.06/1.07/1.07, Color1.00/1.01/1.01;
Surfaces463.38, Plane tests411.99; Bounds calls622.26/627.20/627.81;
Bounds tests842.01/851.67/852.79; Rejected305.55/306.37/307.19;
Split nodes445.21/449.61/449.29; Splits138.65/139.89/139.85;
vertices1027.80/1035.74/1036.96; pairs583.85/586.71/587.85;
hits600.18/605.16/605.70; misses427.62/430.58/431.26.
Summary says Complete; run range pages not supplied. Timer results diagnostic.

Alpha.99 memoizes surface-plane light masks across ordinary and biased crack-seal
passes of one DrawTextureChains call. Fixed64-entry direct-mapped cache (~772bytes
static on PS2), no per-map/heap allocation or change to ModelSurface layout.
Pointer collisions recompute. Generation reset each frame and at every texture
chain context, covering world and each transformed inline brush, repeated model
instances and new maps/address reuse. Existing per-surface vertex-colour cache
reset remains even on mask hit. No-lights fast path skips cache operations.
Plane tests now counts only actuallycomputed plane/light pairs; surfaces still
counts requests. Triangle masks/recursive pruning/cache-colour arithmetic and
geometry unchanged. Production SelectSurfaceLights tested for empty/high-bit
mask hits, changed light coordinates aftercontextreset, no-lights path and
7200 masks across300surfaces/12contexts against scalar reference. Existing160
recursive,2400selector and12000colour tests retained. CI36968832834 passed all
host sanitizer tests/map validation/PROFILE build. Installed7,762,344-byte ELF
in both roots; SHA above matches CI, embedded Alpha.99 version verified. Map
fixture assets unchanged. Runtimegain pending. User should repeat normal world
lights compare (6 runs), then world lights profile ON page (Select ms/Plane tests
vs Alpha.98 2.30ms/411.99). Confirm blaster on walls/movingdoors as contexts changed.
No Alpha.99 runtime/visual acceptance yet.

2026-10-01 Alpha.97 isolated cache comparison completed. CacheON37.46FPS,
26.70ms/frame, world12.62ms; OFF37.13FPS,26.93ms,world12.84ms. Cachegain+0.23ms,
~0.9%FPS. Summary says Complete; run pages with frame ranges were not supplied
in this batch. Details ONworld12.60/12.64/12.62ms,entities8.30/8.31/8.30;
OFFworld12.85/12.84/12.82,entities8.32/8.31/8.32. DMA waits nearly equal.
ON~16200verts/~5400triangles, OFF~16201/~5400. Small measured benefit on this
setup, not a universal speedup. Cache remains defaultON. Next target selection.

Alpha.98 removes repeated source AABB selection only when static cached geometry
has exactly3vertices: BuildCachedLitTriangle/AppendCachedTriangle preserves
original source coordinates when no static subdivision occurred. Gather already
selects that source's light mask; pass boundsSelected=true at transient root.
Recursion defaultsfalse for children; child pruning/64-unitedge/depth7 unchanged.
The known root skips mask selection and position-array copying. Larger cached
subdivisions still test their own bounds. SelectTriangleLights unrolls axis
bounds/nearest-point arithmetic, keeping original min/max nesting, strict
radius comparison, squared-sum order and ascending nonzero mask iteration.
No added per-map storage/heap allocation or quality change. Frozen Alpha.97
selector verifies2400 varied/degenerate masks plus exact radius boundary cases;
160 Alpha.93 differential trees also exercise known-root path. Counter regression
proves exactlyone fewerbounds call with identical emitted tree/UV/colours.
12000colour requests and cacheON/OFF tests remain. CI36908578351 passed all
host sanitizer checks/map validation/PROFILE build. Installed7,758,308-byte ELF
in both roots with CI hash above and embedded Alpha.98 version verified. Fixture
assets unchanged. Runtime gain pending. User should run normal world lights
compare (6 runs), send summary/details, then separate world lights profile:
compare Select ms and Bounds calls/tests to Alpha.96; timers perturb FPS.
Also verify blaster illumination on flat walls and moving doors. No new runtime
FPS, profile counters or Alpha.98 visual acceptance yet.

2026-10-01 Alpha.96 feedback: normal world lights comparison has six matching
688-frame passes/demo195..882. ON37.45FPS/26.70ms/world12.61ms; all3passes18.37s.
OFF44.23FPS/22.61ms/world8.72ms;15.55/15.57/15.55s. Lightcost4.10ms vs
Alpha.94 4.08ms; ON+1.7% but OFF+2.1%, so cache FPS benefit unconfirmed.
Details ONworld12.61/12.60/12.60/entities8.31/8.30/8.31; OFFworld8.72/8.73/8.72,
entities8.12/8.12/8.11. ON~16200verts/~5400tris, OFF~15792/~5264.
Separate profile ONSelect2.83/2.85/2.85ms, Split1.05/1.06/1.06, Color1.01each;
vertices1030.22/1039.36/1040.49; computedpairs585.38/587.75/589.00;
hits601.70/607.49/607.75, misses428.52/431.88/432.73 (~58%reuse).
OFFSelect0.65each/Surfaces463.38; otherlightworkzero. Confirms less duplicate
work, not net FPS gain: Color timer excludeslookup and profile perturbs FPS.
Alpha.96 blaster/door visual acceptance not explicitly reported in this batch.

Alpha.97 adds Game/light cache compare (6 runs):3cacheON then3OFF while
ps2_world_dlights=1 for allsix and ps2_profile_world_lights=0. Five resultpages:
summary, cacheONruns, cacheOFFruns, cacheONdetails, cacheOFFdetails. Cachegain
is weighted OFF mean frame time minus ON; positive means benefit, negative
means overhead. Hidden if incomplete or demo frame counts/ranges differ.
New ps2_world_light_cache defaults1 and renderer refreshes perframe. Disabled
path bypasses key construction, lookup and insertion; still executes identical
scalar light arithmetic, splitting and rendering. Ordinary tests preserve the
caller's cache setting. All11 benchmark cvars restored on completion/cancel/
failure. Tests cover six modes, timers/lights invariants, allfivepage labels,
units/sign, cancellation afterOFFswitch, missing/empty/mismatched demos and
cache-disabled colours/geometry against independent reference. CI36903859510
passed all sanitizer tests/map validation/PROFILE build. ELF installed in both
roots; size/hash above match CI. Embedded Alpha.97 title, new cache-compare menu,
gain label and fixed-light/timers-OFF line verified. Assets unchanged.
User should launch Game -> light cache compare (6 runs), send summary and both
renderer-detail pages. No runtime cache gain result yet. Cache disabled still
retains fixed cache storage and surface epoch invalidation; bypasses actual
per-vertex memoization work. This compares the runtime cached/scalar paths in
one build, not a separate pre-cache executable.

2026-10-01 Alpha.95 profiling screenshots accepted as diagnostic measurements.
ON Select2.84/2.83/2.85ms, Split1.05/1.05/1.07ms, Color2.44/2.43/2.47ms.
Surfaces463.38 each; Plane tests411.99; Bounds calls651.51/647.42/656.94;
Bounds tests881.25/877.30/887.02; Rejected304.54/304.13/305.99;
Split nodes447.71/444.11/452.15; Splits139.26/138.13/140.75;
Lit vertices1031.81/1025.00/1040.90; Vertex tests1357.98/1351.60/1367.11.
OFF Select0.65ms each, surfaces463.38, all other light counters/times zero.
Do not interpret diagnostic times as additive FPS savings: timers perturb work.

Alpha.96 memoizes emitted-vertex dynamic colours in a bounded64-entry direct
mapped cache (1796bytes static, no heap allocation). Key: exact float xyz bits,
original packed RGBA (including alpha) and current32-bit light mask. Hit returns
identical previously computed packed result; collisions recompute. Clear on every
SelectSurfaceLights and every RenderFrame, covering static-light changes, moving
light frames and brush-local origin transformations. No UV/geometry/clipping,
falloff or light order changes. Profile pages append Color hits/Color misses,
14rows at8px spacing; Color ms now times misses only, excluding lookup (labelled).
Vertex tests count computed vertex/light pairs; Lit vertices count requests.
Independent uncached scalar reference verifies12000 colour requests including
mask, alpha, positions, negative colour channels and changing light epochs;
collision replacement/invalidation tests plus160 original recursion cases.
CI36868989858 passed all host checks but PS2 compile caught uint32_t being
unsigned long while engine u32 is unsigned int. 91d1c26 matches cache output to
std::uint32_t then explicitly casts returned value to u32. CI36869297879 passed
all tests/map validation/PROFILE build. Installed 7,754,308-byte ELF into both
roots, SHA verified and embedded Alpha.96/cache labels checked. Assets unchanged.
Runtime FPS/hit rate pending; use normalcompare for speed, separateprofile for
hit/miss diagnostic counts. No claimed speedup before matched userbenchmark.

2026-10-01 Alpha.94 accepted: user reports blaster light looks normal.
Matched six passes: 688 frames / 195..882 each. ON 18.68/18.72/18.68 s,
36.84/36.76/36.84 FPS; combined36.81 FPS/27.16ms, world12.79ms.
OFF15.89/15.88/15.86s,43.29/43.34/43.37FPS; combined43.32FPS/23.09ms,
world8.92ms; reported lightcost4.08ms (vs Alpha.93 5.11ms).
Details ON world12.81/12.80/12.78, entities8.57/8.56/8.55;
OFF world8.92/8.91/8.93, entities8.38/8.37/8.38. Geometry ON ~16200
vertices/~5400triangles; OFF~15792/~5264. DMA waits nearly unchanged.
ON FPS improvement2.2%; OFF also slower, so do not treat20% cost reduction
as a precise universally repeatable result. Alpha.94 visual check accepted.

Alpha.95 adds separate Game/world lights profile (6 runs) diagnostics.
Normal benchmark and compare force ps2_profile_world_lights=0; profile enables1.
All10 settings restored on finish/cancel/failure. Seven pages (existingfive,
then ON/OFF lightprofiles). Timers perturb FPS, explicitly labelled; use normal
compare for performance claims. Raw clock ticks accumulated before per-frame
microsecond conversion. Select includes surface plane and source/recursive AABB;
Split times only longest-edge/midpoint/clip-distance preparation, excluding
recursive children and submission; Color times emitted vertex light calculation.
Scope stop is idempotent. Counters include surface calls/light-plane pairs,
bounds calls/light-AABB pairs/rejected pairs, recursive nodes/splits, emitted
lit vertices/vertex-light pairs. Includes inline BSP brushes, excludes MD2.
No extra persistent per-map allocations, no visual algorithm changes.
CI36866985964 passed; 7,748,156-byte ELF installed in both project roots and
verified against downloaded SHA above. Embedded Alpha.95 title, profile menu
and ON/OFF labels checked. Fixture assets unchanged. User should launch Game ->
world lights profile (6 runs), then provide last two pages Light profile ON/OFF.
FPS from this diagnostic mode must not be compared directly with Alpha.94;
normal compare mode remains the performance reference. No runtime profile
results yet. Host tests exercise all seven pages, per-row units, six passes,
restoration, failed demos, idempotent timer stop and exact light-work counters;
160 differential light cases include enabled/disabled profiling.

Alpha.94 optimizes world dynamic lighting without changing illumination:
GatherPolyTriangles filters the surface plane mask by source-triangle AABB
before cached vertex preparation; cached subdivisions lie inside that source
bound. Misses regain packed-colour fast submission. The original surface mask
is restored at function exit for later polygons/crack seals. AddWorldLights
and transient bounds checks iterate nonzero mask bits via ctz in ascending order
(ctz never receives zero). Transient misses bypass edge-length selection.
No changed falloff, colour rounding, split limits or persistent allocation.
160 differential cases compare emitted geometry/UV/colour/light contributions
against Alpha.93's production recursion in tests/view_effects/dynamic_reference.inc,
including parent source-mask filtering. User benchmark and blaster check accepted.
Both root ELF copies match the downloaded successful CI artifact and embedded
Alpha.94 benchmark title. Fixture assets unchanged. Measurements above select
light CPU processing as the next profiling target. Glass-specific acceptance
was not separately reported in this batch.

Alpha.93 adds Game/world lights compare (6 runs), with three ON then three OFF
demo1 passes. ps2_world_dlights defaults1 and affects only s_worldLightCount,
including inline brushes; MD2 dynamic entity lights and animated styles retain
the same refdef. All benchmark settings including this cvar are restored at
completion/cancel/failure. Five pages: summary, ON runs, OFF runs, ON details,
OFF details. Cost = weighted ON mean frame ms - OFF mean; not shown for incomplete
or mismatched frame ranges across six passes. No extra warmup; loading excluded.
VERSION now supplies PS2_BUILD_VERSION to C/C++; Makefile invalidates objects
when VERSION changes. Host tests check six-run lifecycle, restoration, frame
range mismatch, title, page navigation and displayed weighted cost. Runtime
comparison completed on the user's setup; no optimization or speedup claim yet.
Both project-root PROFILE ELF copies match the downloaded CI artifact, including
the embedded title QUAKE II - BENCHMARK 0.1.0-alpha.93. Alpha.92 fixture assets
are unchanged. Initial CI 36860251873 caught an out-of-bounds detail-average loop
after expanding storage to six runs; d96ae39 limits the loop to the displayed
three-run group, and the successful CI sanitizer test exercises all five pages.

2026-10-01 Alpha.93 matched comparison screenshots: all six passes have 688
frames, demo range 195..882. ON: 19.10/19.10/19.08 s, 36.02/36.02/36.06 FPS;
combined 36.03 FPS / 27.75 ms, world 13.57 ms. OFF: 15.57/15.60/15.57 s,
44.18/44.11/44.18 FPS; combined 44.16 FPS / 22.65 ms, world 8.73 ms.
Summary reports +5.11 ms/frame light cost (unrounded group times), approximately
18.4% of ON frame time. World phase difference is 4.84 ms, explaining most of
the measured cost. Confirms world dynamic lighting as the next optimization
target; keep illumination enabled and visually equivalent. Renderer detail
pages were not supplied in this batch. Do not interpret OFF as an optimized build
or assume every light cost can be eliminated. Post-test setting restoration
is covered by host tests but not explicitly confirmed by this user feedback.

Alpha.92 implements an expanded ps2flow fixture for opaque/translucent MD2 and
SP2 samples (original assets, two frames, NPOT 96x80 cutout discs). Guarded
misc_ps2sample uses only stock server/client render flags; RF_TRANSLUCENT alone
receives 0.7 alpha in cl_ents. Blended VU batches now set ZMSK=1 with depth tests
retained; opaque draws restore writes and BeginFrame restores before clearing.
GIF state grows to eight AD writes, ten total tag qwords, vertex input offset11;
VU packet copy and reused draw-tag offset updated together, 84 vertices retained.
Sprites and translucent MD2 use the six-plane clipper. TGA alpha normalized to
0..128 on the texture-cache path (file decoder and sky RGB16 stay unchanged).
See tests/transparency and tools/effect-map.md. User accepted the fixture visuals
on 2026-10-01. Benchmark feedback is recorded below; performance cost isolation
remains pending. Do not attribute the full change since Alpha.83 to Alpha.92.
Alpha.92 PROFILE ELF and 16 runtime fixture assets are installed in both local
projects, all matching CI hashes. ps2flow.bsp is 59,872 bytes, SHA-256
`85731693AD0C8F536FABBFE23BEDCE7C59E1F5336EF56BCBE3256D3BFE860B74`.
Game -> test map -> ps2flow - renderer effects test loads the expanded room.

2026-10-01 user supplied benchmark screenshots after Alpha.92 installation:
688 frames / demo frames 195..882 each; 19.15/19.11/19.12 s;
35.93/36.00/35.99 FPS, combined 35.97 FPS / 27.80 ms.
World 13.60/13.57/13.59 ms; entities 8.42/8.41/8.40 ms;
setup .30, particles .47, VUwait .13, TexDMA .23, VRAMwait .16/.17/.16;
uploads 20.15/19.83/19.94, reloads 19.94/19.63/19.75,
evictions 20.17/19.78/19.90, VRAMsync 5.61/5.68/5.53;
VU vertices 16202.45/16198.91/16198.84; triangles 5400.82/5399.64/5399.95.
Screenshot title Alpha.83 is hardcoded in src/client/cl_benchmark.c:176,
not reliable build identification. Both installed root ELF hashes still match
Alpha.92. Treat this as post-install feedback, not proof of the launched path.
Compared with accepted Alpha.83: FPS -21.3%, frame time +5.93 ms; world +5.12 ms
and entities +.74 ms dominate. Versions between 83 and 92 restored effects,
including dynamic world lights, so this cannot isolate Alpha.92's cost.
Next: fix benchmark build identification and obtain a matched Alpha.91/92
comparison or isolate dynamic-world-light cost before selecting optimization.
User subsequently confirmed Alpha.92 fixture visuals all look correct.

Alpha.91: user requested a dedicated flowing-surface map after PAK scan found
no ordinary SURF_FLOWING faces in installed campaign maps. tools/effect_map.py
generates ps2flow, a sealed room with labelled STATIC/OPAQUE/GLASS samples and
a side inline moving DOOR. Six original WALs; no water or enemies. CI pins
yquake2/maptools 9b7e43334646f63f25a7aebfd62070b3aebc2554 and exports maps plus
textures as ps2flow-test-map. The compiler-only pics/colormap.pcx must never
be copied to runtime (would replace the game's palette). Alpha.91 adds the
menu entry only; renderer remains Alpha.90. Installation uses maps/ps2flow.bsp
and textures/ps2test/*.wal alongside the PROFILE ELF. User tested the map and
reported everything works on 2026-10-01; accept ordinary flowing surfaces on
this setup. User also confirmed protection and medic resurrection colour shells
work correctly on 2026-10-01. Doc tools/effect-map.md
describes expected behaviour.
The CI map and all six WALs are installed in both projects' baseq2 directories;
all copied assets and ELF hashes match the downloads. ps2flow.bsp is 48,408 bytes,
SHA-256 `389CF447216B7F548DD995C9BF579E135FC35E7554312FB5115FF6DFC6D762A5`.
Select Game -> test map -> ps2flow - flowing surface test and load the map.

Alpha.90 (2026-10-01): user authorized colour shells and ordinary flowing
textures. MD2 shells use stock mixed colours and 4-unit current-frame normal
offsets, constant RGB via neutral BeamTexture, existing entity alpha, inflated
culling radius and six-plane clipping even on non-weapon shells. Ordinary MD2
fast path remains unchanged. SURF_FLOWING adds a bounded modulo-repeat offset
only on submission: opaque cached/clipped triangles, crack seals and alpha
surfaces (world and inline brush). No animated cache key or persistent UV
mutation. Stock speed is 64 repeats per 40 seconds, not 64 pixels; turbulent
water has its own different scroll and is unchanged. Runtime targets: visible
powerup/protection shells, ordinary scrolling surfaces, clipping near camera,
normal model appearance and matching demo1 benchmark. Own player body is hidden
in first-person as in stock; do not infer a missing shell from that alone.

2026-09-30 Alpha.89 feedback: user says everything works, including damage
colour flashes; attached underwater view shows the composed water tint.
Accept water appearance/damage flash on this setup. Separate lava/slime,
pickup/powerup, performance and wider hardware validation remain open.
Read-only comparison against local stock ref_gl identified further omissions:
RF_SHELL_* flags are emitted by cl_ents but not handled in the MD2 renderer;
SURF_FLOWING scroll exists only in DrawTurbulentSurface, not ordinary opaque
or translucent faces; optional gl_shadows projected model pass is absent.
Translucent depth writes still differ from stock ref_gl. Music is intentionally
pending (Makefile builds null/cd_null.c). Recommend shells and ordinary flowing
textures before optional shadows; no renderer implementation made in this audit.

Alpha.88 feedback: user confirmed projectile lights work normally and lasers
are visible. Underwater appearance still lacks an overlay. Original local
ref_gl/gl_rmain.c R_PolyBlend draws v_blend after 3D; PS2 RenderFrame ignored
refdef.blend entirely. Alpha.89 adds the composed view tint using existing
gs::FillRect after vu1::Flush and before client HUD/console. The stock game
already supplies water RGB (0.5,0.3,0.2), alpha 0.4, and slime/lava/damage/
pickup/powerup blends. Do not replace these with a hard-coded blue water tint.
Honour gl_polyblend and cl_add_blend (clear alpha as well as RGB). Runtime
target: water entry/exit, HUD unaffected, damage/pickup flashes fade normally.

2026-09-30 follow-up: user tested through ware1 without crashes. Three renderer
gaps remain: underwater view distortion, projectile light on world surfaces,
and invisible RF_BEAM lasers. Alpha.88 implements these for runtime validation.
No new heap cache is introduced. Dynamic lighting is applied at submission,
with local brush light origins and transient bounded subdivision; static caches
remain reusable. Beams use a builtin 8x8 neutral texture and the existing six-
plane clip/alpha path after BSP transparency. Underwater FOV warps the world,
weapon and frustum together. Run matching demo1 benchmark and visually verify
submerge/emerge, blaster near walls/doors, lasers from both sides/near camera.

2026-09-30: user replied "Проверил, работает" to the Alpha.87 campaign
restoration test request. Treat the reported restoration issue as fixed on
the user's setup. No detailed route or separate flyer count was supplied;
do not infer retail USB validation, 3 -> 4 transition or long-session OOM
coverage from this confirmation.

User still sees fresh Base2 soldiers instead of dead enemies/bridge flyers on
story return. Inspection now finds directories (not files) named base1.sav,
base1.sv2, base2.sav/.sv2, base3.sav/.sv2, game.ssv and server.ssv under the
testing project's save/current. User confirms they appeared automatically.
Previously observed nonempty files do not prove current persistence. These
directories prevent normal fopen writes; exact ROM/backend creation mechanism
has not been captured.

Alpha.87 switches host boot from legacy ROM fileio to SDK-matched embedded
iomanX/fileXio, without resetting IOP, consistent with USB boot. Save writers
(game/level/server/copy destination) call Sys_PrepareSaveFile: rmdir only if
stat reports a directory at that exact filename. Empty directory repair is
allowed; nonempty directories fail explicitly, never recursively deleted.
Host fixture tests repair, preservation of existing regular files and refusal
with contents. Existing user save directories are left untouched by the agent.
The requested runtime target was host boot and a new story campaign return,
killed enemies remaining dead and crosslevel bridge flyer activation. User
confirmed the fix works. USB and long-session memory checks remain pending.

## Alpha.86 gameplay OOM target

Alpha.85 user reported third-map gameplay Quake allocation failure: 60,356
bytes, arena 21,962,736, used 21,727,048, free 235,688, largest chunk 44,688.
World 6.59 MB, textures 4.50 MB, alias 2.88 MB, Quake 5.61 MB. Local PAK
contains models/weapons/v_hyperb/skin.pcx of exactly 60,340 bytes, matching
the request with a 16-byte PS2 Z header. This identifies a likely late weapon
skin load, not a captured allocation stack.
User clarified hyperblaster was given through cheats; this failure is a separate
all-weapons stress case and does not establish a normal Base3 campaign OOM.

Alpha.86 mandatory normal/aligned heap allocation retries once after an
optional cache reclaimer reports freeing memory. Renderer registers callback
after model initialization and unregisters on shutdown. Reclaimer clears all
triangle lighting pointers before releasing 96 KB chunks; refuses while inside
RenderFrame or if no chunks exist. Retention stays disabled until next map
registration; scratch tessellation is the same, but world CPU cost can rise.
Nonfatal cache try-allocation never triggers reclamation. No sound/texture/
model ownership is evicted during gameplay. Host production-body tests check
retry/accounting, alignment, optional allocation, cache invalidation and render
guard. Require long Base3 play, weapon pickup/switching (especially hyperblaster),
lighting/edge visuals and 3 -> 4 transition, plus benchmark. No universal OOM fix.

Read-only inspection after user's Alpha.85 test found base1/base2 .sav and
.sv2 files under baseq2/save/current (plus base2 copies under save0). Creation
now works; restoration is still unconfirmed. Do not delete these user saves.

## Alpha.85 campaign state and transition test target

User confirmed Alpha.84 third-to-second return works, but third-to-fourth
failed requesting 274,768 Quake bytes. Arena 21,660,144, used 6,490,360,
free 15,169,784, largest free chunk 235,968. All renderer model tags zero;
Quake 4.98 MB remains. Exact allocating asset is not identified. Alpha.85
also stops sounds and releases cached PCM before CM_LoadMap, retaining stable
sfx identities for subsequent registration. Expired alias truename allocations
are freed rather than lost during memset. Need long-play 3 -> 4 validation.

User reports killed enemies reset on campaign returns. Sys_Mkdir and Sys_Find*
were stubs; Alpha.85 implements POSIX/newlib mkdir, bounded directory search,
wildcard matching and attribute filtering. Existing SV/Game save/restore now
has directory creation and archive copy/wipe enumeration. Host fixture tests
real FS_CreatePath, Sys_Find*, SV_WipeSavegame and S_PurgeLevelSounds bodies;
actual game entity serialization and host:/mass: writes require runtime tests.
Start a new campaign (previous missing state cannot be recovered), kill enemies
and return 1 -> 2 -> 3 -> 2 -> 1 via story exits, check deaths, doors/items and
cross-level bridge flyers, then reach map four. The game-data directory must
be writable. Check sound after transitions and three benchmark passes too.
Do not claim campaign-wide stability or restore confirmation yet.

An unrelated user image shows DEMO1 cache-detail Build A1850/969 frames;
it is not an Alpha.84 Quake II benchmark result and has not been recorded
as a Quake II performance comparison.

## Alpha.84 transition fix and accepted Alpha.83 baseline

User reported third-to-second map return failure: Quake allocation 2,076,404
bytes, free 7,116,792, largest chunk 828,032. Mdl_World was already zero, but
alias models (2.78 MB) and textures (4.40 MB) survived the early server purge.
Alpha.84 extends PS2_PurgeLevelRendererMemory to release all models and
nonpersistent textures before CM_LoadMap's full BSP file read. Lighting cache
clears before world ownership; stale 3D submissions remain guarded until
client EndRegistration. Pics/built-ins retain their established lifetime.
Host test checks actual purge bodies/order and repeated model purge; it does
not reproduce the retail EE fragmented heap. Require long exploration and
the 1 -> 2 -> 3 -> 2 sequence, plus repeated benchmark and normal gameplay.
No campaign-wide memory stability claim yet.

Alpha.83 user confirmed visuals normal. Three passes 688 frames/195..882;
45.74/45.74/45.73 FPS, combined 45.73 FPS / 21.87 ms. World 8.46/8.47/8.45 ms,
entities 7.67/7.66/7.67. Accepted performance baseline.

## Alpha.82 baseline and Alpha.83 implementation

Alpha.82 user confirmed no observed visual issues. Three passes 688 frames,
195..882; 43.77/43.82/43.76 FPS, combined 43.78 FPS / 22.84 ms. World
9.40/9.38/9.40 ms, entities7.70/7.69/7.70. This is the next comparison baseline.

Alpha.83 defers full ClipVertex preparation for cached world triangles until
clipping is needed. The interior path computes distances from the same Vec4
positions, tests the same planes and writes cached fields straight to scratch.
Fallback uses zero-initialized records plus copied distances (spare lanes now
explicitly zero). No memory/cache growth. Need both benchmark pages and visual
checks at walls/edges, moving brush models, water/glass and crack seals.
The old third-map OOM still needs a separate long-playthrough validation.

## Alpha.81 result and Alpha.82 world candidate

Alpha.81 user screenshots: 688 frames/195..882 each; 39.30/39.27/39.26 FPS,
combined 39.28 FPS / 25.46 ms. World 11.73/11.73/11.72 ms, entities
7.80/7.80/7.81 ms, geometry essentially unchanged. MD2 packing candidate
has measured benefit (~4.8% FPS), but explicit visual colour confirmation is
still missing. Same-hardware/settings comparison is assumed, not recorded.

Alpha.82 bypasses cached-world colour unpack/repack only for triangles where
all original six clip distances are >=0. Directly emits cached packed colour;
partially clipped triangles keep the old interpolation and submission path.
Scratch size and persistent caches unchanged. Need both benchmark pages,
same 688 frames/range, and visual checks of wall lighting/camera edge movement,
crack seals, doors, water/glass and weapons. Compare 39.28 FPS/world11.73ms.

## Alpha.80 measured baseline and Alpha.81 candidate

User supplied both Alpha.80 pages: 688 frames/195..882 each; 37.48/37.50/37.44
FPS, combined 37.48 FPS / 26.68 ms. World 11.70/11.70/11.71 ms; entities
9.21/9.20/9.20 ms; setup .30, particles .47, VUwait .13, TexDMA .23, VRAMwait
.16 ms. Uploads 19.83/19.64/20.43, reloads 19.62/19.45/20.23, evictions
19.80/19.60/20.38, syncs 5.55/5.57/5.62. Vertices about 15807; tris about5269.
Alpha.80 summary overhead is below this run's variation versus Alpha.79.

Alpha.81 candidate only moves non-weapon MD2 PackFloatColor from indexed
corners into unique-vertex preparation. Uses the same function and alpha,
keeps float colour weapon clipping and skin seams untouched. A union preserves
32-byte scratch layout; no heap/cache growth. Need both benchmark pages and
visual checks of enemy/pickup lighting, translucent entities and view weapons.
Compare to 37.48 FPS and entity 9.20 ms, keeping frames/ranges and setup fixed.
Do not claim improvement before measured results.

## Validated Alpha.79 baseline and Alpha.80 next step

User screenshot confirms all three passes complete on Alpha.79 with identical
688 frames, IDs 195..882; 18.35/18.36/18.33 seconds; 37.49/37.48/37.53 FPS.
Combined 37.50 FPS / 26.67 ms, range about 0.13%. The black results background
is correct. Exact hardware/emulator settings have not been recorded; compare
on the same setup. This validates the benchmark flow, not the third-map OOM fix.

Alpha.80 adds Left/Right renderer details with per-pass averages from existing
counters, sampled only after measured frames. Includes HUD/final waits; wait
components overlap world/entity times. Uses fixed-size 64-bit totals. The next
user run should provide BOTH pages: compare FPS with 37.50 to assess sampling
cost, then use world/entity/texture/VRAM costs to choose a concrete optimization.
No renderer speedup is claimed in Alpha.80. Three passes/no warm-up unchanged.

## Alpha.79 loading-plaque correction

Alpha.78 user report: pass one finishes (688 frames / 18.37s / 37.52 FPS),
then LOADING remains. Disabling developer enabled the stock plaque path.
Repeated demo serverdata has the same servercount, and CL_ParseFrame only
attempts plaque dismissal on the initial active transition, potentially before
registration. Alpha.79 dismisses the plaque at benchmark BeginFrame only with
fresh serverdata + valid active frame + refresh_prepped; cancel clears it too.
Tests simulate a held plaque through stale/unprepared states and all repeats.
Runtime validation pending; do not yet treat this as a stable baseline.

## Alpha.78 benchmark measurement correction

The user completed Alpha.77: 688/689/689 frames, 22.38/24.46/24.63 seconds,
30.74/28.17/27.98 FPS; console text obscured results. Inspection identified
stale cl.frame/refreshed state after queuing subsequent demomap: measurement
could start before fresh serverdata, including reload time. Alpha.78 gates each
pass on CL_ParseServerData, deduplicates serverframe IDs, displays ranges and
flags differing counts/ranges. Developer output and notify text are disabled
and restored; the results draw over a black fill. No extra warm-up was added.
Regression tests pass locally; runtime must confirm all three ranges/counts
match and the first/later timing discrepancy is reduced. Alpha.77 numbers are
not a directly comparable optimization baseline after this protocol change.

## Publication preference

Alpha.76 runtime failed immediately after selecting benchmark: the log shows
demo1 found, server spawned and then ShutdownGame, before demo playback.
Root cause: SV_InitGame calls CL_Drop for normal startup; Alpha.76 had put
CL_BenchmarkCancel at the beginning of CL_Drop, which queued killserver even
for an already-disconnected client. Alpha.77 moves cancellation into the two
recoverable Com_Error paths, leaving normal drops alone. The host test now
extracts/compiles the actual CL_Drop body to cover this engine interaction.
Runtime recheck is pending; do not treat Alpha.76 as a working benchmark baseline.

Alpha.76 adds `Game -> benchmark (3 runs)`. It uses stock `demos/demo1.dm2`
from the virtual filesystem (confirmed in the user's pak0: 696 records and
normal -1 end marker). Three passes, no separate warm-up, loading excluded.
The summary shows per-pass frames/time/FPS and combined frames / total time;
it does not yet collect percentile times, VRAM counters or memory snapshots.
Start ends the current session without saving. Any button during the demo
cancels through the stock attract-loop menu handling. Timedemo bypasses the
client FPS cap and GS VSync wait but retains rendering completion barriers.
Compare the same game data, hardware/emulator speed and renderer settings;
these throughput results are not VSync-limited gameplay FPS. Runtime validation
is pending: three complete passes, results/back navigation, cancellation,
restored diagnostic overlays, and normal gameplay VSync after the benchmark.
The Alpha.75 third-map OOM runtime check is still pending and independent.

The user explicitly authorized any GitHub actions on September 14, including
push/CI. Continue PROFILE-only builds. New numbered releases remain deferred
until a useful runtime result, as requested earlier. Alpha.72 is the latest
public release; Alpha.83 is available locally for testing. Both root ELF copies
were updated on September 30 and verified against the CI hash.

## Workspace safety

The worktree intentionally contains many untracked files, including the user's
game/source data, downloaded CI builds and local tools:

- `Quake2Game/`
- `Quake2Source/`
- `build/ci-*/`
- `tools/`
- local archives and diagnostic images under `build/`

These files belong to the user. Do not delete, clean, move or commit them. Stage
only the exact source/documentation files changed for a release. In particular,
never use `git clean`, `git reset --hard` or broad recursive deletion here.

`*.elf` is ignored. After a successful PROFILE CI build, copy the downloaded
`quake2-profile.elf` to the project root for the user's convenient testing even
though that root copy is not committed.

## User workflow and test constraints

- The user currently tests only `quake2-profile.elf`.
- PCSX2 is the usual short-cycle target; important renderer and stability fixes
  are periodically checked on a retail PS2.
- The user cannot type into an in-game console. Every test switch, diagnostic,
  map selector or cheat needed for validation must be reachable by gamepad/menu.
- Preserve the gamepad Give All test shortcut and the map-selection menu.
- Lead with a ready-to-test ELF/release and concise test positions. The user can
  provide screenshots and observed FPS/counters.
- Keep `ROADMAP.md`, `CHANGELOG`, `VERSION` and the README version badge in sync
  with code releases. Publish numbered GitHub prereleases.

## Validated project state

The port boots from host files in PCSX2 and from FAT32 USB through uLaunchELF on
real hardware. The following major paths have been implemented and validated:

- Quake II BSP levels, MD2 models, weapons, enemies and gameplay interaction
- Dual-stick controls: left stick movement, right stick view
- Doors, buttons, lifts, map transitions and a gamepad map selector
- Textures and mipmaps without the former wall strips or camera-motion shimmer
- Coloured/static/animated lighting, player/entity lighting and muzzle flashes
- Sky, water, glass transparency, particles and transparent surface ordering
- Conservative BSP, face, brush-entity and MD2 frustum culling
- AI player detection and close-range attacks
- PROFILE/release build separation and gamepad-controlled diagnostics

Renderer changes must be checked for regressions in all of the following:

- black/foreign texture strips caused by stale or reused GS pages
- sky pixels leaking through BSP cracks
- dark stripes caused by crack-seal depth placement
- triangle-shaped particles over water or sky
- glass losing transparency or showing unrelated textures
- stretched/corrupted weapon geometry
- missing doors, moving brush models or entities at screen edges
- differences between PCSX2 and retail-PS2 ordering

## Known unfinished areas

- Heavy scenes remain EE-bound and often run around 15-20 FPS in the PROFILE
  build; the light Base1 position is around 26-31 FPS with diagnostics enabled.
- Map transitions can still exhaust EE memory. This is not proven to be only a
  delayed unload: retained level/model/texture allocations and fragmentation
  must be measured under the P8 plan.
- Low-rate game audio works in stereo but can crackle. The HIGH audio mode is
  silent or can hang and remains deferred until frame pacing is healthier.
- Music is not implemented. The roadmap records a separate user-supplied,
  offline-converted PS2 ADPCM streaming design using IOP/SPU2 double buffers;
  copyrighted CD audio must not be bundled.
- Campaign-wide rendering, cinematics, long-session memory stability and all
  special effects are not yet fully validated.

## Priority: Alpha.74 runtime image OOM on the third map

The user reached the third map and reported a gameplay-time TexImage OOM:
262,144 bytes / alignment 128; arena 21,901,296, used 21,101,912,
free 799,384, largest free chunk 133,704. Tags include Mdl_World 6.43 MB,
TexImage 3.89 MB, Alias 2.88 MB, Quake 5.75 MB. This is both low available
memory and fragmentation, distinct from the earlier large world-hunk failure.
Reaching map three does not prove every transition/fallback path is validated.

Alpha.75 targets the matching lazy-sky load path: a 256x256 TGA previously
needed a 256 KB RGBA decode plus a simultaneous 128 KB RGB16 conversion.
Sky now decodes directly into its final 128 KB RGB16 output, retaining identical
packing and row order. Regular TGA images still use RGBA32. The first sky
resolution also clears regenerable lighting caches before world submission;
no cached geometry pointers are in use at that point. No texture quality loss
or change to depth/PATH ordering is intended. The failing filename was not
shown, so runtime confirmation of this targeted fix is still required.

Next: repeat the route to map three and explore/fight until the previously
failing view, checking sky, lights, doors, water/glass and weapons. If it fails,
record full HEAP/tags. Large non-sky images, source-file buffers and aggregate
memory pressure may still require further work. Keep releases deferred.

## Alpha.74 verification

Native MinGW host tests and Linux CI ASan/UBSan tests passed for segmented BSP
allocation and VRAM accounting. PROFILE CI 34801447124 passed for 1d4ae74.
Only quake2-profile.elf was built for PS2; both root copies match the hash above.
The tests validate allocation alignment, zero-fill, lifetime, repeated teardown
and exact-size fallback. The long Base1 -> Base2 gameplay test remains pending.

## Current transition result and Alpha.74 test target

Alpha.73 crash screenshot confirms heap fragmentation: arena 21,440,592 bytes,
used 8,558,008, free 12,582,584, largest free chunk 2,611,976. The requested
world hunk is 5,063,632 bytes. Old Mdl_World/Alias are zero; this is not simply
an old-world unload delay. The screenshot does not identify which surviving
allocations originally fragmented the heap.

Alpha.74 tries the normal contiguous world hunk first. On failure, world arrays
and small polygon records use a list of aligned zero-filled segments owned by
ModelInstance. Small allocations share up to 64 KB; larger arrays get their own
contiguous segment. If preferred chunk allocation fails, the exact record size
is attempted. The prepass logical capacity/BytesUsed check remains active.
Unload frees every block with its actual allocated size; inline models clear
both ownership pointers. Alias/sprite layouts and all renderer barriers stay
unchanged. No pointers are relocated and no game data is modified.

Host tests cover the real segment helper with a smaller allocation limit than
the reported hunk, zero-fill/alignment/data preservation and repeated cleanup.
These are not hardware transition tests. Individual BSP arrays still need a
contiguous chunk, and segment tails/headers add some memory overhead.

Next: use Alpha.74 after long Base1 exploration, then enter Base2. Also try a
quick-run control and Base2 -> Base3 / repeated map changes. Check doors/lifts,
lighting, sky and model geometry. If it fails, capture the complete HEAP/tag
report: the failed request now identifies any remaining oversized array or
pressure. Keep releases paused until a useful result is confirmed.

## Previous diagnosis: Base1 -> Base2 EE allocation failure

The user reports that long Base1 exploration leads to a transition crash, while
speedrunning Base1 allows Base2 to load. The screenshot shows a failed
5,063,632-byte / alignment 16 / Mdl_World request. Tags: Quake 6.93 MB,
Renderer 1.04 MB, TexImage 73.01 KB, OpNew 72.04 KB, total 8.12 MB;
Mdl_World and Mdl_Alias are zero, with allocation/free counts balanced.
Do not diagnose this as the old world failing to unload or as a proven leak.

LoadBrushModel allocates one exact contiguous hunk after ComputeBrushHunkSize,
while ModelCache::LoadModel still holds the full FS_LoadFile buffer. Long-lived
allocations from gameplay may fragment the heap; the tag report cannot prove it.
Alpha.73 adds dlmalloc arena/used/free bytes and largest free chunk to the error
report. These include allocator bookkeeping and untagged libc allocations;
free covers the acquired arena only, largest chunk includes metadata. No heap
policy is changed, and no hardware reproduction or fix is claimed.

Historical Alpha.73 test request: test Alpha.73 on the long Base1 route and collect both HEAP
lines if the failure recurs, with a fast-run control. If enough heap is free
but no large chunk fits, consider segmented BSP ownership or reducing source
file/hunk overlap. If total pressure dominates, measure Quake sound/game/cache
lifetimes before purging anything. Preserve game data and existing transitions.
The VRAM optimization task below is deferred until transition stability improves.

## Latest PROFILE result: Alpha.72 sky clipping accepted for supplied views

The user supplied three Base1 screenshots and reported no noticed sky problems.
This validates the change in those views; no specific NTSC/PAL, rotating-sky or
retail-PS2 matrix was supplied, so those checks remain open.

| Scene | FPS | Uploads | E/R/S | TexUp | TexDMA us | VRAMwait us | VRAMsync | VIFchain | VUWait us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Light | 20 | 32 | 36/32/15 | 18 | 465 | 206 | 10 | 20 | 90 |
| Outdoor | 20 | 28 | 28/28/3 | 14 | 431 | 208 | 8 | 16 | 160 |
| Heavy | 15 | 58 | 59/58/23 | 37 | 671 | 476 | 20 | 34 | 189 |

| Scene | Pic N/R/KB | Skin N/R/KB | Wall N/R/KB | Sky N/R/KB | W/E/A/P/2D |
| --- | --- | --- | --- | --- | --- |
| Light | 0/0/0 | 4/4/169 | 25/25/226 | 3/3/384 | 27/5/0/0/0 |
| Outdoor | 0/0/0 | 9/9/285 | 16/16/112 | 3/3/384 | 18/9/1/0/0 |
| Heavy | 10/10/5 | 8/8/274 | 37/37/290 | 3/3/384 | 28/20/0/0/10 |

Other/Sprite are zero. Sky transfers fell from Alpha.71's 4/5/5 faces and
512/640/640 KiB to 3/3/3 and 384 KiB in each scene. Total uploads changed
33/32/59 -> 32/28/58; FPS is unchanged. Heavy VRAMwait increased 387 -> 476 us
and VRAMsync 17 -> 20, so reduced sky bandwidth has not solved general churn.

Geometry Tris/Batches/VUvert is 4016/71/12048, 5505/66/16515 and
6884/86/20652. World/Ent/3D us is 18771/8734/27799, 18506/12142/30921,
29967/18385/48647. LitBuild/LitColor are zero. Camera/geometry differ from
Alpha.71, so whole-frame timing differences are not isolated speedup evidence.

## Exact next task: investigate contiguous allocation churn

Keep Alpha.72 sky clipping. The remaining heavy view reloads 37 Wall images,
8 Skins and 10 Pics. Source review shows demand and prefetch choose victims
individually by retention/plan/LRU without considering whether their addresses
contribute to a usable contiguous range. First-fit is retried after every
victim. Thus separated evictions can discard more textures than a suitable
local span requires; these screenshots do not measure how much churn that causes.

Before changing policy, reproduce this with the host allocator tests using
unequal-size blocks and separated free spans. Evaluate bounded selection of a
contiguous victim span, retaining the small-Pic budget and future-use priorities.
Measure evicted blocks/words per allocation, largest free span and final reloads;
do not claim fragmentation from total free KB alone. Preserve prefetch's
no-current-frame-victim rule and failure-without-mutation guarantee, all GS/PATH
barriers and bounded demand fallback. Do not reserve sky/skin memory blindly.
Use the Alpha.72 tables above as the next runtime comparison baseline.

## Previous PROFILE result: Alpha.71 identifies sky upload traffic

Supplied Base1 light/outdoor/heavy screenshots, not agent-run hardware tests:

| Scene | FPS | Uploads | E/R/S | TexUp | TexDMA us | VRAMwait us | VRAMsync | VIFchain | VUWait us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Light | 20 | 33 | 30/33/9 | 19 | 527 | 246 | 11 | 21 | 85 |
| Outdoor | 20 | 32 | 32/32/5 | 18 | 576 | 261 | 11 | 20 | 152 |
| Heavy | 15 | 59 | 58/59/21 | 40 | 808 | 387 | 17 | 37 | 207 |

| Scene | Pic N/R/KB | Skin N/R/KB | Wall N/R/KB | Sky N/R/KB | W/E/A/P/2D |
| --- | --- | --- | --- | --- | --- |
| Light | 0/0/0 | 4/4/169 | 25/25/223 | 4/4/512 | 28/5/0/0/0 |
| Outdoor | 0/0/0 | 9/9/285 | 18/18/123 | 5/5/640 | 22/9/1/0/0 |
| Heavy | 11/11/5 | 8/8/274 | 35/35/263 | 5/5/640 | 31/17/0/0/11 |

Other/Sprite are zero in all three. Type/phase sums equal Uploads. Sky is the
largest pixel payload contributor, even in the light indoor view. HUD retention
works in the first two captures but still churns in the heavy frame. Sky faces
are 128 KiB each; the world plan cannot avoid their earlier sky-pass transfers.
The current sky code clips to the VU +/-0.8 guard, whereas visible GS-scaled
NDC spans only screen width/4096 and height/4096. This unnecessarily submits
some faces entirely outside the framebuffer.

Geometry Tris/Batches/VUvert is 3699/70/11097, 5314/65/15942 and
7369/86/22107. World/Ent/3D us is 18165/7182/25695, 21040/11481/32794,
32368/17378/50041. FPS remains 20/20/15. Camera, weapon and entity differences
limit comparisons with prior captures. No obvious new corruption is visible;
static screenshots do not establish motion or retail-PS2 correctness.

## Alpha.72 sky clipping implementation and remaining coverage

Alpha.72 replaces only sky side-plane distances with the actual framebuffer
bounds plus two pixels of overscan per edge. The existing triangle clipper then
rejects off-screen sky faces before they can bind/upload textures. It preserves
rotation, full-resolution images, seam sampling, exact far Z and sky-before-world
ordering. Alpha.70 retention, world/weapon clipping and PATH barriers are intact.

Compare Sky N/R/KB against Alpha.71's 4/5/5 and 512/640/640 KiB in the same
Base1 views. Also record total Uploads, Pic/Skin/Wall, phase counts, TexDMA,
VRAMwait/sync, geometry and FPS: fewer sky transfers must not worsen total churn.
Rotate horizontally/vertically through all sky seams and screen edges, walk
between indoor/outdoor views, and check NTSC/PAL, sky rotation and the existing
weapon/water/glass/particle regression list. Supplied-view results are above.
If sky traffic remains large after clipping, investigate portal bounds before
considering permanent residency or a quality change. Heavy HUD churn remains open.

## Previous PROFILE result: Alpha.70 partial churn improvement

Supplied Base1 screenshots (light/outdoor/heavy):

| Scene | FPS | Uploads | E/R/S | TexUp | TexDMA us | VRAMwait us | VRAMsync | VIFchain | VUWait us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 20 | 32 | 32/32/9 | 18 | 520 | 224 | 10 | 20 | 85 |
| Base1 outdoor | 20 | 34 | 34/34/9 | 20 | 611 | 260 | 10 | 22 | 170 |
| Base1 heavy | 15 | 47 | 47/47/20 | 30 | 726 | 421 | 18 | 33 | 179 |

| Scene | Nodes | Surfs | Tris | Batches | BoxCull | SurfCull | BoxPlane | SurfBBox | VIFqw | VUvert | MD2Vert | MD2Corner |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 563 | 326 | 3652 | 70 | 21 | 49 | 572 | 238 | 2374 | 10956 | 949 | 5655 |
| Base1 outdoor | 692 | 322 | 5679 | 71 | 26 | 104 | 1128 | 326 | 2907 | 17037 | 1373 | 7845 |
| Base1 heavy | 578 | 468 | 6725 | 83 | 41 | 83 | 849 | 373 | 3371 | 20175 | 1073 | 6090 |

World/Ent/3D times are 18167/7319/25777, 20835/12181/33287 and
29889/16821/47006 us. LitBuild and LitColor are zero in all three captures.
Uploads fell from Alpha.69's 40/40/62 to 32/34/47; FPS is unchanged. The
heavy view has fewer triangles/entities and the outdoor weapon/camera differ,
so the whole reduction cannot be attributed to retention. Recent-upload labels
now show WALs/model skins instead of HUD Pics. No obvious new corruption is
visible in these static views; motion/glass/water/retail-PS2 remain unverified.

## Previous PROFILE result: Alpha.69 rejected for performance

The supplied September 13 Base1 screenshots report the following light,
outdoor and heavy views. These are user-supplied observations, not an agent-run
PCSX2 or retail-PS2 test.

| Scene | FPS | Uploads | E/R/S | TexUp | TexDMA us | VRAMwait us | VRAMsync | VIFchain | VUWait us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 20 | 40 | 45/40/12 | 26 | 587 | 295 | 13 | 21 | 85 |
| Base1 outdoor | 20 | 40 | 40/40/5 | 27 | 669 | 279 | 11 | 19 | 171 |
| Base1 heavy | 15 | 62 | 62/62/27 | 47 | 902 | 614 | 26 | 40 | 210 |

| Scene | Nodes | Surfs | Tris | Batches | BoxCull | SurfCull | BoxPlane | SurfBBox | VIFqw | VUvert | MD2Vert | MD2Corner |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 563 | 326 | 3635 | 69 | 21 | 49 | 572 | 238 | 2353 | 10905 | 949 | 5655 |
| Base1 outdoor | 650 | 313 | 5629 | 66 | 35 | 101 | 1038 | 314 | 2760 | 16887 | 1404 | 7935 |
| Base1 heavy | 627 | 524 | 8343 | 91 | 36 | 80 | 836 | 386 | 3949 | 25029 | 1655 | 9438 |

World/Ent/3D times are 23591/7874/31725, 19700/13556/33530 and
34096/20121/54507 us. LitBuild is zero in all three captures. Static views show
no obvious new texture/geometry corruption, but do not prove motion, glass,
water or campaign-wide correctness. Camera/weapon/entity differences mean the
samples are not deterministic benchmarks.

Alpha.67 uploads were 22/21/34, E/R/S 22/22/15, 21/21/7, 34/34/22,
TexDMA 270/335/486 us, TexUp 20/21/34 and VRAMsync 13/7/20.
Alpha.68 uploads were 41/41/61, E/R/S 40/41/4, 41/41/4, 61/61/24,
TexDMA 609/659/859 us and FPS 20/20/15. Alpha.69's world-only plan therefore
failed to recover Alpha.67's upload baseline. The latest-upload labels include
HUD icons and conchars in all three views; these are absent from the world
plan but used later, motivating bounded retention rather than more exact LRU.

## Current policy: bounded small-Pic retention

Alpha.70 prefers a recently used small-Pic subset before applying Alpha.69's
world plan and serial fallback. Each image is at most 16 KB, touched this or
the previous frame, with a total cap of 256 KB or one quarter of the heap.
The subset is selected in address order on each victim scan. This is soft
retention, not a separate permanent arena: large demand allocations can evict
it, while prefetch still cannot recycle anything touched in the current frame.
The built-in font and particle dot are Pics; weapons/common world retention
remain unimplemented. No GS/PATH barriers or texture formats are changed.

Host tests compile production vram.cpp with minimal SDK/texture stubs and
exercise retention, expiration, size/budget limits, prefetch failure/success
and full-heap fallback under ASan/UBSan in CI. They do not validate GS DMA,
rendering or real frame time. Alpha.70 screenshot results are recorded above.

## Alpha.71 diagnostic reference

Alpha.71 retains Alpha.70 policy and adds PROFILE-only upload accounting.
In GAME -> TEST MAP -> DIAGNOSTICS: FULL, the right-side UP TYPE panel shows
N/R/KB: uploaded images, images reloaded after eviction, and packed pixel KiB
rounded up per type. KB excludes DMA commands and VRAM padding. Pic includes
HUD/font/particle images; Skin is model/weapon skins; Wall includes liquid and
brush WALs; Sprite and Sky have separate rows. Other is the unused Null class.

PHASE W/E/A/P/2D counts uploads during world (including sky and prefetch), all
entities (including weapons/brushes/translucent entities), world alpha surfaces,
particles, and other/2D respectively. Type and phase totals should each equal
the lower-left Uploads count because both panels use the same snapshot. The
snapshot precedes drawing those panels, so any resulting diagnostic-font miss
is excluded from both. All counters reset at BeginFrame; transfers counted are
images, not TexUp DMA batches. Tests cover payload/reload/phase/reset semantics.

Repeat the same Base1 positions with the same weapon, camera and settled scene.
Record the new panel together with Uploads/E/R/S, TexUp/TexDMA/VRAMwait/VRAMsync,
FPS and geometry. Use the dominant type/phase to choose further P4 work:
weapon/skin retention, expansion of the known-use plan or fragmentation work.
The supplied Alpha.71 measurements above motivate sky clipping. Check panel readability
in NTSC/PAL and the usual HUD/weapon/particle/sky/water/glass regressions.
Alpha.71 screenshots confirm readable accounting; instrumentation claims no speedup.

E counts each evicted block, R counts uploads restoring previously evicted
images, and S counts demand victims already touched this frame. Initial/dirty
uploads are not reloads. Multiple evictions can make E exceed Uploads; all
counters reset at BeginFrame. Lower S alone is not a performance improvement.

## Build and release procedure

The GitHub Actions `build` workflow is the reproducible toolchain. During this
optimization cycle it builds only `quake2-profile.elf` and uploads it in the
`quake2-ps2-build` artifact.

Local commands when PS2DEV/PS2SDK are configured:

```sh
make BUILD=profile
```

For every numbered code release:

1. Update source plus `VERSION`, README badge, `CHANGELOG` and `ROADMAP.md`.
2. Stage only those exact files and commit in the development worktree.
3. Verify fork `main` has not advanced independently, then fast-forward it
   from the worktree branch. Never force-push or reset the user's main checkout.
4. Wait for the GitHub Actions `build` run to pass.
5. Download its artifact into a new `build/ci-<short-commit>/` directory.
6. Compute and record the PROFILE ELF SHA-256.
7. Copy `quake2-profile.elf` to the repository root.
8. Publish `v<version>` as a GitHub prerelease with only the PROFILE ELF asset
   unless the user requests another package.

Never publish copyrighted `baseq2` data, soundtrack files or the user's local
game directories.

## Suggested prompt for the new task

> Continue the Quake II PS2 port in this workspace. Read HANDOFF.md completely,
> then ROADMAP.md and CHANGELOG. Check git status and recent commits. Review the
> Alpha.72 Base1 PROFILE results and renderer screenshots. Compare lower-left
> Uploads and E/R/S with Alpha.67/71, together with
> TexUp/TexDMA/VRAMwait/VRAMsync,
> then choose the next P4 residency step without weakening PATH1/PATH3 ordering.
> Build and publish only the numbered PROFILE prerelease, copy the successful
> quake2-profile.elf to the project root, and do not touch untracked game data.
