#define _POSIX_C_SOURCE 200809L // clock_gettime with -std=c99
// SPDX-License-Identifier: GPL-2.0-or-later
// m52xq: full-resolution photos of plasma-camera, see rawphoto.h.

#include "rawphoto.h"
#include "photo_gpu.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

static __thread EGLDisplay g_dpy = EGL_NO_DISPLAY;
static __thread EGLContext g_ctx = EGL_NO_CONTEXT;

static bool
ensure_context(void)
{
	if (g_ctx != EGL_NO_CONTEXT)
		return eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_ctx) == EGL_TRUE;

	if (epoxy_has_egl_extension(EGL_NO_DISPLAY, "EGL_MESA_platform_surfaceless"))
		g_dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	if (g_dpy == EGL_NO_DISPLAY)
		g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (g_dpy == EGL_NO_DISPLAY || !eglInitialize(g_dpy, NULL, NULL)) {
		fprintf(stderr, "m52xq rawphoto: no EGL display\n");
		return false;
	}
	eglBindAPI(EGL_OPENGL_ES_API);

	EGLConfig cfg = (EGLConfig)0; // EGL_NO_CONFIG_KHR
	if (!epoxy_has_egl_extension(g_dpy, "EGL_KHR_no_config_context")) {
		const EGLint attrs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE };
		EGLint n = 0;
		if (!eglChooseConfig(g_dpy, attrs, &cfg, 1, &n) || n < 1) {
			fprintf(stderr, "m52xq rawphoto: no EGL config\n");
			return false;
		}
	}
	const EGLint ctx_attrs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE };
	g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (g_ctx == EGL_NO_CONTEXT) {
		fprintf(stderr, "m52xq rawphoto: cannot create a GLES 3 context (0x%x)\n", eglGetError());
		return false;
	}
	if (eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_ctx) != EGL_TRUE) {
		fprintf(stderr, "m52xq rawphoto: eglMakeCurrent failed (0x%x)\n", eglGetError());
		return false;
	}
	printf("m52xq rawphoto: GL %s, %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
	return true;
}

static inline int
px10(const uint8_t *row, int x)
{
	const uint8_t *g = row + (x >> 2) * 5;
	int i = x & 3;
	return ((int)g[i] << 2) | ((g[4] >> (2 * i)) & 3);
}

// --- copy of remosaic_tetracell_raw10() of the m52xq Megapixels build (r64 + r68 position gains) ---
// 4-cell -> Bayer by moving pixels within each 4x4 block. Target (GRBG) <- source position in the block:
static const uint8_t remosaic_src[4][4][2] = {
	{ { 0, 0 }, { 0, 2 }, { 0, 1 }, { 0, 3 } }, // G R G R
	{ { 2, 0 }, { 1, 1 }, { 2, 1 }, { 2, 3 } }, // B G B G
	{ { 1, 0 }, { 1, 2 }, { 2, 2 }, { 1, 3 } }, // G R G R
	{ { 3, 0 }, { 3, 2 }, { 3, 1 }, { 3, 3 } }, // B G B G
};

static void
remosaic_tetracell_raw10(const uint8_t *src, uint8_t *dst, int width, int height, size_t line)
{
	memcpy(dst, src, line * height);

	static const int pos_color[4][4] = { { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 2, 2, 1, 1 }, { 2, 2, 1, 1 } }; // 0 R 1 G 2 B
	double sum[4][4] = { { 0 } }, cnt[4][4] = { { 0 } };
	for (int y = 0; y + 4 <= height; y += 32) {
		for (int gx = 0; gx < width / 4; gx += 8) {
			for (int r = 0; r < 4; r++) {
				const uint8_t *p = src + (size_t)(y + r) * line + (size_t)gx * 5;
				for (int c = 0; c < 4; c++) {
					int v = ((int)p[c] << 2) | ((p[4] >> (2 * c)) & 3);
					if (v > 80 && v < 960) {
						sum[r][c] += v - 64;
						cnt[r][c] += 1;
					}
				}
			}
		}
	}
	double csum[3] = { 0 }, ccnt[3] = { 0 };
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++) {
			csum[pos_color[r][c]] += sum[r][c];
			ccnt[pos_color[r][c]] += cnt[r][c];
		}
	float gain[4][4];
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++) {
			int k = pos_color[r][c];
			double m = cnt[r][c] > 100 ? sum[r][c] / cnt[r][c] : 0;
			double cm = ccnt[k] > 100 ? csum[k] / ccnt[k] : 0;
			gain[r][c] = (m > 1 && cm > 1) ? (float)(cm / m) : 1.0f;
			if (gain[r][c] < 0.8f || gain[r][c] > 1.25f)
				gain[r][c] = 1.0f;
		}

	for (int y = 0; y < height; y += 4) {
		for (int gx = 0; gx < width / 4; gx++) {
			uint16_t s[4][4];
			for (int r = 0; r < 4; r++) {
				const uint8_t *p = src + (size_t)(y + r) * line + (size_t)gx * 5;
				for (int c = 0; c < 4; c++) {
					int v = ((int)p[c] << 2) | ((p[4] >> (2 * c)) & 3);
					if (v > 64 && v < 1023) {
						v = 64 + (int)((v - 64) * gain[r][c] + 0.5f);
						v = v > 1023 ? 1023 : v;
					}
					s[r][c] = (uint16_t)v;
				}
			}
			for (int r = 0; r < 4; r++) {
				uint8_t *q = dst + (size_t)(y + r) * line + (size_t)gx * 5;
				uint8_t low = 0;
				for (int c = 0; c < 4; c++) {
					uint16_t v = s[remosaic_src[r][c][0]][remosaic_src[r][c][1]];
					q[c] = v >> 2;
					low |= (v & 3) << (2 * c);
				}
				q[4] = low;
			}
		}
	}
}

// mean linear level (black 64, white 1023) of each colour of the Bayer frame, saturated pixels left out
static void
bayer_means(const uint8_t *raw, int width, int height, int stride, const char *cfa, double out[3])
{
	double sum[3] = { 0 }, cnt[3] = { 0 };
	for (int y = 0; y + 1 < height; y += 16) {
		for (int dy = 0; dy < 2; dy++) {
			const uint8_t *row = raw + (size_t)(y + dy) * stride;
			for (int x = 0; x + 1 < width; x += 16) {
				for (int dx = 0; dx < 2; dx++) {
					int v = px10(row, x + dx);
					if (v >= 1000)
						continue;
					char c = cfa[dy * 2 + dx];
					int k = c == 'R' ? 0 : (c == 'B' ? 2 : 1);
					sum[k] += (v - 64 > 0 ? v - 64 : 0) / 959.0;
					cnt[k] += 1;
				}
			}
		}
	}
	for (int k = 0; k < 3; k++)
		out[k] = cnt[k] > 0 ? sum[k] / cnt[k] : 0.0;
}

static double
now_ms_rp(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

bool
m52xq_raw_develop(const uint8_t *raw, int width, int height, int stride, const char *cfa, bool tetracell,
                  int exif_orientation, float dark, bool front, const char *jpeg_path)
{
	double t0 = now_ms_rp();
	if (!raw || width <= 0 || height <= 0 || !cfa || strlen(cfa) < 4 || !ensure_context())
		return false;

	const uint8_t *frame = raw;
	uint8_t *rm = NULL;
	if (tetracell) {
		rm = malloc((size_t)stride * height);
		if (!rm)
			return false;
		remosaic_tetracell_raw10(raw, rm, width, height, (size_t)stride);
		frame = rm;
	}
	double t1 = now_ms_rp();

	// gray world white balance and an exposure gain to a mid grey of 0.16 (linear), both measured on the frame
	double m[3];
	bayer_means(frame, width, height, stride, cfa, m);
	float rg = m[0] > 1e-4 ? (float)(m[1] / m[0]) : 1.0f;
	float bg = m[2] > 1e-4 ? (float)(m[1] / m[2]) : 1.0f;
	rg = rg < 0.5f ? 0.5f : (rg > 4.0f ? 4.0f : rg);
	bg = bg < 0.5f ? 0.5f : (bg > 4.0f ? 4.0f : bg);
	float gain = m[1] > 1e-4 ? (float)(0.16 / m[1]) : 1.0f;
	gain = gain < 0.6f ? 0.6f : (gain > 12.0f ? 12.0f : gain);
	const float cm[9] = { rg * gain, 0, 0, 0, gain, 0, 0, 0, bg * gain };

	if (dark < 0.0f)
		dark = 0.0f;
	if (dark > 1.0f)
		dark = 1.0f;
	mp_photo_gpu_set_mid_nr(front ? dark : 0.6f * dark);
	mp_photo_gpu_set_wide_nr(front ? 1.0f : (dark >= 0.5f ? dark : 0.0f));
	const float nr = front ? 1.0f : 0.35f;

	bool ok = mp_photo_gpu_develop(frame, width, height, stride, cfa, exif_orientation, cm, 64.0f / 1023.0f, 1.0f / 2.2f,
				       jpeg_path, 92, nr);
	free(rm);
	printf("m52xq rawphoto: %dx%d %s means R %.3f G %.3f B %.3f -> wb %.2f/%.2f gain %.2f, dark %.2f, remosaic %.0f ms, "
	       "total %.0f ms: %s\n",
	       width, height, cfa, m[0], m[1], m[2], rg, bg, gain, dark, t1 - t0, now_ms_rp() - t0, ok ? jpeg_path : "FAILED");
	fflush(stdout);
	return ok;
}
