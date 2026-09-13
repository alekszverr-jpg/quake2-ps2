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
- Current version: `0.1.0-alpha.70`
- Current code commit before this handoff: `adf2ffd`
  (`Retain bounded small HUD textures across world scans`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.70`
- Alpha.70 PROFILE ELF SHA-256 (7,504,756 bytes):
  `5A54B32A315BFF1FC7D5370F6D1271B7A64B2295BA95ACFC17DB82CC16F3DC52`
- CI `34740150864` passed host allocator ASan/UBSan tests and the PROFILE build.
  Both root ELF copies above match the downloaded CI artifact. The release
  contains only `quake2-profile.elf`.

The next code release should normally be `0.1.0-alpha.71`. This handoff-only
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

## Latest PROFILE result: Alpha.69 rejected for performance

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

## Exact next task: validate bounded small-Pic retention

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
rendering or real frame time. Alpha.70 runtime validation remains pending.

Test the Alpha.70 PROFILE ELF in the same three Base1 views. Compare Uploads,
E/R/S, TexUp, TexDMA, VRAMwait, VRAMsync, FPS and geometry with both Alpha.67
and Alpha.69 above. Check HUD/weapon changes, particles, large menus, sky,
water/glass, moving brush models and the regression list above. A lower HUD
reload rate is useful only if total uploads/waits improve without corruption.
If total churn remains near Alpha.69, measure phase/type upload distribution
before expanding retention to weapon/world textures or extending the use plan.

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
> Alpha.70 Base1 PROFILE results and renderer screenshots. Compare lower-left
> Uploads and E/R/S with Alpha.67/69, together with
> TexUp/TexDMA/VRAMwait/VRAMsync,
> then choose the next P4 residency step without weakening PATH1/PATH3 ordering.
> Build and publish only the numbered PROFILE prerelease, copy the successful
> quake2-profile.elf to the project root, and do not touch untracked game data.
