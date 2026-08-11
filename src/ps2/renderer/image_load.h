#pragma once
/* ================================================================================================
 * File: image_load.h
 * Brief: Loaders for the Quake II on-disk image formats (PCX/WAL/TGA), decoding
 *        into pixel buffers the GS texture pipeline consumes directly. The 8-bit
 *        formats stay as palette indices sampled through the shared global-palette
 *        CLUT (PixelFormat::Palette8); TGA expands to RGBA32.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include <tamtypes.h>

namespace ps2::img {

// On success every loader hands back a pixel buffer allocated with
// PS2_MemAllocAligned(16, ..., MEMTAG_TEXIMAGE) - DMA-ready - that the caller
// owns and frees with PS2_MemFree (the loader-provided byte size when the
// format carries a mip chain, otherwise width * height * bytes per texel).
// On failure they warn via Com_DPrintf and return false with nothing allocated.

// PCX: 8-bit palette indices, 1 byte/texel. The palette embedded in the file is
// ignored - all Quake II art indexes the shared global palette, the same way
// ref_gl decoded through d_8to24table.
bool LoadPcx(const char * filename, u8 ** outPic, int * outWidth, int * outHeight);

// WAL: all four precomputed mip levels as packed 8-bit palette indices,
// level 0 first. The GS can only describe power-of-two texture dimensions, so
// NPOT source levels are resampled into a power-of-two storage chain while the
// original width/height are returned separately for world-space UV scaling.
// outPixelBytes covers the complete storage allocation.
bool LoadWal(const char * filename, u8 ** outPic, int * outWidth, int * outHeight,
             int * outStorageWidth, int * outStorageHeight,
             int * outMipLevels, int * outPixelBytes);

// TGA (types 2 and 10, 24/32 bpp, no colormaps - all Quake II ever shipped):
// RGBA32 texels, 4 bytes/texel. *outHasAlpha is set when the file carried an
// alpha channel (32 bpp source); 24 bpp texels get alpha 255.
bool LoadTga(const char * filename, u8 ** outPic, int * outWidth, int * outHeight, bool * outHasAlpha);

} // namespace ps2::img
