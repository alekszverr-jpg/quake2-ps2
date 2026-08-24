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
- Current version: `0.1.0-alpha.66`
- Current code commit before this handoff: `c1b27f1`
  (`Batch visible world texture uploads`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.66`
- Alpha.66 PROFILE ELF SHA-256:
  `AD6FF21F306F69FC5104BD777F48B51DBAD6751D40B738BB16AF8ABE40BA1850`

The next code release should normally be `0.1.0-alpha.67`. This handoff-only
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

## Latest PROFILE result: Alpha.66 awaiting validation

Alpha.66 builds a bounded draw-order working-set prefix from the opaque BSP
texture chains. Up to 32 missing clean textures are allocated without evicting
anything already prepared this frame, then all selected mip levels share one
PATH3 source chain and one GS `FINISH`. Dirty images, packet overflow and sets
larger than available VRAM remain on the proven synchronous per-bind path.

Alpha.65's supplied PCSX2 results rejected the extra entity sort:

| Scene | FPS | EntTex0/EntTex | TexDMA us | TexUp | Uploads | VRAMwait us | VRAMsync | VIFchain | VUWait us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 24 | 3/3 | 721 | 21 | 21 | 324 | 14 | 17 | 83 |
| Base1 outdoor | 20 | 11/11 | 335 | 21 | 21 | 180 | 7 | 12 | 150 |
| Base1 heavy | 15 | 12/12 | 529 | 37 | 37 | 532 | 20 | 29 | 183 |

The screenshots showed correct geometry, textures, sky, entities and weapons,
but `EntTex0 == EntTex` in every scene. Quake II's existing client order was
already sufficient, so Alpha.66 removes the second sort/copy and its counters.
Alpha.66 CI passed for `c1b27f1`; PCSX2 and retail-PS2 validation are pending.

## Exact next task: validate bounded world-texture prefetch

Test the published Alpha.66 PROFILE ELF in the same Base1 positions. Record the
lower-left `Uploads`, draw-stats `TexUp`, `TexDMA`, `VRAMwait`, `VRAMsync`, FPS
and the established VIF/geometry counters. Inspect WALs, sky, glass/water,
weapons, entities and all renderer regression paths listed above.

The relevant files are:

- `src/ps2/renderer/render_view.cpp`
- `src/ps2/renderer/gs.cpp`
- `src/ps2/renderer/gs.h`
- `src/ps2/renderer/vram.cpp`
- `src/ps2/renderer/vram.h`

Alpha.66 behaviour:

1. The BSP walk builds its normal opaque texture chains in draw order.
2. Resident textures and successful allocations pin only the prepared prefix.
3. Prefetch allocation may evict only textures not touched in this frame and
   stops before recycling any prepared/current-frame allocation.
4. Deferred PATH1 geometry is drained, selected mip transfers share one PATH3
   chain, and one final `FINISH` completes the whole batch.
5. Any remaining or dirty texture uploads through `EnsureTextureResident` as
   before; entity and translucent draw order is the original ref_gl order.

Expected Alpha.66 validation result:

- Lower-left `Uploads` remains the number of texture images transferred, while
  `TexUp` is now the number of synchronous DMA batches. Useful batching should
  produce `TexUp < Uploads` in streaming-heavy frames.
- `TexDMA` and especially `VRAMsync` should fall or at least not regress; world,
  triangle and VIF counts should remain comparable to Alpha.65.
- No black/foreign strips, stale pages, sky leaks, glass/water corruption,
  missing entities or weapon/sprite regressions.

If Alpha.66 validates and reduces `TexUp`/synchronisation, retain the bounded
prefix and consider extending the same batch primitive only to another measured,
order-safe pass. If `TexUp` remains equal to `Uploads` or uploads/visuals regress,
revert the prefetch experiment and continue with P4 residency/fragmentation
measurement rather than weakening PATH1/PATH3 ordering.

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
> Alpha.66 Base1 PROFILE results and renderer screenshots. Compare lower-left
> Uploads with TexUp plus TexDMA/VRAMwait/VRAMsync, then retain or revert the
> bounded P2 world-prefetch experiment without weakening PATH1/PATH3 ordering.
> Build and publish only the numbered PROFILE prerelease, copy the successful
> quake2-profile.elf to the project root, and do not touch untracked game data.
