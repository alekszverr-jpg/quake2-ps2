# Development handoff

This file is the durable context for continuing development in a new Codex
task. Read it together with [ROADMAP.md](ROADMAP.md) and [CHANGELOG](CHANGELOG)
before changing renderer, audio or memory-management code.

## Repository checkpoint

- Development worktree: `C:\Users\user\.codex\worktrees\cb03\quake2-ps2`
- Local testing project: `C:\Users\user\Documents\quake2-ps2`
- Worktree branch: `codex/alpha70-small-pic-retention`; published to fork `main`
- Fork used for pushes and releases:
  `https://github.com/alekszverr-jpg/quake2-ps2.git`
- Read-only upstream reference:
  `https://github.com/glampert/quake2-ps2.git`
- Current version: `0.1.0-alpha.72`
- Current code commit before this handoff: `978fa46`
  (`Clip sky faces to the visible GS viewport`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.72`
- Alpha.72 PROFILE ELF SHA-256 (7,513,024 bytes):
  `B7C8ABB549B1F533C9392DC05E21CF944A1F65BFFCD9494267533A5BDB121549`
- CI `34762425594` passed host allocator ASan/UBSan tests and the PROFILE build.
  Both root ELF copies above match the downloaded CI artifact. The release
  contains only `quake2-profile.elf`.

The next code release should normally be `0.1.0-alpha.73`. This handoff-only
checkpoint does not advance `VERSION`.

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
