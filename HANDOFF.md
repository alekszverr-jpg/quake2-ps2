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
- Current version: `0.1.0-alpha.63`
- Current code commit before this handoff: `c68fe23`
  (`Batch deferred VU1 draw submissions`)
- Current published release:
  `https://github.com/alekszverr-jpg/quake2-ps2/releases/tag/v0.1.0-alpha.63`
- Alpha.63 PROFILE ELF SHA-256:
  `5980B3C35F5A477F45B1BE89BD0690F439E032F978F6FFCCE8A5A15B737475AE`

The next code release should normally be `0.1.0-alpha.64`. This handoff-only
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

## Latest PROFILE result: Alpha.63 awaiting validation

Alpha.63 replaces the synchronous wait after every `DrawTriangles` call with
deferred pass-level chains. Vertices are copied into a 128-byte-aligned static
3072-vertex staging area and per-draw constants are stored inline. An in-chain
VIF `FLUSH` protects the fixed constants addresses between draws, while public
ordering flushes drain PATH1 before texture uploads, VRAM reuse, 2D overlays
and frame boundaries.

The latest validated comparison remains Alpha.62:

| Scene | FPS | VIFchain | VUchunk | VIFqw | VUvert | VUstate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base1 light | 26 | 70 | 175 | 1911 | 10938 | 93 |
| Base1 outdoor | 20 | 74 | 249 | 2500 | 17493 | 112 |
| Base1 heavy | 15 | 89 | 324 | 3107 | 23079 | 128 |

Alpha.63 CI passed for `c68fe23`, but no PCSX2 or retail-PS2 screenshots have
been supplied yet. Its immediate success criterion is `VIFchain < Batches`
with comparable `VUvert` and triangle counts and no visual corruption.

## Exact next task: validate P1 larger VIF submissions

Test the published Alpha.63 PROFILE ELF in the three recorded Base1 positions.
Record FPS, `Batches`, `VIFchain`, `VUchunk`, `VIFqw`, `VUvert` and `VUstate`,
and inspect all renderer regression paths listed above.

The relevant files are:

- `src/ps2/renderer/vu1.cpp`
- `src/ps2/renderer/vu1.h`
- `src/ps2/renderer/render_view.cpp`
- `src/ps2/renderer/gs.cpp`
- `src/ps2/renderer/vif_packet.*`

Alpha.63 behaviour:

1. `vu1::DrawTriangles` flushes pending 2D.
2. It calls `gs::EnsureTextureResident`.
3. It copies caller vertices to stable aligned staging and appends constants
   plus chunks to the pending chain.
4. It submits/waits only when staging or packet space fills or an explicit
   texture/PATH/frame ordering boundary calls `vu1::Flush()`.
5. Each new draw/chain segment seeds both XTOP halves before compact chunks.

Expected Alpha.63 validation result:

- `VIFchain` should become lower than `Batches` in the three Base1 positions.
- `VUvert` and rendered triangle counts should remain comparable to Alpha.62.
- `VUstate` may remain similar; the first goal is chain merging.
- No texture, transparency, sky, particle, weapon or moving-brush regressions.
- FPS may improve only slightly because the captures are primarily EE-bound.

If Alpha.63 validates, the next unchecked P1 implementation item is two or
three EE-side packet/staging buffers so EE preparation can overlap an in-flight
VU1/GS chain. Do not begin that asynchronous lifetime expansion until the
single-buffer ordering model is visually proven.

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
> Alpha.63 Base1 PROFILE results and renderer screenshots. If pass-level VIF
> batching validates, continue the next P1 packet-buffer overlap item without
> weakening PATH1/PATH3 texture ordering. Build and publish only the numbered
> PROFILE prerelease, copy the successful quake2-profile.elf to the project
> root, and do not touch untracked game data.
