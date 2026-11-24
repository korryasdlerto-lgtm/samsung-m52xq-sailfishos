#pragma once

#include <stdbool.h>
#include <stdint.h>

/* m52xq r67: develop a packed RAW10 Bayer frame on the GPU (full-resolution demosaic + the viewfinder colour processing,
 * EXIF orientation 1..8 applied) straight into a JPEG. Needs the GL context of the process pipeline to be current. */
bool mp_photo_gpu_develop(const uint8_t *raw, int width, int height, int line_bytes, const char *cfa, int orientation,
                          const float color_matrix[9], float blacklevel, float inv_gamma, const char *jpeg_path, int quality,
                          float nr);
void mp_photo_gpu_set_wide_nr(float k); // m52xq r83: 0..1, wide colour pass of the next photo
void mp_photo_gpu_set_mid_nr(float k); // m52xq r82: 0..1, mid-scale brightness noise reduction of the next photo
