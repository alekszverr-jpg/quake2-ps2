
# Quake II port for the PlayStation 2

Development has resumed. See the [roadmap](ROADMAP.md) for current priorities
and the [changelog](CHANGELOG) for completed work.

![Raw level geometry](https://raw.githubusercontent.com/glampert/quake2-for-ps2/master/misc/screens/q2ps2-level-notex-2.png "Raw level geometry")

## Overview

This is an unofficial fan made port, targeting the PS2 Console, of the original
[Quake II source code released by id Software][link_id_repo].

This port relies on the free [PS2DEV SDK][link_ps2_dev] to provide rendering,
input, audio and system services for the Quake Engine.

The project is in active development. Menus, cinematics, gamepad input,
textured BSP levels and animated MD2 models are running on PCSX2 and are being
validated on real PS2 hardware.

The long term goal would be to have a fully functional and playable (single-player)
Quake II on the PlayStation 2, using only on the freely available tools and libraries.

The main features still missing are:

- Add lightmaps, model lighting and dynamic lights
- Complete sprite, brush-entity, sky, water and transparency rendering
- Add mipmaps and stable texture minification
- Add sound rendering/mixing for the PS2
- Add save/load and persistent configuration for PS2 storage
- Optimize memory allocation/usage as much as possible
- Optimize rendering to ensure smooth 30fps gameplay

## License

Quake II was originally released as GPL, and it remains as such. New code written
for the PS2 port or any changes made to the original source code are also released under the
GNU General Public License version 2. See the accompanying LICENSE file for the details.

You can also find a copy of the GPL version 2 [in here][link_gpl_v2].

[link_id_repo]: https://github.com/id-Software/Quake-2
[link_ps2_dev]: https://github.com/ps2dev
[link_gpl_v2]:  https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html

