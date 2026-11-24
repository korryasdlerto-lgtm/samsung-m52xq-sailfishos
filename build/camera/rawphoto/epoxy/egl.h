/* m52xq (Sailfish has no libepoxy): plain EGL */
#ifndef M52XQ_EPOXY_EGL_H
#define M52XQ_EPOXY_EGL_H
#define EGL_EGLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdbool.h>
#include <string.h>
static inline bool epoxy_has_egl_extension(EGLDisplay d, const char *ext)
{
	const char *s = eglQueryString(d, EGL_EXTENSIONS), *p;
	size_t n = strlen(ext);
	for (p = s; p && (p = strstr(p, ext)); p += n)
		if ((p == s || p[-1] == ' ') && (p[n] == ' ' || !p[n]))
			return true;
	return false;
}
/* EGL 1.5 core entry point; attribute lists are EGL_NONE or NULL here */
#define eglGetPlatformDisplayEXT(p, n, a) ((PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT"))((p), (n), (a))
#endif
