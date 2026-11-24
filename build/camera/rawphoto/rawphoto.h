// SPDX-License-Identifier: GPL-2.0-or-later
// m52xq: full-resolution photos of plasma-camera (main camera 9248x6936, front camera 6528x4896) from a raw RAW10 frame,
// developed on the GPU like the m52xq Megapixels build does (own copy of its code, see photo_gpu.c).

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * raw: packed RAW10 (MIPI CSI-2), stride bytes per line; cfa: "GRBG", "RGGB", ...; tetracell: the frame is the 4-cell array
 * of the main camera (re-mosaiced to Bayer first); exif_orientation 1..8 is stored in the JPEG; dark 0..1 controls the noise
 * reduction; front: front camera (stronger noise reduction). The white balance and the brightness are measured on the
 * frame itself. Creates (once per thread) a surfaceless EGL / GLES 3 context. Returns false on any error.
 */
bool m52xq_raw_develop(const uint8_t *raw, int width, int height, int stride, const char *cfa, bool tetracell,
                       int exif_orientation, float dark, bool front, const char *jpeg_path);

#ifdef __cplusplus
}
#endif
