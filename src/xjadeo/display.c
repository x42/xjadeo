/* xjadeo - video output abstraction and common display functions
 *
 * (C) 2006-2014 Robin Gareus <robin@gareus.org>
 * (C) 2006-2011 Luis Garrido <luisgarrido@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "xjadeo.h"
#include "display.h"
#include <assert.h>

#include <libavcodec/avcodec.h> // needed for PIX_FMT
#include <libavformat/avformat.h>

#ifndef MIN
#define MIN(A,B) (((A)<(B)) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) (((A)>(B)) ? (A) : (B))
#endif

extern char  *smpte_offset;
extern int64_t ts_offset; // display on screen
extern int64_t frames;
extern int64_t dispFrame;
extern int    want_nosplash;
extern double framerate;
extern float index_progress;
extern uint8_t splashed ;
uint8_t osd_seeking = 0;

/*******************************************************************************
 * NULL Video Output
 */

static int  open_window_null (void) { return (1); }
static void close_window_null (void) { ; }
static void render_null (uint8_t *mybuffer) { ; }
static void handle_X_events_null (void) { ; }
static void newsrc_null (void) { ; }
static void resize_null (unsigned int x, unsigned int y) { ; }
static void getsize_null (unsigned int *x, unsigned int *y) { if (x) *x = 1; if (y) *y = 1; }
static void position_null (int x, int y) { ; }
static void getpos_null (int *x, int *y) { if(x)*x=1; if (y) *y = 1; }
static void fullscreen_null (int a) { ; }
static void mousepointer_null (int a) { ; }
static void ontop_null (int a) { ; }
static int  getfullscreen_null () { return (0); }
static int  getontop_null () { return(0); }
static void letterbox_change_null () { ; }

/*******************************************************************************
 * strided memcopy - convert pitches of video buffer
 */
inline void stride_memcpy(void * dst, const void * src, int width, int height, int dstStride, int srcStride) {
	int i;
	if(dstStride == srcStride)
		memcpy(dst, src, srcStride*height);
	else for(i=0; i<height; i++) {
		memcpy(dst, src, width);
		src+= srcStride;
		dst+= dstStride;
	}
}

/*******************************************************************************
 * On Screen Display helper
 * overlay render directly on image on the given buffer
 */

typedef struct {
	size_t Uoff;
	size_t Voff;
	int bpp;
} rendervars;

static void _overlay_YUV422 (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int yoff = (2 * dx + movie_width * dy * 2);
	mybuffer[yoff+1] = 0xff - ((mybuffer[yoff+1] >> 1) + (val >> 1));
	mybuffer[yoff+3] = 0xff - ((mybuffer[yoff+3] >> 1) + (val >> 1));
}

static void _overlay_YUV (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int yoff = (dx + movie_width * dy);
	mybuffer[yoff] = 0xff - ((mybuffer[yoff] >> 1)  + (val >> 1));
}

static void _overlay_RGB (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int pos = rv->bpp * (dx + movie_width * dy);
	mybuffer[pos]  = 0xff - ((mybuffer[pos]   >> 1) + (val >> 1));
	mybuffer[pos+1]= 0xff - ((mybuffer[pos+1] >> 1) + (val >> 1));
	mybuffer[pos+2]= 0xff - ((mybuffer[pos+2] >> 1) + (val >> 1));
}

static void _render_YUV422 (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int yoff = 2 * dx + movie_width * dy * 2;
	mybuffer[yoff+0] = 0x80;
	mybuffer[yoff+1] = val;
	mybuffer[yoff+2] = 0x80;
	mybuffer[yoff+3] = val;
}

static void _render_YUV (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int yoff = (dx + movie_width * dy);
	const int uvoff = (dx / 2) + movie_width / 2 * (dy / 2);
	mybuffer[yoff] = val;
	mybuffer[rv->Uoff+uvoff] = 0x80;
	mybuffer[rv->Voff+uvoff] = 0x80;
}

static void _render_RGB (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int pos = rv->bpp * (dx + movie_width * dy);
	mybuffer[pos] = val;
	mybuffer[pos+1] = val;
	mybuffer[pos+2] = val;
}

static void _red_render_YUV422 (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int yoff = 2 * (dx&~1) + movie_width * dy * 2;
	if (val > 0x10) {
		mybuffer[yoff+0] = 0x40;
		mybuffer[yoff+2] = 0xb0;
	}
	if (dx%2)
		mybuffer[yoff+3] = val >> 1;
	else
		mybuffer[yoff+1] = val >> 1;
}

static void _red_render_YUV (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int yoff = (dx + movie_width * dy);
	const int uvoff = (dx / 2) + movie_width / 2 * (dy / 2);
	mybuffer[yoff] = val > 0x90 ? 0x90 : val;
	mybuffer[rv->Uoff+uvoff] = val > 0x10 ? 0x50 : 0x80;
	mybuffer[rv->Voff+uvoff] = val > 0x10 ? 0xc0 : 0x80;
}

static void _red_render_RGB (uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val) {
	const int pos = rv->bpp * (dx + movie_width * dy);
	mybuffer[pos] = val>>2;
	mybuffer[pos+1] = val>>2;
	mybuffer[pos+2] = val;
}


/*******************************************************************************
 * colorspace utils (slow) - used for old imlib big/little endian compat
 */

void rgb2argb (uint8_t *rgbabuffer, uint8_t *rgbbuffer, int width, int height) {
	int x, y, p3, p4;
	for (x = 0; x < width; ++x)
		for (y = 0; y < height; ++y) {
			p3 = 3 * (x + movie_width * y);
			p4 = 4 * (x + movie_width * y);
			rgbabuffer[p4+0] = 255;
			rgbabuffer[p4+1] = rgbbuffer[p3];
			rgbabuffer[p4+2] = rgbbuffer[p3+1];
			rgbabuffer[p4+3] = rgbbuffer[p3+2];
		}
}

void rgb2abgr (uint8_t *rgbabuffer, uint8_t *rgbbuffer, int width, int height) {
	int x, y, p3, p4;
	for (x = 0; x < width; ++x)
		for (y = 0; y < height; ++y) {
			p3 = 3 * (x + movie_width * y);
			p4 = 4 * (x + movie_width * y);
			rgbabuffer[p4+3] = 255;
			rgbabuffer[p4+2] = rgbbuffer[p3];
			rgbabuffer[p4+1] = rgbbuffer[p3+1];
			rgbabuffer[p4+0] = rgbbuffer[p3+2];
		}
}

/*******************************************************************************
 * xjadeo displays engine definitions
 */

#define NULLOUTPUT &render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null, &getpos_null, &fullscreen_null, &ontop_null, &mousepointer_null, &getfullscreen_null, &getontop_null, &letterbox_change_null

// see xjadeo.h VideoModes
const vidout VO[] = {
	{ PIX_FMT_RGB24,   1, 		"NULL", NULLOUTPUT}, // NULL is --vo 0 -> autodetect
	{ PIX_FMT_BGRA32,   SUP_OPENGL,   "OpenGL",
#ifdef HAVE_GL
		&gl_render, &gl_open_window, & gl_close_window,
		&gl_handle_events, &gl_newsrc,
		&gl_resize, &gl_get_window_size,
		&gl_position, &gl_get_window_pos,
		&gl_set_fullscreen, &gl_set_ontop,
		&gl_mousepointer,
		&gl_get_fullscreen, &gl_get_ontop,
		&gl_letterbox_change
#else
			NULLOUTPUT
#endif
	},
	{ PIX_FMT_YUV420P, SUP_LIBXV, "XV - X11 video extension",
#if HAVE_LIBXV
		&render_xv, &open_window_xv, &close_window_xv,
		&handle_X_events_xv, &newsrc_xv, &resize_xv,
		&get_window_size_xv, &position_xv, get_window_pos_xv,
		&xj_set_fullscreen, &xj_set_ontop, &xj_mousepointer,
		&xj_get_fullscreen, &xj_get_ontop, &xj_letterbox
#else
			NULLOUTPUT
#endif
	},
	{ PIX_FMT_YUV420P, SUP_SDL, "SDL",
#ifdef HAVE_SDL
		&render_sdl, &open_window_sdl, &close_window_sdl,
		&handle_X_events_sdl, &newsrc_sdl, &resize_sdl,
		&getsize_sdl, &position_sdl, &get_window_pos_sdl,
		&sdl_toggle_fullscreen, &sdl_set_ontop, &mousecursor_sdl,
		&sdl_get_fullscreen, &sdl_get_ontop, &sdl_letterbox_change
#else
			NULLOUTPUT
#endif
	},
	{
#ifdef IMLIB2RGBA
		PIX_FMT_BGRA32,   SUP_IMLIB2,   "ImLib2/x11 (RGBA32)",
#else
		PIX_FMT_RGB24,   SUP_IMLIB2,   "ImLib2/x11 (RGB24)",
#endif
#if HAVE_IMLIB2
		&render_imlib2, &open_window_imlib2, &close_window_imlib2,
		&handle_X_events_imlib2, &newsrc_imlib2, &resize_imlib2,
		&get_window_size_imlib2, &position_imlib2, &get_window_pos_imlib2,
		&xj_set_fullscreen, &xj_set_ontop, &xj_mousepointer,
		&getfullscreen_null, &getontop_null, &xj_letterbox
#else
			NULLOUTPUT
#endif
	},
	{ PIX_FMT_UYVY422,   SUP_MACOSX,   "Mac OSX - quartz",
#if defined PLATFORM_OSX && (defined __i386 || defined __ppc__)
		&render_mac, &open_window_mac, &close_window_mac,
		&handle_X_events_mac, &newsrc_mac, &resize_mac,
		&getsize_mac, &position_mac, &getpos_mac,
		&fullscreen_mac, &ontop_mac, &mousepointer_null,
		&get_fullscreen_mac, &get_ontop_mac, &mac_letterbox_change
#else
			NULLOUTPUT
#endif
	},
	{-1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL} // the end.
};

static int VOutput = 0;

int parsevidoutname (char *arg) {
	int i = 0;
	int s0 = strlen(arg);
	if (s0 == 0) return (0);

	if (!strncasecmp("list", arg, s0 > 4 ? 4 : s0)) return -1;

	for(i = 0; i < sizeof(VO) / sizeof (vidout); ++i) {
		if (VO[i].supported >= 0) {
			int s1 = strlen(VO[i].name);
			if (!strncasecmp(VO[i].name, arg, s0 > s1 ? s1 : s0)) return i;
		}
	}
	return(0);
}

const char * vidoutname (int i) {
	return (VO[i].name);
}

int vidoutsupported (int i) {
	return(VO[i].supported);
}

void dump_vopts (void) {
	int i = 0;
	fprintf (stdout, "Video Output Modes: \n");
	fprintf (stdout, " --vo 0 # autodetect (default)\n");

	while (VO[++i].supported >= 0) {
		fprintf (stdout, " --vo %i # %s %s\n", i, VO[i].name,
				VO[i].supported ? "(supported by this xjadeo)" : "(NOT compiled in this xjadeo)");
	}
}

int try_next_vidoutmode(int user_req) {
	int i = 0;
	// check available modes..
	while (VO[++i].supported >= 0);

	if (user_req >= i || user_req < 0 ) return (-1);
	if (user_req < i && user_req > 0 && VO[user_req].supported) return(1);
	return (0);
}

int vidoutmode(int user_req) {
	int i = 0;
	if (user_req < 0) {
		dump_vopts();
		exit (0);
	}

	VOutput = 0;

	// check available modes..
	while (VO[++i].supported >= 0) {
		if (VO[i].supported && VOutput == 0) {
			VOutput = i;
		}
	}

	if (user_req < i && user_req > 0)
		if (VO[user_req].supported) VOutput = user_req;

	return VO[VOutput].render_fmt;
}

/*******************************************************************************
 * On Screen Display
 */

extern unsigned char ST_image[][ST_WIDTH];
extern int ST_rightend;
extern int ST_height;
extern int ST_top;

#define PB_W (movie_width - 2 * PB_X)

#define SET_RFMT(FORMAT, POINTER, VARS, FUNC) \
	if ((FORMAT) == PIX_FMT_YUV420P) \
		(POINTER) = &_##FUNC##_YUV; \
	else if ((FORMAT) == PIX_FMT_UYVY422) \
		(POINTER) = &_##FUNC##_YUV422; \
	else if ((FORMAT) == PIX_FMT_RGB24) { \
		 (POINTER) = &_##FUNC##_RGB; \
		VARS.bpp = 3; \
	} else if ((FORMAT) == PIX_FMT_RGBA32) { \
		(POINTER) = &_##FUNC##_RGB; \
		VARS.bpp = 4; \
	} else if ((FORMAT) == PIX_FMT_BGRA32) { \
		(POINTER) = &_##FUNC##_RGB; \
		VARS.bpp = 4; \
	} else return ;

#if (HAVE_LIBXV || HAVE_IMLIB2)
static void OSD_bitmap(int rfmt, uint8_t *mybuffer, int yperc, int xoff, int w, int h, uint8_t *src, uint8_t *mask) {
	int x, y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val);

	rv.Uoff  = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height/4;
	rv.bpp = 0;

	xalign = (movie_width - w) / 2;
	yalign = (movie_height - h) * yperc / 100.0;
	if (xalign < 0) xalign = 0;
	if (yalign < 0) yalign = 0;

	SET_RFMT(rfmt, _render, rv, render); // TODO once per window, and rather make _render fn's inline, include LOOP in fn pointer.

	for (x = 0; x < w && (x + xalign) < movie_width; ++x) {
		for (y = 0; y < h && (y + yalign) < movie_height; ++y) {
			int byte = ((y * w + x) >> 3); // PIXMAP width must be mult. of 8 !
			int val = src[byte] & (1 << (x % 8));
			if (!mask || mask[byte] & (1 << (x % 8)))
				_render (mybuffer, &rv, (x + xalign), (y + yalign), val ? 0xee : 0x11);
		}
	}
}
#endif

static void OSD_cmap(int rfmt, uint8_t *mybuffer, int yperc, int xoff, const int w, const int h,
		uint8_t const * const img, uint8_t const * const map) {
	int x, y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val);

	rv.Uoff = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height / 4;
	rv.bpp  = 0;

	xalign = (movie_width - w) / 2;
	yalign = (movie_height - h) * yperc / 100.0;
	if (xalign < 0) xalign = 0;
	if (yalign < 0) yalign = 0;

	SET_RFMT(rfmt, _render, rv, render); // TODO once per window, and rather make _render fn's inline, include LOOP in fn pointer.

	for (y = 0; y < h && (y + yalign) < movie_height; ++y) {
		const int y0 = w * y;
		for (x = 0; x < w && (x + xalign) < movie_width; ++x) {
			const int pos = y0 + x;
			uint8_t val = map[(uint8_t)img[pos]];
			_render (mybuffer, &rv, (x+xalign), (y+yalign), val);
		}
	}
}


/* background, black box */
#define ST_BG ((OSD_mode&OSD_BOX)?0:1)

static void OSD_bar (int rfmt, uint8_t *mybuffer, int yperc, double min, double max, double val, double tara)
{
	int x, y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val);
	if (movie_width < 4 * PB_X || movie_height < 6 * PB_H) return;

	rv.Uoff  = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height/4;
	rv.bpp = 0;

	xalign = PB_X;
	yalign = (movie_height - PB_H) * yperc / 100.0;
	int pb_val = (int) (PB_W * (val-min) / (max - min));
	int pb_not = (int) (PB_W * (tara-min) / (max - min));

	SET_RFMT(rfmt, _render, rv, overlay); // TODO once per window instance, inline _render.

	for (x = 0; x < pb_val && (x + xalign) < movie_width; ++x) {
		for (y = 3; y < PB_H && (y + yalign) < movie_height; ++y) {
			_render (mybuffer, &rv, x + xalign, y + yalign,
					(x % 9 && y != 3) ? 0xf0 : 0x20);
		}
		if ((x % 9) == 5) x += 3; // bars'n'stripes
	}
	if (tara >= min) {
		/* zero notch */
		for (x = pb_not - 1; x < pb_not + 2 && (x + xalign) < movie_width; ++x) {
			for (y = 0; x >= 0 && y < 3 && (y + yalign) < movie_height; ++y)
				_render (mybuffer, &rv, x + xalign, y + yalign, 0);
			for (y = PB_H; x >= 0 && y < PB_H + 3 && (y + yalign) < movie_height; ++y)
				_render (mybuffer, &rv, x + xalign, y + yalign, 0);
		}
	} else if (tara < min - 1) {
		/* border */
		const uint8_t bcol = tara < min - 2 ? 0x10 : 0xf0;
		for (x = -2; x < PB_W + 1 && (x + xalign) < movie_width; ++x) {
			if (yalign >= 0 && yalign < movie_height)
				_render (mybuffer, &rv, x + xalign, yalign, bcol);
			if (yalign + PB_H + 3 >= 0 && yalign + PB_H + 3 < movie_height)
				_render (mybuffer, &rv, x + xalign, yalign + PB_H + 3, bcol);
		}
		for (y = 1; y < PB_H + 3 && (y + yalign) < movie_height; ++y) {
			_render (mybuffer, &rv, PB_X - 3 , y + yalign, bcol);
			_render (mybuffer, &rv, PB_X + PB_W + 1, y + yalign, bcol);
		}
	}
}

enum MinWHVariant {
	MINW__TC = -1,
	MINWH_NONE = 0,
	MINWH_SYNCTC,
	MINWH_FRAMEN,
};

static void OSD_render (int rfmt, uint8_t *mybuffer, char *text, int xpos, int yperc, enum MinWHVariant minwh) {
	static int OSD_movieheight = -1;
	static int OSD_fonty0 = -1;
	static int OSD_fonty1 = -1;
	static int OSD_fontsize = -1;
	static int OSD_monospace = 0;
	static int minw_frame = 0;
	static int minw_smpte = 0;

	int x, y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, const int dx, const int dy, const uint8_t val);

	if (strlen(text) == 0) return;

	rv.Uoff = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height/4;
	rv.bpp = 0;

	if ((!strncmp(text, "++ ", 3) || !strncmp(text, "-- ", 3)) && strstr(text, " EOF")) {
		SET_RFMT(rfmt, _render, rv, red_render);
	} else {
		SET_RFMT(rfmt, _render, rv, render);
	}

	if (OSD_movieheight != movie_height) {
		OSD_movieheight = movie_height;
	  OSD_fontsize = MIN(MAX(13, movie_height / 18), 56);
		render_font(OSD_fontfile, "+1234567890:.", OSD_fontsize, 0);
	  OSD_fonty0 = ST_top;
	  OSD_fonty1 = ST_height;
		minw_smpte = 0;
		render_font(OSD_fontfile, "000000000000000", OSD_fontsize, 0);
		minw_smpte = MAX(minw_smpte, ST_rightend);
		render_font(OSD_fontfile, "00000000000", OSD_fontsize, 0);
		minw_frame = ST_rightend;
		render_font(OSD_fontfile, "0", OSD_fontsize, 0);
		OSD_monospace = ST_rightend;
		if (want_verbose)
			printf("Set Fontsize to %d\n", OSD_fontsize);
	}
	int minw = 0;
	switch (minwh) {
		case MINW__TC:
			minw = -1;
			break;
		case MINWH_NONE:
			minw = 0;
			break;
		case MINWH_SYNCTC:
			minw = minw_smpte;
			break;
		case MINWH_FRAMEN:
			minw = minw_frame;
			break;
	}

	if (render_font(OSD_fontfile, text, OSD_fontsize, minw > 0 ? OSD_monospace : 0)) return;
	ST_rightend = MAX(minw, ST_rightend);

	if (xpos == OSD_LEFT) xalign = ST_PADDING; // left
	else if (xpos == OSD_RIGHT) xalign = movie_width - ST_PADDING - ST_rightend; // right
	else xalign = (movie_width - ST_rightend) / 2; // center

	const int fh = 1 + (minw != 0 ? OSD_fonty1 : ST_height);
	const int fo = ST_HEIGHT - 9 - (minw != 0 ? OSD_fonty0 : ST_top);
	assert(fo > 0);
	assert(fh + fo < ST_HEIGHT);
	yalign = (movie_height - fh) * yperc / 100.0;

	if (!ST_BG) {
		for (y = 0; y < fh && (y + yalign) < movie_height; ++y) {
			for (x = -4; x < 0; ++x) {
				if (x + xalign >= 0)
					_render (mybuffer, &rv, x + xalign, y +yalign, 0);
				if (ST_rightend + xalign - x - 1 < movie_width)
					_render (mybuffer, &rv, ST_rightend + xalign - x - 1, y + yalign, 0);
			}
		}
		for (x = xalign - 4; x < xalign + ST_rightend + 4; ++x) {
			if (x < 0 || x >= movie_width)
				continue;
			for (y = 0; y < 4; ++y) {
				if (yalign - 1 - y >= 0 && yalign - y < movie_height)
					_render (mybuffer, &rv, x, yalign - y, 0);
				if (yalign + fh + y >= 0 && yalign + fh + y < movie_height)
					_render (mybuffer, &rv, x, y + fh + yalign, 0);
			}
		}
	}
	for (y = 0; y < fh && (y + yalign) < movie_height; ++y) {
		for (x = 0; x < ST_rightend && (x + xalign) < movie_width; ++x) {
			if (ST_image[y + fo][x] >= ST_BG) {
				if (x + xalign >= 0)
					_render (mybuffer, &rv, x + xalign, y + yalign, ST_image[y + fo][x]);
			}
		}
	}
}

#include "icons/xjadeoH.h"

void splash (uint8_t *mybuffer) {
	if (want_nosplash) return;
	if (movie_width >= xjadeo_splash_height && movie_height >= xjadeo_splash_width)
		OSD_cmap (VO[VOutput].render_fmt, mybuffer, 50, 0,
				xjadeo_splash_width, xjadeo_splash_height, xjadeo_splash, xjadeo_splash_cmap);
}

static void update_smptestring () {
	if (smpte_offset) free (smpte_offset);
	smpte_offset = calloc(15, sizeof(char));
	frame_to_smptestring (smpte_offset, ts_offset, 0);
}

/*******************************************************************************
 *
 * display wrapper functions
 */

#if (defined HAVE_LIBXV || defined HAVE_IMLIB2)
#include "icons/osd_bitmaps.h"
#endif

#define OBM(NAME, YPOS) \
	OSD_bitmap(VO[VOutput].render_fmt, mybuffer, YPOS, 0, osd_##NAME##_width, osd_##NAME##_height, osd_##NAME##_bits, osd_##NAME##_mask_bits);

void render_buffer (uint8_t *mybuffer) {
	if (!mybuffer) return;

	// render OSD on buffer
	if (OSD_mode & (OSD_FRAME | OSD_VTC))
		OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_frame, OSD_fx, OSD_fy, MINWH_FRAMEN);
	if (OSD_mode & OSD_SMPTE)
		OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_smpte, OSD_sx, OSD_sy, MINWH_SYNCTC);

	if (!splashed) {
		; // keep center free
	}
#if (HAVE_LIBXV || HAVE_IMLIB2)
	else if (OSD_mode & OSD_EQ) {
		int v0, v1, v2, v3, v4;
		if (xj_get_eq("brightness", &v0)) v0 = 0;
		if (xj_get_eq("contrast", &v1)) v1 = 0;
		if (xj_get_eq("gamma", &v2)) v2 = 0;
		if (xj_get_eq("saturation", &v3)) v3 = 0;
		if (xj_get_eq("hue", &v4)) v4 = 0;
		OBM(brightness, 3);
		OBM(contrast, 23);
		OBM(gamma, 43);
		OBM(saturation, 63);
		OSD_bar (VO[VOutput].render_fmt, mybuffer, 10, -1000.0, 1000.0, v0, 0.0);
		OSD_bar (VO[VOutput].render_fmt, mybuffer, 30, -1000.0, 1000.0, v1, (VOutput == VO_XV) ? -500.0 : 0.0);
		OSD_bar (VO[VOutput].render_fmt, mybuffer, 50, -1000.0, 1000.0, v2, 0.0);
		OSD_bar (VO[VOutput].render_fmt, mybuffer, 70, -1000.0, 1000.0, v3, 0.0);
		OSD_bar (VO[VOutput].render_fmt, mybuffer, 90, -1000.0, 1000.0, v4, 0.0);
	} else
#endif
	{
		if (OSD_mode & OSD_TEXT)
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_text, OSD_tx, OSD_ty, MINWH_NONE);
		if (OSD_mode & OSD_MSG)
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_msg, 50, 85, MINWH_NONE);

		if (index_progress >= 0 && index_progress <= 100 && movie_height >= OSD_MIN_NFO_HEIGHT) {
			OSD_bar (VO[VOutput].render_fmt, mybuffer, 91, 0, 100.0, index_progress, -1);
		}

		if (OSD_mode & OSD_OFFF) {
			char tempoff[30];
			snprintf(tempoff, 30, "O:  %"PRId64, ts_offset);
			OSD_render (VO[VOutput].render_fmt, mybuffer, tempoff, OSD_CENTER, 50, MINW__TC);
		} else if (OSD_mode&OSD_OFFS) {
			char tempsmpte[30];
			strcpy(tempsmpte, "O: ");
			if (frame_to_smptestring(tempsmpte+3, ts_offset, 1)) {
				strcat(tempsmpte, " +d");
			}
			OSD_render (VO[VOutput].render_fmt, mybuffer, tempsmpte, OSD_CENTER, 50, MINWH_SYNCTC);
		} else if (OSD_mode & (OSD_NFO | OSD_IDXNFO) && movie_height >= OSD_MIN_NFO_HEIGHT) {
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_tme[0], OSD_CENTER, 31, MINWH_NONE);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_tme[1], OSD_CENTER, 41, MINW__TC);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_tme[2], OSD_CENTER, 50, MINWH_SYNCTC);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_tme[3], OSD_CENTER, 59, MINWH_SYNCTC);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_tme[4], OSD_CENTER, 68, MINWH_SYNCTC);
		} else if (OSD_mode & (OSD_GEO) && movie_height >= OSD_MIN_NFO_HEIGHT) {
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_geo[0], OSD_CENTER, 31, MINWH_NONE);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_geo[1], OSD_CENTER, 41, MINWH_NONE);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_geo[2], OSD_CENTER, 50, MINWH_NONE);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_geo[3], OSD_CENTER, 59, MINWH_NONE);
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_nfo_geo[4], OSD_CENTER, 68, MINWH_NONE);
		}
	}
	if (OSD_mode & OSD_POS && index_progress < 0 && frames > 1 && movie_height >= OSD_MIN_NFO_HEIGHT) {
		int sbox = osd_seeking ? -3 : -2;
		if (ui_syncsource() != SYNC_NONE || (interaction_override&OVR_MENUSYNC)) {
			sbox = -1;
		}
		OSD_bar (VO[VOutput].render_fmt, mybuffer, 100. * BAR_Y, 0, frames - 1, dispFrame, sbox);
	}

	VO[VOutput].render(buffer); // buffer = mybuffer (so far no share mem or sth)
}

void open_window(void) {
	if (want_verbose)
		printf("Video output: %s\n", VO[VOutput].name);
	if (VO[VOutput].open() ) {
		if (!want_quiet)
			fprintf(stderr, "Could not open video output.\n");
		VOutput = 0;
		loop_run = 0;
	} else loop_run = 1;
}

void close_window(void) {
	int vmode = VOutput;
	VOutput = 0;
	loop_run = 0;
	VO[vmode].close();
}

void handle_X_events (void) {
	VO[VOutput].eventhandler();
}

void newsourcebuffer (void) {
	VO[VOutput].newsrc();
}

int getvidmode (void) {
	return(VOutput);
}

void Xgetpos (int *x, int *y) {
	VO[VOutput].getpos(x, y);
}

void Xgetsize (unsigned int *x, unsigned int *y) {
	VO[VOutput].getsize(x, y);
}

void Xresize (unsigned int x, unsigned int y) {
	VO[VOutput].resize(x, y);
}

void Xontop (int a) {
	VO[VOutput].ontop(a);
}

int Xgetontop (void) {
	return (VO[VOutput].getontop());
}

int Xgetfullscreen (void) {
	return (VO[VOutput].getfullscreen());
}

void Xmousepointer (int a) {
	VO[VOutput].mousepointer(a);
}

int Xgetmousepointer (void) {
	return hide_mouse;
}

int Xgetletterbox (void) {
	return want_letterbox;
}

void Xletterbox (int action) {
	if (action == 2) want_letterbox = !want_letterbox;
	else want_letterbox = action ? 1 : 0;
	VO[VOutput].letterbox_change();
}

void Xfullscreen (int a) {
	VO[VOutput].fullscreen(a);
}

void Xposition (int x, int y) {
	VO[VOutput].position(x, y);
}


void XCresize_percent (float p) {
	const int w = rintf (ffctv_width * p / 100.f);
	const int h = rintf (ffctv_height * p / 100.f);
	Xresize(w, h);
}

void XCresize_aspect (int scale) {
	const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
	unsigned int w, h;
	Xgetsize (&w, &h);

	if (scale < 0 && w > 32 && h > 32)  {
		const float step = sqrtf ((float)h);
		w -= floorf (step * asp_src);
		h -= step;
	} else if (scale > 0) {
		const float step = sqrtf ((float)h);
		w += floorf (step * asp_src);
		h += step;
	}

	if (asp_src < ((float)w / (float)h))
		w = rintf ((float)h * asp_src);
	else
		h = rintf((float)w / asp_src);

	Xresize(w, h);
}

void XCresize_scale (int up) {
	const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
	unsigned int w, h;

	Xgetsize (&w, &h);
	const float step = (float)up * 0.17 * h;
	w += floorf (step * asp_src);
	h += step;
	if (w > 32 && h > 32) {
		Xresize(w, h);
	}
}

void XCtimeoffset (int mode, unsigned int charcode) {
	if ((interaction_override&OVR_AVOFFSET) != 0 ) {
		if (charcode != 0)
			remote_notify(NTY_KEYBOARD, 310, "keypress=%d", charcode);
		return;
	}

	int64_t off = ts_offset;
	switch(mode) {
		case -1:
			--ts_offset;
			break;
		case 1:
			++ts_offset;
			break;
		case -2:
			if (framerate > 0) {
				// TODO drop-frame ??
				ts_offset -= framerate * 60;
			} else {
				ts_offset -= 25*60;
			}
			break;
		case 2:
			if (framerate > 0) {
				// TODO drop-frame ??
				ts_offset += framerate * 60;
			} else {
				ts_offset += 25*60;
			}
			break;
		case -3:
			if (framerate > 0) {
				// TODO drop-frame ??
				ts_offset -= framerate * 3600;
			} else {
				ts_offset -= 25*3600;
			}
			break;
		case 3:
			if (framerate > 0) {
				// TODO drop-frame ??
				ts_offset += framerate * 3600;
			} else {
				ts_offset += 25*3600;
			}
			break;
		default:
			ts_offset = 0;
			break;
	}

	if (off != ts_offset) {
		force_redraw = 1;
		update_smptestring ();
	}
}
