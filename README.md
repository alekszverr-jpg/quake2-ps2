# Quake II for PlayStation 2

[![Build](https://github.com/alekszverr-jpg/quake2-ps2/actions/workflows/build.yml/badge.svg)](https://github.com/alekszverr-jpg/quake2-ps2/actions/workflows/build.yml)
[![Version](https://img.shields.io/badge/version-v0.1.0--alpha.22-orange.svg)](https://github.com/alekszverr-jpg/quake2-ps2/releases)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE)

An active continuation of the unofficial Quake II port for the Sony
PlayStation 2. It is based on id Software's released Quake II source code and
the original PS2 port by [Guilherme Lampert](https://github.com/glampert).

The current test build boots on PCSX2 and real PS2 hardware, renders textured
BSP levels and animated MD2 models through a VU1-assisted pipeline, and supports
DualShock controls. It is not yet a complete port: lighting, sound and several
rendering paths are still under development.

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
| On-screen renderer profiling | EE phases, VU wait and VRAM transfer/stall timing |
| Adaptive BSP lighting cache | Working; reduced measured world time by about 66-72% |
| Indexed MD2 preparation | Working; reduced measured entity time by about 24-44% |
| Full lightmaps and dynamic world lighting | Not implemented |
| Sound | Not implemented |
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

The resulting ELF is written to:

```text
build/quake2.elf
```

Host utilities can be built separately with:

```sh
make tools
```

The GitHub Actions workflow is the reference reproducible build environment.

## Development

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
