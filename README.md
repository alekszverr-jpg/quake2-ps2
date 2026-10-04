# Quake II for PlayStation 2

[![Build](https://github.com/alekszverr-jpg/quake2-ps2/actions/workflows/build.yml/badge.svg)](https://github.com/alekszverr-jpg/quake2-ps2/actions/workflows/build.yml)
[![Version](https://img.shields.io/badge/version-v0.1.0--alpha.111-orange.svg)](https://github.com/alekszverr-jpg/quake2-ps2/releases)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE)

An active continuation of the unofficial Quake II port for the Sony
PlayStation 2. It is based on id Software's released Quake II source code and
the original PS2 port by [Guilherme Lampert](https://github.com/glampert).

The current test build boots on PCSX2 and real PS2 hardware, renders textured
BSP levels and animated MD2 models through a VU1-assisted pipeline, and supports
DualShock controls. It is not yet a complete port: audio has only its first
PCM effects backend, while lighting and several rendering paths remain under
development.

> This repository contains source code only. It does not include or distribute
> copyrighted Quake II game data. You must provide data files from your own
> copy of the game.

## Project status

| Area | Status |
| --- | --- |
| PCSX2 boot from `host:` | Working |
| Real PS2 boot from FAT32 USB | Working and hardware-tested |
| Menus, HUD and console | Working |
| DualShock input | Working |
| Textured BSP world | Working |
| Animated MD2 models and weapons | Working |
| NTSC `640x448` / PAL `640x512` | Implemented and hardware-tested |
| Cinematics | Implemented |
| Static BSP vertex lighting | Implemented and validated in PCSX2 |
| MD2 entity/weapon lighting | Implemented and validated in PCSX2 |
| Dynamic MD2 lighting | Implemented and validated in PCSX2 |
| Moving doors/platforms | Opaque brush pass working in PCSX2 |
| Weapon depth range | Working in PCSX2 and on real PS2 |
| Sprite entities | First pass, PCSX2 validation pending |
| Particles and sprite alpha | First pass working in PCSX2 |
| Transparent BSP glass | Working in PCSX2 and on real PS2 |
| Turbulent water/lava/slime | Working and validated in PCSX2 |
| On-screen renderer profiling | EE phases, VU wait and VRAM transfer/stall timing |
| Adaptive BSP lighting cache | Working; reduced measured world time by about 66-72% |
| Indexed MD2 preparation | Working; reduced measured entity time by about 24-44% |
| Full lightmaps and dynamic world lighting | Not implemented |
| Sound effects | First `audsrv`/SPU2 PCM backend, validation pending |
| WAL world-texture mipmaps | Implemented and hardware-tested |
| Save/load on PS2 storage | Not implemented |

See [ROADMAP.md](ROADMAP.md) for milestones and [CHANGELOG](CHANGELOG) for
completed work and known limitations.

## Download a test build

Open the [GitHub Actions build page](https://github.com/alekszverr-jpg/quake2-ps2/actions/workflows/build.yml),
select the latest successful run and download the `quake2-ps2-build` artifact.
The archive contains:

```text
quake2.elf
tools/
```

GitHub requires you to be signed in before downloading workflow artifacts.

## Game data

At minimum, copy `pak0.pak` from a full, legally owned Quake II installation:

```text
baseq2/
  pak0.pak
  pak1.pak       # optional official update data
  pak2.pak       # optional official update data
  video/         # optional loose cinematics
  players/       # optional loose player assets
```

Do not commit these files to the repository.

## Run on PCSX2

1. Extract `quake2.elf`.
2. Put the `baseq2` directory beside the ELF.
3. In PCSX2, enable **Settings → Emulation → Enable Host Filesystem**.
4. Start `quake2.elf`.

Example:

```text
quake2-test/
  quake2.elf
  baseq2/
    pak0.pak
```

## Run on a real PS2

Current hardware builds load game data from a FAT32 USB drive through `mass:`.

1. Format a USB drive as FAT32.
2. Put `baseq2` in the root of the drive.
3. Copy `quake2.elf` to the drive.
4. Start the ELF through uLaunchELF.

Expected layout:

```text
mass:/
  quake2.elf
  baseq2/
    pak0.pak
    pak1.pak
    pak2.pak
```

USB enumeration may take a few seconds. HDD, MX4SIO and memory-card game-data
paths are not supported yet.

## Default controls

| Control | Action |
| --- | --- |
| Left stick | Move / strafe |
| Right stick | Look |
| Cross | Jump / confirm |
| Circle | Crouch / back |
| Square | Use |
| Triangle | Help |
| R1 | Attack |
| L1 | Run |
| L2 / R2 | Previous / next weapon |
| L3 | Give all weapons, ammo and items (test helper) |
| D-pad | Inventory |
| R3 | Center view |
| Start | Menu |
| Select | Console |

Bindings will become configurable once persistent configuration is completed.

## Build from source

The port uses the open-source [PS2DEV toolchain](https://github.com/ps2dev).
The build also requires
[vclpp](https://github.com/glampert/vclpp) and
[OpenVCL](https://github.com/ps2dev/openvcl) for the VU1 microprogram.

With `PS2DEV` and `PS2SDK` configured:

```sh
make
```

This produces the normal gameplay build, with per-frame profiling counters and
diagnostic overlays compiled out:

```text
build/quake2.elf
```

For performance investigations, build the separate gamepad-controlled profile
variant:

```sh
make BUILD=profile
```

Its output is `build/quake2-profile.elf`. Release and profile objects are kept
in separate directories, so the two variants can be built back-to-back safely.

Host utilities can be built separately with:

```sh
make tools
```

The GitHub Actions workflow is the reference reproducible build environment.
During the current optimization cycle it builds and publishes only the
gamepad-controlled `quake2-profile.elf` test variant.

## Development

- [Development handoff](HANDOFF.md)
- [Roadmap](ROADMAP.md)
- [Changelog](CHANGELOG)
- [Builds](https://github.com/alekszverr-jpg/quake2-ps2/actions)
- [Issues and test reports](https://github.com/alekszverr-jpg/quake2-ps2/issues)

Useful reports include console model/region, launch method, storage device,
video mode, exact reproduction steps and a photo or emulator screenshot.

## License and credits

Quake II source code was released under the GNU General Public License. This
port and its modifications remain under the GNU GPL version 2; see [LICENSE](LICENSE).

- Quake II by id Software:
  [id-Software/Quake-2](https://github.com/id-Software/Quake-2)
- Original PlayStation 2 port by Guilherme Lampert:
  [glampert/quake2-ps2](https://github.com/glampert/quake2-ps2)
- PlayStation 2 open-source toolchain:
  [PS2DEV](https://github.com/ps2dev)

### VIDEO settings (Alpha.100)

VIDEO offers FOV, world brightness, world dynamic lights, world mipmapping,
screen colour effects and FPS. Left/Right changes the selected option; Back
saves changed settings to `baseq2/config.cfg`. Restore defaults resets VIDEO.

Output mode options are Auto (the previous NTSC/PAL behavior), 240p (640x224
active area), 480i (640x448 active area) and 480p (640x480). Save with Back and
fully restart the game to apply output changes. All other VIDEO controls are
live. In 240p, the HUD, menus, console and cinematics share the native logical
320x224 layout: font rows stay intact and horizontal coordinates scale to the
640-pixel framebuffer. The HUD has an eight-line bottom safe area.

For benchmark comparisons, use the same output mode, FOV and brightness.
User accepted the weapon/UI fixes and supported TV modes on Alpha.103.
480p remains unverified on real hardware; the test television does not support it.

### MD2 model profiling (Alpha.104)

In a PROFILE build, select GAME -> model profile (3 runs). After demo1 finishes,
press Left/Right twice to reach MD2 profile (incl view weapon). The table reports
per-frame averages for setup/culling, model lighting, animation plus vertex color,
triangle expansion/clipping and CPU submission, alongside model/geometry counts.
Submission time is excluded from triangle time, and does not measure asynchronous
VU/GS completion outside that call. Brush/sprite entities and HUD are excluded.

Diagnostic timers affect FPS; use the ordinary benchmark for performance comparisons.
World lighting/cache settings are preserved, all diagnostics restored after the run.

Alpha.105 reuses clipping data per unique vertex for weapons, shells and translucent
MD2 models. Fully visible triangles skip the full clipping payload; boundary
triangles retain the existing clipper. Compare the same VIDEO settings using the
ordinary benchmark and the model profile. Alpha.105 was visually accepted, with
Tris/clip reduced from3.78 to2.87ms; its ordinary benchmark measured39.21FPS.
A matching Alpha.104 ordinary result was not supplied, so total FPS gain is unquantified.

Alpha.106 also reuses indexed texture coordinates per MD2 draw, preserving UV
seams, shell constants and the scalar fallback for oversized coordinate tables.
Compare against Alpha.105 with identical VIDEO settings; new runtime results
and visual skin/weapon validation are pending.

Alpha.107 reuses exact shared BSP positions when preparing clip distances for
cached subdivisions. Its8KiB scratch cache is invalidated per polygon, including
moving brushes. User measurements: ordinary benchmark40.27FPS/24.83ms vs106's
39.27FPS/25.45ms; World11.56..11.59ms vs12.13..12.15ms. Explicit visual acceptance
of the BSP clipping change remains pending.

### World phase profiling (Alpha.108)

In a PROFILE build, select GAME -> world profile (3 runs), then press Right twice
to reach World profile (opaque pass + sky). BSP/PVS measures cluster setup, leaf
visibility and traversal; Sky measures sky preparation; Geometry includes opaque
geometry preparation, lighting, clipping, crack seals and texture prefetch.
Submit measures CPU time inside triangle submission, excluded from Sky/Geometry.
Deferred waits outside submission, entities, late translucent surfaces and HUD
are excluded. Nodes, surfaces, emitted triangles and batches are per-frame counts.

World light/cache settings are preserved; detailed light/MD2 timers are disabled
during the run and all diagnostics restored afterwards. Timers affect FPS: use
the ordinary benchmark for speed comparisons. The table fits the native240p UI.

Alpha.108 user measurements: BSP/PVS1.07ms, Sky0.30ms, Geometry9.25..9.27ms,
Submit1.01..1.04ms. Geometry is about79% of the measured World pass.

Alpha.109 adds a fourth page: press Right three times for World Geometry breakdown.
Prep/light includes chain setup, cached geometry preparation and dynamic lighting;
Clip/emit includes guard planes, clipping and scratch vertex emission. Tex fetch
measures the resident-prefix prefetch and planned texture use setup; per-bind work
remains in Submit. Seal prep measures seal corners, colour lookup and their plane
transforms; seal clipping/emission belongs to Clip/emit. All four categories exclude
nested categories and CPU Submit, including recursive lighting. They partition
Geometry with small scope-boundary overhead; use these diagnostic results to pick
an optimization, then measure actual FPS with the ordinary benchmark.

Alpha.109 diagnostic averages: Prep/light6.97ms, Clip/emit8.84ms, Tex fetch0.21ms,
Seal prep1.58ms. Frequent nested clocks add overhead; these cannot be compared
directly with Alpha.108's Geometry total. Alpha.110 rejects cached BSP triangles
sharing one outside plane before building full clipping records, when no dynamic
lights are selected. Active-light subdivision retains its existing path. Compare
the ordinary benchmark and World time with identical VIDEO settings; inspect
walls, doors and near-plane edges before accepting the optimization.

Alpha.110 ordinary benchmark40.40FPS/24.76ms, World11.60..11.62ms, versus
Alpha.10740.27FPS/24.83ms and World11.56..11.59ms: no clear World speed gain.
Alpha.111 adds BSP early reject coverage to the same World profile run. Press
Right four times to see Cached tris, No-light tris, Early rejects, Of all % and
Of unlit %. Counts are per-frame pre-clipping cached opaque World triangles;
sky, seals, brush entities and dynamically generated children are excluded.
No-light means no selected dynamic source for the source triangle, not disabled
world lighting. Ratios use aggregated counts, with zero for empty denominators.
