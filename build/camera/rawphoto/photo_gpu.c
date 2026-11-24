#define _POSIX_C_SOURCE 200809L // clock_gettime with -std=c99
// m52xq: copy of photo_gpu.c of the m52xq Megapixels build (GPU development of a packed RAW10 frame into a JPEG);
// kept as a separate copy so that plasma-camera and Megapixels do not depend on each other.
// m52xq r67: develop a photo on the GPU.
//
// The stock path wrote the raw burst as DNG files, then dcraw_emu made a 16-bit TIFF and ImageMagick ran its filters on it,
// all on the CPU through files: 15 s for 16 MP, ~40 s (or an OOM kill) for 64 MP. The viewfinder already develops every
// frame on the GPU, but at half resolution (one output pixel per 2x2 Bayer quad). Here one fragment shader reads the packed
// RAW10 frame directly, demosaics it at full resolution (Malvar-He-Cutler 5x5, "High-quality linear interpolation for
// demosaicing of Bayer-patterned color images", ICASSP 2004), applies exactly the colour processing of the viewfinder
// (black level, white balance and gain in the colour matrix, gamma, S-curve, saturation) and the orientation the DNG would
// carry. The result is rendered in strips of rows, read back and handed to libjpeg-turbo line by line, so the memory use
// stays small even for 64 MP.
#include "photo_gpu.h"


#include <epoxy/gl.h>
#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRIP_ROWS 512

static const char *vertex_src =
        "#version 300 es\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID & 1) * 2 - 1), float((gl_VertexID >> 1) * 2 - 1));\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";

static const char *fragment_src =
        "#version 300 es\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "uniform highp sampler2D raw;\n"
        "uniform ivec2 src_size;\n"
        "uniform ivec2 red_pos;\n"     // position of the red pixel in the 2x2 pattern
        "uniform int orient;\n"        // EXIF orientation 1..8
        "uniform int strip_y0;\n"
        "uniform mat3 color_matrix;\n"
        "uniform float blacklevel;\n"
        "uniform float inv_gamma;\n"
        "out vec4 frag;\n"
        "float px(ivec2 p) {\n"
        "    p = clamp(p, ivec2(0), src_size - 1);\n"
        "    int g = p.x >> 2;\n"
        "    int i = p.x & 3;\n"
        "    uint hi = uint(texelFetch(raw, ivec2(g * 5 + i, p.y), 0).r * 255.0 + 0.5);\n"
        "    uint lo = uint(texelFetch(raw, ivec2(g * 5 + 4, p.y), 0).r * 255.0 + 0.5);\n"
        "    return float((hi << 2u) | ((lo >> uint(2 * i)) & 3u)) / 1023.0;\n"
        "}\n"
        "ivec2 source_pos(ivec2 o) {\n"
        "    int W = src_size.x, H = src_size.y;\n"
        "    if (orient == 2) return ivec2(W - 1 - o.x, o.y);\n"
        "    if (orient == 3) return ivec2(W - 1 - o.x, H - 1 - o.y);\n"
        "    if (orient == 4) return ivec2(o.x, H - 1 - o.y);\n"
        "    if (orient == 5) return ivec2(o.y, o.x);\n"
        "    if (orient == 6) return ivec2(o.y, H - 1 - o.x);\n"
        "    if (orient == 7) return ivec2(W - 1 - o.y, H - 1 - o.x);\n"
        "    if (orient == 8) return ivec2(W - 1 - o.y, o.x);\n"
        "    return o;\n"
        "}\n"
        "void main() {\n"
        "    ivec2 o = ivec2(int(gl_FragCoord.x), strip_y0 + int(gl_FragCoord.y));\n"
        "    ivec2 p = clamp(source_pos(o), ivec2(0), src_size - 1);\n"
        "    float C = px(p);\n"
        "    float N = px(p + ivec2(0, -1)), S = px(p + ivec2(0, 1)), E = px(p + ivec2(1, 0)), Wv = px(p + ivec2(-1, 0));\n"
        "    float N2 = px(p + ivec2(0, -2)), S2 = px(p + ivec2(0, 2)), E2 = px(p + ivec2(2, 0)), W2 = px(p + ivec2(-2, 0));\n"
        "    float D = px(p + ivec2(-1, -1)) + px(p + ivec2(1, -1)) + px(p + ivec2(-1, 1)) + px(p + ivec2(1, 1));\n"
        "    float cross1 = N + S + E + Wv;\n"
        "    float cross2 = N2 + S2 + E2 + W2;\n"
        "    int rx = (p.x - red_pos.x) & 1, ry = (p.y - red_pos.y) & 1;\n"
        "    float g_at_rb = (4.0 * C + 2.0 * cross1 - cross2) / 8.0;\n"
        "    float rb_at_br = (6.0 * C + 2.0 * D - 1.5 * cross2) / 8.0;\n"
        "    float h = (5.0 * C + 4.0 * (E + Wv) - (E2 + W2) - D + 0.5 * (N2 + S2)) / 8.0;\n" // the colour of the row neighbours
        "    float v = (5.0 * C + 4.0 * (N + S) - (N2 + S2) - D + 0.5 * (E2 + W2)) / 8.0;\n" // the colour of the column neighbours
        "    vec3 rgb;\n"
        "    if (rx == 0 && ry == 0) rgb = vec3(C, g_at_rb, rb_at_br);\n"         // red site
        "    else if (rx == 1 && ry == 1) rgb = vec3(rb_at_br, g_at_rb, C);\n"    // blue site
        "    else if (ry == 0) rgb = vec3(h, C, v);\n"                            // green in a red row
        "    else rgb = vec3(v, C, h);\n"                                         // green in a blue row
        "    vec3 color = max(rgb - blacklevel, vec3(0.0));\n"
        "    color *= color_matrix;\n"
        "    vec3 g = clamp(pow(max(color, vec3(0.0)), vec3(inv_gamma)), 0.0, 1.0);\n"
        // identical to the viewfinder (debayer_packed.frag): S-curve k=0.5 and +25 % saturation
        "    g += 0.5 * (g * g * (3.0 - 2.0 * g) - g);\n"
        "    float l = dot(g, vec3(0.299, 0.587, 0.114));\n"
        "    g = clamp(vec3(l) + (g - vec3(l)) * 1.25, 0.0, 1.0);\n"
        "    frag = vec4(g, 1.0);\n"
        "}\n";

// m52xq r72: second pass, edge-preserving 5x5 noise reduction on the developed strip (colour noise fully, brightness
// partly): weights by distance and by the brightness difference, so edges stay sharp. The strip is developed with
// 10 extra rows above and below (tmp texture, r73) so the filters need no neighbouring strips.
static const char *denoise_src =
        "#version 300 es\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "uniform highp sampler2D tmp;\n"
        "uniform float sigma_r;\n"    // brightness difference that still counts as the same surface
        "uniform float luma_k;\n"     // share of the filtered brightness (colour is always filtered)
        "uniform float wide_k;\n"     // r73: share of the wide (19x19, every 3rd pixel) colour average
        "uniform float mid_k;\n"      // r82: share of the mid-scale (9x9, every 2nd pixel) brightness average (high gain)
        "out vec4 frag;\n"
        "void main() {\n"
        "    ivec2 c = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y) + 10);\n"
        "    ivec2 sz = textureSize(tmp, 0);\n"
        "    vec3 c0 = texelFetch(tmp, c, 0).rgb;\n"
        "    const vec3 lw = vec3(0.299, 0.587, 0.114);\n"
        "    float y0 = dot(c0, lw);\n"
        "    vec3 acc = vec3(0.0);\n"
        "    float wsum = 0.0;\n"
        "    for (int j = -2; j <= 2; j++) {\n"
        "        for (int i = -2; i <= 2; i++) {\n"
        "            ivec2 q = clamp(c + ivec2(i, j), ivec2(0), sz - 1);\n"
        "            vec3 v = texelFetch(tmp, q, 0).rgb;\n"
        "            float d = dot(v, lw) - y0;\n"
        "            float w = exp(-float(i * i + j * j) / 4.5 - d * d / (2.0 * sigma_r * sigma_r));\n"
        "            acc += v * w;\n"
        "            wsum += w;\n"
        "        }\n"
        "    }\n"
        "    vec3 f = acc / wsum;\n"
        "    float yf = dot(f, lw);\n"
        "    float y = mix(y0, yf, luma_k);\n"
        "    if (mid_k > 0.0) {\n"   // r82: the grain of 3-6 px at high gain (the 5x5 pass leaves it)
        "        float macc = 0.0, mw = 0.0;\n"
        "        for (int j = -2; j <= 2; j++) {\n"
        "            for (int i = -2; i <= 2; i++) {\n"
        "                ivec2 q = clamp(c + ivec2(i, j) * 2, ivec2(0), sz - 1);\n"
        "                float yv = dot(texelFetch(tmp, q, 0).rgb, lw);\n"
        "                float d = yv - yf;\n"
        "                float w = exp(-float(i * i + j * j) / 4.5 - d * d / (2.0 * 2.25 * sigma_r * sigma_r));\n"
        "                macc += yv * w;\n"
        "                mw += w;\n"
        "            }\n"
        "        }\n"
        "        y = mix(y, macc / mw, mid_k);\n"
        "    }\n"
        "    vec3 ch = f - vec3(yf);\n"
        // r73: the large yellow/blue colour blotches: average only the colour over +-9 px, weighted by distance and
        // by the brightness difference (so colour does not leak over edges); brightness is left untouched
        "    if (wide_k > 0.0) {\n"   // r74: skipped on the rear camera (costs ~0.45 s at 64 MP, little colour noise there)
        "    vec3 cacc = vec3(0.0);\n"
        "    float cw = 0.0;\n"
        "    for (int j = -3; j <= 3; j++) {\n"
        "        for (int i = -3; i <= 3; i++) {\n"
        "            ivec2 q = clamp(c + ivec2(i, j) * 3, ivec2(0), sz - 1);\n"
        "            vec3 v = texelFetch(tmp, q, 0).rgb;\n"
        "            float yv = dot(v, lw);\n"
        "            float d = yv - yf;\n"
        "            float w = exp(-float(i * i + j * j) / 8.0 - d * d / (2.0 * 4.0 * sigma_r * sigma_r));\n"
        "            cacc += (v - vec3(yv)) * w;\n"
        "            cw += w;\n"
        "        }\n"
        "    }\n"
        "    ch = mix(ch, cacc / cw, wide_k);\n"
        "    }\n"
        "    frag = vec4(clamp(vec3(y) + ch, 0.0, 1.0), 1.0);\n"
        "}\n";

static GLuint program = 0;
static GLuint dn_program = 0;
static GLint u_tmp, u_sigma_r, u_luma_k, u_wide_k, u_mid_k;
static float mid_nr = 0.0f; // r82: set per photo from the sensor gain
static float wide_nr = -1.0f; // r83: share of the wide colour pass (-1: the r74 rule, front only)

void
mp_photo_gpu_set_wide_nr(float k)
{
        wide_nr = k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
}

void
mp_photo_gpu_set_mid_nr(float k)
{
        mid_nr = k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
}
static GLint u_raw, u_src_size, u_red_pos, u_orient, u_strip_y0, u_color_matrix, u_blacklevel, u_inv_gamma;

static GLuint
compile(GLenum type, const char *src)
{
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
                char log[2048];
                glGetShaderInfoLog(s, sizeof(log), NULL, log);
                fprintf(stderr, "photo_gpu: shader compile failed: %s\n", log);
                glDeleteShader(s);
                return 0;
        }
        return s;
}

static bool
ensure_program(void)
{
        if (program)
                return true;
        GLuint vs = compile(GL_VERTEX_SHADER, vertex_src);
        GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_src);
        if (!vs || !fs)
                return false;
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
                char log[2048];
                glGetProgramInfoLog(p, sizeof(log), NULL, log);
                fprintf(stderr, "photo_gpu: link failed: %s\n", log);
                glDeleteProgram(p);
                return false;
        }
        program = p;
        {
                GLuint vs2 = compile(GL_VERTEX_SHADER, vertex_src);
                GLuint fs2 = compile(GL_FRAGMENT_SHADER, denoise_src);
                if (vs2 && fs2) {
                        GLuint d = glCreateProgram();
                        glAttachShader(d, vs2);
                        glAttachShader(d, fs2);
                        glLinkProgram(d);
                        GLint dok = 0;
                        glGetProgramiv(d, GL_LINK_STATUS, &dok);
                        if (dok) {
                                dn_program = d;
                                u_tmp = glGetUniformLocation(d, "tmp");
                                u_sigma_r = glGetUniformLocation(d, "sigma_r");
                                u_luma_k = glGetUniformLocation(d, "luma_k");
                                u_wide_k = glGetUniformLocation(d, "wide_k");
                                u_mid_k = glGetUniformLocation(d, "mid_k");
                        } else {
                                glDeleteProgram(d);
                        }
                }
                if (vs2)
                        glDeleteShader(vs2);
                if (fs2)
                        glDeleteShader(fs2);
        }
        u_raw = glGetUniformLocation(p, "raw");
        u_src_size = glGetUniformLocation(p, "src_size");
        u_red_pos = glGetUniformLocation(p, "red_pos");
        u_orient = glGetUniformLocation(p, "orient");
        u_strip_y0 = glGetUniformLocation(p, "strip_y0");
        u_color_matrix = glGetUniformLocation(p, "color_matrix");
        u_blacklevel = glGetUniformLocation(p, "blacklevel");
        u_inv_gamma = glGetUniformLocation(p, "inv_gamma");
        return true;
}

static double
now_ms(void)
{
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// m52xq r75: the GPU governor (simple_ondemand) stays at 315 MHz for the short burst of a photo (the same 64 MP development
// took 0.45 s or 1.36 s): raise the floor to the top frequency while developing. min_freq is made writable for the user by
// /etc/udev/rules.d/90-m52xq-gpu-minfreq.rules; without it nothing changes.
#define GPU_MINFREQ "/sys/class/devfreq/3d00000.qcom,kgsl-3d0/min_freq"
#define GPU_MAXFREQ "/sys/class/devfreq/3d00000.qcom,kgsl-3d0/max_freq"
static void
gpu_floor(bool boost)
{
        static char saved[32] = "";
        char buf[32] = "";
        if (boost) {
                FILE *f = fopen(GPU_MINFREQ, "r");
                if (!f)
                        return;
                if (!fgets(saved, sizeof(saved), f))
                        saved[0] = 0;
                fclose(f);
                f = fopen(GPU_MAXFREQ, "r");
                if (!f)
                        return;
                if (!fgets(buf, sizeof(buf), f))
                        buf[0] = 0;
                fclose(f);
        } else {
                if (!saved[0])
                        return;
                snprintf(buf, sizeof(buf), "%s", saved);
                saved[0] = 0;
        }
        FILE *w = fopen(GPU_MINFREQ, "w");
        if (w) {
                fputs(buf, w);
                fclose(w);
        }
}

static bool develop(const uint8_t *raw, int width, int height, int line_bytes, const char *cfa, int orientation,
                    const float color_matrix[9], float blacklevel, float inv_gamma, const char *jpeg_path, int quality,
                    float nr);

bool
mp_photo_gpu_develop(const uint8_t *raw, int width, int height, int line_bytes, const char *cfa, int orientation,
                     const float color_matrix[9], float blacklevel, float inv_gamma, const char *jpeg_path, int quality,
                     float nr)
{
        gpu_floor(true);
        bool ok = develop(raw, width, height, line_bytes, cfa, orientation, color_matrix, blacklevel, inv_gamma, jpeg_path,
                          quality, nr);
        gpu_floor(false);
        return ok;
}

static bool
develop(const uint8_t *raw, int width, int height, int line_bytes, const char *cfa, int orientation,
                     const float color_matrix[9], float blacklevel, float inv_gamma, const char *jpeg_path, int quality,
                     float nr)
{
        double t0 = now_ms();
        if (width % 4 || !ensure_program())
                return false;

        GLint max_tex = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
        if (line_bytes > max_tex || height > max_tex) {
                fprintf(stderr, "photo_gpu: %dx%d raw does not fit a %d texture\n", line_bytes, height, max_tex);
                return false;
        }

        int red_x = 0, red_y = 0;
        if (cfa && strcmp(cfa, "GRBG") == 0) {
                red_x = 1;
        } else if (cfa && strcmp(cfa, "GBRG") == 0) {
                red_y = 1;
        } else if (cfa && strcmp(cfa, "BGGR") == 0) {
                red_x = 1;
                red_y = 1;
        }
        // m52xq r68: the pixels stay in sensor order and the JPEG carries the EXIF orientation of the DNG (the shader
        // mapping of the transposed cases came out 180 degrees off for the mirrored front camera); viewers apply it
        int exif_orientation = orientation >= 1 && orientation <= 8 ? orientation : 1;
        orientation = 1;
        int out_w = width;
        int out_h = height;

        // keep the state of the viewfinder rendering
        GLint prev_program, prev_fb, prev_tex, prev_active, prev_viewport[4], prev_align;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fb);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        glGetIntegerv(GL_VIEWPORT, prev_viewport);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);

        GLuint src_tex, dst_tex, fb;
        glGenTextures(1, &src_tex);
        glBindTexture(GL_TEXTURE_2D, src_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, line_bytes, height, 0, GL_RED, GL_UNSIGNED_BYTE, raw);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glGenTextures(1, &dst_tex);
        glBindTexture(GL_TEXTURE_2D, dst_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, out_w, STRIP_ROWS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex, 0);
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        // m52xq r72/r73: developed strip with 10 margin rows on each side, input of the noise reduction pass
        bool denoise = nr > 0.01f && dn_program != 0;
        GLuint tmp_tex = 0, fb_tmp = 0;
        if (ok && denoise) {
                glGenTextures(1, &tmp_tex);
                glBindTexture(GL_TEXTURE_2D, tmp_tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, out_w, STRIP_ROWS + 20, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glGenFramebuffers(1, &fb_tmp);
                glBindFramebuffer(GL_FRAMEBUFFER, fb_tmp);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_tex, 0);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                        denoise = false;
                glBindFramebuffer(GL_FRAMEBUFFER, fb);
        }

        FILE *out = ok ? fopen(jpeg_path, "wb") : NULL;
        uint8_t *strip = ok ? malloc((size_t)out_w * STRIP_ROWS * 4) : NULL;
        if (!out || !strip) {
                ok = false;
        }

        double t_gpu = 0, t_jpeg = 0;
        if (ok) {
                struct jpeg_compress_struct cinfo;
                struct jpeg_error_mgr jerr;
                cinfo.err = jpeg_std_error(&jerr);
                jpeg_create_compress(&cinfo);
                jpeg_stdio_dest(&cinfo, out);
                cinfo.image_width = out_w;
                cinfo.image_height = out_h;
                cinfo.input_components = 4;
                cinfo.in_color_space = JCS_EXT_RGBA;
                jpeg_set_defaults(&cinfo);
                jpeg_set_quality(&cinfo, quality, TRUE);
                jpeg_start_compress(&cinfo, TRUE);
                {
                        // minimal EXIF: TIFF header (little endian) + IFD0 with the Orientation tag only
                        const JOCTET exif[] = {
                                'E', 'x', 'i', 'f', 0, 0,
                                'I', 'I', 0x2a, 0, 8, 0, 0, 0,
                                1, 0,
                                0x12, 0x01, 3, 0, 1, 0, 0, 0, (JOCTET)exif_orientation, 0, 0, 0,
                                0, 0, 0, 0,
                        };
                        jpeg_write_marker(&cinfo, JPEG_APP0 + 1, exif, sizeof(exif));
                }

                glUseProgram(program);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, src_tex);
                glUniform1i(u_raw, 0);
                glUniform2i(u_src_size, width, height);
                glUniform2i(u_red_pos, red_x, red_y);
                glUniform1i(u_orient, orientation);
                glUniformMatrix3fv(u_color_matrix, 1, GL_FALSE, color_matrix);
                glUniform1f(u_blacklevel, blacklevel);
                glUniform1f(u_inv_gamma, inv_gamma);

                for (int y0 = 0; y0 < out_h; y0 += STRIP_ROWS) {
                        int rows = out_h - y0 < STRIP_ROWS ? out_h - y0 : STRIP_ROWS;
                        double a = now_ms();
                        if (denoise) {
                                // pass 1: develop rows y0-10 .. y0+rows+9 (the raw lookup clamps at the frame edges)
                                glUseProgram(program);
                                glActiveTexture(GL_TEXTURE0);
                                glBindTexture(GL_TEXTURE_2D, src_tex);
                                glBindFramebuffer(GL_FRAMEBUFFER, fb_tmp);
                                glViewport(0, 0, out_w, rows + 20);
                                glUniform1i(u_strip_y0, y0 - 10);
                                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                                // pass 2: noise reduction into the output strip
                                glUseProgram(dn_program);
                                glBindTexture(GL_TEXTURE_2D, tmp_tex);
                                glUniform1i(u_tmp, 0);
                                glUniform1f(u_sigma_r, 0.02f + 0.06f * nr);
                                glUniform1f(u_luma_k, 0.6f * nr);
                                glUniform1f(u_wide_k, wide_nr >= 0.0f ? wide_nr : (nr >= 0.99f ? 1.0f : 0.0f)); // r74/r83
                                glUniform1f(u_mid_k, mid_nr);
                                glBindFramebuffer(GL_FRAMEBUFFER, fb);
                                glViewport(0, 0, out_w, rows);
                                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        } else {
                                glViewport(0, 0, out_w, rows);
                                glUniform1i(u_strip_y0, y0);
                                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        }
                        glReadPixels(0, 0, out_w, rows, GL_RGBA, GL_UNSIGNED_BYTE, strip);
                        double b = now_ms();
                        for (int r = 0; r < rows; r++) {
                                JSAMPROW row = strip + (size_t)r * out_w * 4;
                                jpeg_write_scanlines(&cinfo, &row, 1);
                        }
                        t_gpu += b - a;
                        t_jpeg += now_ms() - b;
                }
                jpeg_finish_compress(&cinfo);
                jpeg_destroy_compress(&cinfo);
        }
        if (out)
                fclose(out);
        free(strip);

        glBindFramebuffer(GL_FRAMEBUFFER, prev_fb);
        if (fb_tmp)
                glDeleteFramebuffers(1, &fb_tmp);
        if (tmp_tex)
                glDeleteTextures(1, &tmp_tex);
        glDeleteFramebuffers(1, &fb);
        glDeleteTextures(1, &dst_tex);
        glDeleteTextures(1, &src_tex);
        glUseProgram(prev_program);
        glBindTexture(GL_TEXTURE_2D, prev_tex);
        glActiveTexture(prev_active);
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);

        printf("photo_gpu: EXIF orientation %d, noise reduction %.2f, mid-scale %.2f\n", exif_orientation, denoise ? nr : 0.0f,
               denoise ? mid_nr : 0.0f);
        printf("photo_gpu: %dx%d -> %s %s in %.0f ms (GPU+readback %.0f ms, JPEG %.0f ms)\n", out_w, out_h, jpeg_path,
               ok ? "developed" : "FAILED", now_ms() - t0, t_gpu, t_jpeg);
        if (!ok)
                remove(jpeg_path);
        return ok;
}
