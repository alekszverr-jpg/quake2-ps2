# Development handoff

This file is the durable context for continuing development in a new Codex
task. Read it together with [ROADMAP.md](ROADMAP.md) and [CHANGELOG](CHANGELOG)
before changing renderer, audio or memory-management code.

## Repository checkpoint

- Workspace: `C:\Users\user\Documents\quake2-ps2`
- Active Git branch: `main`
- Fork used for pushes and releases:
  `https://github.com/alekszverr-jpg/quake2-ps2.git`
- Read-only upstream reference:
  `https://github.com/glampert/quake2-ps2.git`
- Current version: `0.1.0-alpha.68`
- Current code commit before this handoff: `cc682d6`
  (`Use exact VRAM texture LRU order`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.68`
- Alpha.68 PROFILE ELF SHA-256:
  `59879CEECA5410752933F25A9BDC314AD5FA7E18BDBE0FEEF6F23D19ACB9E624`

The next code release should normally be `0.1.0-alpha.69`. This handoff-only
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

## Latest PROFILE result: Alpha.68 awaiting validation

Alpha.67's supplied PCSX2 screenshots proved that the visible texture workload
is dominated by VRAM replacement rather than first-time uploads. Every upload
restored an evicted texture, and many victims had already been touched in the
same frame:

| Scene | FPS | Uploads | E/R/S | TexUp | TexDMA us | VRAMwait us | VRAMsync | VIFchain | VUWait us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 22 | 22 | 22/22/15 | 20 | 270 | 295 | 13 | 16 | 84 |
| Base1 outdoor | 20 | 21 | 21/21/7 | 21 | 335 | 184 | 7 | 12 | 155 |
| Base1 heavy | 15 | 34 | 34/34/22 | 34 | 486 | 484 | 20 | 29 | 149 |

The screenshots showed correct geometry, textures, sky, entities and weapons.
Inspection then found that `lastBoundFrame` was serving both as the safety stamp
and as LRU recency. All binds in one frame therefore tied, so the allocator fell
back to VRAM block order instead of choosing the least-recently-bound texture.

Alpha.68 retains the frame stamp for current-frame safety and prefetch pins, but
adds a monotonic 64-bit bind serial for exact victim ordering. New allocations
and every texture touch advance the serial. CI passed for `cc682d6`; runtime
validation is pending.

## Exact next task: validate exact bind-serial LRU

Test the published Alpha.68 PROFILE ELF in the same three Base1 positions.
Record lower-left `Uploads` and `E/R/S`, plus `TexUp`, `TexDMA`, `VRAMwait`,
`VRAMsync`, FPS and the established geometry counters. Compare directly with
the Alpha.67 table above. Inspect WALs, sky, glass/water, weapons, entities and
all renderer regression paths listed above.

The relevant files are:

- `src/ps2/renderer/texture.h`
- `src/ps2/renderer/texture.cpp`
- `src/ps2/renderer/gs.cpp`
- `src/ps2/renderer/vram.cpp`
- `src/ps2/renderer/vram.h`
- `src/ps2/renderer/ref.cpp`

Counter interpretation remains:

1. `E` increments for each resident VRAM block evicted, including multiple
   victims needed to form one larger allocation.
2. `R` increments when a later upload restores a texture marked by eviction;
   initial uploads, explicit releases and dirty in-place updates do not count.
3. `S` increments when ordinary on-demand allocation evicts a texture already
   touched this frame; bounded prefetch allocation refuses such a victim.
4. All three counters reset with `Uploads` at `BeginFrame` and do not alter the
   release renderer's allocation policy.

Expected Alpha.68 result: lower `Uploads`, `R` and especially `S`, with lower
texture DMA/wait cost and no visual regression. If churn remains essentially
unchanged, the working set genuinely exceeds usable VRAM or pass ordering
reuses textures too late; use the measured counters to choose the next P4 step.
Preserve Alpha.66 PATH1/PATH3 ordering and bounded fallback during validation.

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
2. Stage only those exact files and commit on `main`.
3. Push to remote `fork`.
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
> Alpha.68 Base1 PROFILE results and renderer screenshots. Compare lower-left
> Uploads and E/R/S with Alpha.67, together with TexUp/TexDMA/VRAMwait/VRAMsync,
> then choose the next P4 residency step without weakening PATH1/PATH3 ordering.
> Build and publish only the numbered PROFILE prerelease, copy the successful
> quake2-profile.elf to the project root, and do not touch untracked game data.
