/* xjadeo - simple X11 context menu
 *
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

#ifdef XDLG
#if (defined HAVE_LIBXV || defined HAVE_IMLIB2 || (defined HAVE_GL && !defined PLATFORM_WINDOWS && !defined PLATFORM_OSX))

void xapi_close (void *d);

#include <X11/Xresource.h>

static XContext _dlg_ctx = 0;
static int      _dlg_font_height = 0;
static int      _dlg_font_ascent = 0;
static XColor   _c_gray1, _c_gray2;
static Font     _dlgfont = 0;

static Window   _dlg_mwin = 0;
static Window   _dlg_swin = 0;
static Window   _parent = 0;

struct XjxMenuItem {
	const char *text;
	const char *key;
	struct XjxMenuItem *submenu;
	void (*callback)(void);
	int enabled; // TODO use a callback ?!
	int sensitive;
};

struct XJDialog {
	GC  gc;
	int x0;
	int y0;
	int width;
	int height;
	int menu_hover;
	int menu_count;
	struct XjxMenuItem *menu_items;
};


static int show_submenu(Display *dpy, struct XjxMenuItem *sm, int x, int y, int pw);
static void hide_submenu(Display *dpy);

///////////////////////////////////////////////////////////////////////////////

//static void cb_none ()      { printf("TEST CALLBACK\n"); }

static void ui_scale50()    { XCresize_percent(50); }
static void ui_scale100()   { XCresize_percent(100); }
static void ui_scale150()   { XCresize_percent(150); }
static void ui_scale_inc()  { XCresize_scale( 1); }
static void ui_scale_dec()  { XCresize_scale(-1); }
static void ui_aspect()     { XCresize_aspect(0); }
static void ui_ontop()      { Xontop(2); }
static void ui_letterbox()  { Xletterbox(2); }
static void ui_fullscreen() { Xfullscreen(2); }
static void ui_mousetoggle(){ Xmousepointer(2); }

static void ui_offset_rst() { XCtimeoffset( 0, 0); }
static void ui_offset_pf()  { XCtimeoffset( 1, 0); }
static void ui_offset_mf()  { XCtimeoffset(-1, 0); }
static void ui_offset_pm()  { XCtimeoffset( 2, 0); }
static void ui_offset_mm()  { XCtimeoffset(-2, 0); }
static void ui_offset_ph()  { XCtimeoffset( 3, 0); }
static void ui_offset_mh()  { XCtimeoffset(-3, 0); }

static void ui_quit() {
	loop_flag=0;
}

static void ui_open_file() {
	;
}

static void ui_close_file() {
	if (interaction_override&OVR_LOADFILE) return;
	xapi_close(NULL);
}

/* unlisted key-shorcuts
 * - Esc (quit)
 * - e, E, 0-9 , Shift + 1-4 (color EQ for xv/imlib)
 * - [, ] (pan/crop -- ifdef'ed)
 */
static struct XjxMenuItem submenu_sync[] = {
	{"JACK",           "", NULL, &ui_sync_to_jack, 0, 1},
	{"LTC",            "", NULL, &ui_sync_to_ltc, 0, 1},
	{"MTC (JACK)",     "", NULL, &ui_sync_to_mtc_jack, 0, 1},
	{"MTC (PortMidi)", "", NULL, &ui_sync_to_mtc_portmidi, 0, 1},
	{"MTC (ALSA Seq)", "", NULL, &ui_sync_to_mtc_alsaseq, 0, 1},
	{"MTC (ALSA Raw)", "", NULL, &ui_sync_to_mtc_alsaraw, 0, 1},
	{"None",           "", NULL, &ui_sync_none, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0},
};

static struct XjxMenuItem submenu_size[] = {
	{"50%",           "",  NULL, &ui_scale50, 0, 1},
	{"100%",          ".", NULL, &ui_scale100, 0, 1},
	{"150%",          "",  NULL, &ui_scale150, 0, 1},
	{"",              "",  NULL, NULL, 0, 0},
	{"-20%",          "<", NULL, &ui_scale_dec, 0, 1},
	{"+20%",          ">", NULL, &ui_scale_inc, 0, 1},
	{"",              "",  NULL, NULL, 0, 0},
	{"Reset Aspect",  ",", NULL, &ui_aspect, 0, 1},
	{"Retain Aspect (Letterbox)",    "L", NULL, &ui_letterbox, 0, 1},
	{"",              "",  NULL, NULL, 0, 0},
	{"Window On Top", "A", NULL, &ui_ontop, 0, 1},
	{"Fullscreen",    "F", NULL, &ui_fullscreen, 0, 1},
	{"",              "",  NULL, NULL, 0, 0},
	{"Mouse Cursor",  "M", NULL, &ui_mousetoggle, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0},
};

static struct XjxMenuItem submenu_osd[] = {
	{"External Timecode",   "S", NULL, &ui_osd_tc, 0, 1},
	{"",                    "",  NULL, NULL, 0, 0},
	{"VTC Off",             "V", NULL, &ui_osd_vtc_off, 0, 1},
	{"VTC Timecode",        "",  NULL, &ui_osd_vtc_tc, 0, 1},
	{"VTC Frame Number",    "",  NULL, &ui_osd_vtc_fn, 0, 1},
	{"",                    "",  NULL, NULL, 0, 0},
	{"Offset Off",          "O", NULL, &ui_osd_offset_none, 0, 1},
	{"Offset Timecode",     "",  NULL, &ui_osd_offset_tc, 0, 1},
	{"Offset Frame Number", "",  NULL, &ui_osd_offset_fn, 0, 1},
	{"",                    "",  NULL, NULL, 0, 0},
	{"Time Info",           "I", NULL, &ui_osd_fileinfo, 0, 1},
	{"Geometry",            "G", NULL, &ui_osd_geo, 0, 1},
	{"",                    "",  NULL, NULL, 0, 0},
	{"Background",          "B", NULL, &ui_osd_box, 0, 1},
	{"Swap Position",       "P", NULL, &ui_osd_permute, 0, 1},
	{"",                    "",  NULL, NULL, 0, 0},
	{"Clear All",     "Shift+C", NULL, &ui_osd_clear, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0},
};

static struct XjxMenuItem submenu_offs[] = {
	{"Reset",     "\\", NULL, &ui_offset_rst, 0, 1},
	{"+ 1 Frame",  "+", NULL, &ui_offset_pf, 0, 1},
	{"- 1 Frame",  "-", NULL, &ui_offset_mf, 0, 1},
	{"+ 1 Minute", "{", NULL, &ui_offset_pm, 0, 1},
	{"- 1 Minute", "}", NULL, &ui_offset_mm, 0, 1},
	{"+ 1 Hour",   "",  NULL, &ui_offset_ph, 0, 1},
	{"- 1 Hour",   "",  NULL, &ui_offset_mh, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0},
};

static struct XjxMenuItem submenu_jack[] = {
	{"Play/Pause", "Space",     NULL, &jackt_toggle, 0, 1},
	{"Play", "",                NULL, &jackt_start, 0, 1},
	{"Stop", "",                NULL, &jackt_stop, 0, 1},
	{"Rewind",     "Backspace", NULL, &jackt_rewind, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0},
};

static struct XjxMenuItem submenu_file[] = {
	{"Open Video",         "Ctrl+O", NULL, &ui_open_file, 0, 1},
	{"Close Video",        "Ctrl+W", NULL, &ui_close_file, 0, 1},
	{"",                   "",       NULL, NULL, 0, 0},
	{"(Drag&Drop File on", "",       NULL, NULL, 0, 1},
	{" Window to Load)",   "",       NULL, NULL, 0, 1},
	{"",                   "",       NULL, NULL, 0, 0},
	{"Quit",               "Ctrl+Q", NULL, &ui_quit, 0, 1},
};

static struct XjxMenuItem mainmenu[] = {
	{"XJadeo " VERSION,    "", NULL, NULL, 0, 1},
	{"",                   "", NULL, NULL, 0, 0},
	{"File",               "", submenu_file, NULL, 0, 1},
	{"Sync",               "", submenu_sync, NULL, 0, 1},
	{"Display",            "", submenu_size, NULL, 0, 1},
	{"OSD",                "", submenu_osd,  NULL, 0, 1},
	{"Offset"   ,          "", submenu_offs, NULL, 0, 1},
	{"Transport",          "", submenu_jack, NULL, 0, 1},
	{NULL, NULL, NULL, NULL, 0, 0},
};

#define CLEARMENU(mnu) \
	for (i = 0; mnu[i].text; ++i) { mnu[i].enabled = 0; }

static void update_menus () {
	int i;
	CLEARMENU(mainmenu);
	CLEARMENU(submenu_sync);
	CLEARMENU(submenu_size);
	CLEARMENU(submenu_osd);
	CLEARMENU(submenu_jack);

#ifdef HAVE_LTC
	submenu_sync[1].sensitive = 1;
#else
	submenu_sync[1].sensitive = 0;
#endif
#ifdef HAVE_JACKMIDI
	submenu_sync[2].sensitive = 1;
#else
	submenu_sync[2].sensitive = 0;
#endif
#ifdef HAVE_PORTMIDI
	submenu_sync[3].sensitive = 1;
#else
	submenu_sync[3].sensitive = 0;
#endif
#ifdef ALSA_SEQ_MIDI
	submenu_sync[4].sensitive = 1;
#else
	submenu_sync[4].sensitive = 0;
#endif
#ifdef ALSA_RAW_MIDI
	submenu_sync[5].sensitive = 1;
#else
	submenu_sync[5].sensitive = 0;
#endif

	submenu_sync[ui_syncsource()].enabled = 1;

	if (OSD_mode&OSD_SMPTE) {
		submenu_osd[0].enabled = 1;
	}
	if (!(OSD_mode&(OSD_FRAME|OSD_VTC))) {
		submenu_osd[2].enabled = 1;
	}
	if (OSD_mode&OSD_VTC) {
		submenu_osd[3].enabled = 1;
	}
	if (OSD_mode&OSD_FRAME) {
		submenu_osd[4].enabled = 1;
	}
	if (!(OSD_mode&(OSD_OFFF|OSD_OFFS))) {
		submenu_osd[6].enabled = 1;
	}
	if (OSD_mode&OSD_OFFS) {
		submenu_osd[7].enabled = 1;
	}
	if (OSD_mode&OSD_OFFF) {
		submenu_osd[8].enabled = 1;
	}
	if (OSD_mode&OSD_NFO) {
		submenu_osd[10].enabled = 1;
	}
	if (OSD_mode&OSD_GEO) {
		submenu_osd[11].enabled = 1;
	}
	if (movie_height < OSD_MIN_NFO_HEIGHT) {
		submenu_osd[10].sensitive = 0;
		submenu_osd[11].sensitive = 0;
	} else {
		submenu_osd[10].sensitive = 1;
		submenu_osd[11].sensitive = 1;
	}
	if (OSD_mode&OSD_BOX) {
		submenu_osd[13].enabled = 1;
	}

	if (Xgetletterbox()) {
		submenu_size[8].enabled = 1;
	}
	if (Xgetontop()) {
		submenu_size[10].enabled = 1;
	}
	if (Xgetfullscreen()) {
		submenu_size[11].enabled = 1;
	}
	if (!Xgetmousepointer()) {
		submenu_size[13].enabled = 1;
	}

	if ((interaction_override&OVR_AVOFFSET) != 0 )
	{
		mainmenu[6].sensitive = 0;
	} else {
		mainmenu[6].sensitive = 1;
	}
	if (ui_syncsource() == SYNC_JACK && !(interaction_override&OVR_JCONTROL))
	{
		mainmenu[7].sensitive = 1;
	} else {
		mainmenu[7].sensitive = 0;
	}

	if (interaction_override & OVR_MENUSYNC) {
		mainmenu[3].sensitive = 0;
	} else {
		mainmenu[3].sensitive = 1;
	}

	if (interaction_override & OVR_LOADFILE) {
		submenu_file[0].sensitive = 0;
		submenu_file[1].sensitive = 0;
		submenu_file[3].sensitive = 0;
		submenu_file[4].sensitive = 0;
	} else {
		submenu_file[0].sensitive = 1;
		submenu_file[1].sensitive = have_open_file() ? 1 : 0;
		submenu_file[3].sensitive = 1;
		submenu_file[4].sensitive = 1;
	}
#ifndef XFIB
	submenu_file[0].sensitive = 0;
#endif
	if (interaction_override & OVR_QUIT_WMG) {
		submenu_file[6].sensitive = 0;
	} else {
		submenu_file[6].sensitive = 1;
	}
}

///////////////////////////////////////////////////////////////////////////////

static int query_font_geometry (Display *dpy, GC gc, const char *txt, int *w, int *h, int *a, int *d) {
	XCharStruct text_structure;
	int font_direction, font_ascent, font_descent;
	XFontStruct *fontinfo = XQueryFont (dpy, XGContextFromGC (gc));

	if (!fontinfo) { return -1; }
	XTextExtents (fontinfo, txt, strlen (txt), &font_direction, &font_ascent, &font_descent, &text_structure);
	if (w) *w = XTextWidth (fontinfo, txt, strlen (txt));
	if (h) *h = text_structure.ascent + text_structure.descent;
	if (a) *a = text_structure.ascent;
	if (d) *d = text_structure.descent;
	XFreeFontInfo (NULL, fontinfo, 1);
	return 0;
}

static void close_x_dialog_win (Display *dpy, Window *win) {
	struct XJDialog *dlg = NULL;
	if (!win || !*win) return;
	XFindContext (dpy, *win, _dlg_ctx, (XPointer*)&dlg);
	if (dlg) {
		XDeleteContext (dpy, *win, _dlg_ctx);
		XFreeGC (dpy, dlg->gc);
		free(dlg);
	}
	XDestroyWindow (dpy, *win);
	*win = 0;
}

static uint8_t font_err = 0;
static int x_error_handler(Display *d, XErrorEvent *e) {
	font_err = 1;
	return 0;
}

static int open_x_dialog_win (
		Display *dpy, Window *win,
		int x, int y, int pw,
		struct XjxMenuItem *menu, const int m_items
		)
{
	if (!dpy) return -1;
	if (!win) return -1;
	if (*win) return -1;

	XSetWindowAttributes attr;
	memset (&attr, 0, sizeof(XSetWindowAttributes));
	attr.override_redirect = True;
	attr.save_under = True;
	attr.border_pixel = _c_gray2.pixel;

	attr.event_mask = ExposureMask | KeyPressMask
		| ButtonPressMask | ButtonReleaseMask
		| ConfigureNotify | StructureNotifyMask
		| PointerMotionMask | LeaveWindowMask;

	int dlg_width  = 100;
	int dlg_height = 100;

	*win = XCreateWindow (
		dpy, DefaultRootWindow (dpy),
		x, y, dlg_width, dlg_height,
		1,  CopyFromParent, InputOutput, CopyFromParent,
		CWOverrideRedirect | CWSaveUnder | CWEventMask | CWBorderPixel, &attr);

	if (!*win) { return 1; }

	GC dlg_gc = XCreateGC (dpy, *win, 0, NULL);

	int (*handler)(Display *, XErrorEvent *) = XSetErrorHandler(&x_error_handler);
#define _XTESTFONT(FN) \
	{ \
		font_err = 0; \
		_dlgfont = XLoadFont(dpy, FN); \
		XSetFont(dpy, dlg_gc, _dlgfont); \
		XSync (dpy, False); \
	}

	font_err = 1;
	if (getenv("XJFONT")) _XTESTFONT (getenv("XJFONT"));
	if (font_err) _XTESTFONT ("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");
	if (font_err) _XTESTFONT ("-*-verdana-medium-r-normal-*-12-*-*-*-*-*-*-*");
	if (font_err) _XTESTFONT ("-misc-fixed-medium-r-normal-*-13-*-*-*-*-*-*-*");
	if (font_err) _XTESTFONT ("-misc-fixed-medium-r-normal-*-12-*-*-*-*-*-*-*");
	if (font_err) _dlgfont = None;
	XSync (dpy, False);
	XSetErrorHandler(handler);

	if (_dlg_font_height == 0) { // 1st time only
		if (query_font_geometry (dpy, dlg_gc, "|0Yy", NULL, &_dlg_font_height, &_dlg_font_ascent, NULL)) {
			XFreeGC (dpy, dlg_gc);
			XDestroyWindow (dpy, *win);
			*win = 0;
			return -1;
		}
		_dlg_font_height +=2;
		_dlg_font_ascent +=1;
	}

	dlg_height = (_dlg_font_height + 2) * (m_items + .5);

	int i;
	int max_w = 80;
	for(i = 0; i < m_items; ++i) {
		int cw;
		if (!query_font_geometry(dpy, dlg_gc, menu[i].text, &cw, NULL, NULL, NULL)) {
			cw += 4;
			if (menu[i].key && strlen(menu[i].key) > 0) {
				int ks = 0;
				query_font_geometry(dpy, dlg_gc, menu[i].key, &ks, NULL, NULL, NULL);
				cw += ks + 10;
			}
			else if (menu[i].submenu) {
				cw += 10;
			}
			if (cw > max_w) max_w = cw;
		}
	}
	dlg_width = max_w + 20;
	XResizeWindow (dpy, *win, dlg_width, dlg_height);

	XWindowAttributes wa;
	XGetWindowAttributes (dpy, *win, &wa);
	if (wa.screen) {
		int sc_w = WidthOfScreen (wa.screen) - dlg_width - 5;
		int sc_h = HeightOfScreen (wa.screen) - dlg_height - 5;
		if (x > sc_w) x = sc_w - pw;
		if (y > sc_h) y = y + _dlg_font_height - 2 - dlg_height;
		XMoveWindow (dpy, *win, x, y);
	}

	//NB. this is free()ed on close by XFindContext() reference.
	struct XJDialog *dlg = malloc(sizeof(struct XJDialog));
	dlg->gc = dlg_gc;
	dlg->x0 = x;
	dlg->y0 = y;
	dlg->width = dlg_width;
	dlg->height = dlg_height;
	dlg->menu_hover = -1;
	dlg->menu_count = m_items;
	dlg->menu_items = menu;

	XSaveContext(dpy, *win, _dlg_ctx, (XPointer)dlg);
	XMapRaised (dpy, *win);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

static void dialog_expose (Display *dpy, Window win) {
	struct XJDialog *dlg = NULL;
	XFindContext (dpy, win, _dlg_ctx, (XPointer*)&dlg);
	if (!dlg) return;

	const unsigned long whiteColor = WhitePixel (dpy, DefaultScreen (dpy));
	const unsigned long blackColor = BlackPixel (dpy, DefaultScreen (dpy));

	XSetForeground (dpy, dlg->gc, _c_gray1.pixel);
	XFillRectangle (dpy, win, dlg->gc, 0, 0, dlg->width, dlg->height);

	int i;
	for (i=0; i < dlg->menu_count; ++i) {
		int t_x = 15;
		int t_y = (i+1) * (_dlg_font_height + 2);
		if (strlen (dlg->menu_items[i].text) == 0) {
			t_y -= _dlg_font_ascent * .5;
			XSetForeground (dpy, dlg->gc, _c_gray2.pixel);
			XDrawLine (dpy, win, dlg->gc, 5, t_y, dlg->width - 6, t_y);
			continue; // separator
		}

		if (!dlg->menu_items[i].callback && !dlg->menu_items[i].submenu) {
			// center align headings
			int t_w = 0;
			query_font_geometry (dpy, dlg->gc, dlg->menu_items[i].text, &t_w, NULL, NULL, NULL);
			t_x = (dlg->width - t_w) * .5;
		}

		if (dlg->menu_hover == i && dlg->menu_items[i].sensitive
				&& (dlg->menu_items[i].callback || dlg->menu_items[i].submenu))
		{
			XSetForeground (dpy, dlg->gc, blackColor);
			XFillRectangle (dpy, win, dlg->gc, 2, t_y - _dlg_font_ascent, dlg->width - 4, _dlg_font_height);
			XSetForeground (dpy, dlg->gc, whiteColor);
		}
		else if (!dlg->menu_items[i].sensitive)
		{
			XSetForeground (dpy, dlg->gc, _c_gray2.pixel);
		}
		else
		{
			XSetForeground (dpy, dlg->gc, blackColor);
		}

		XDrawString (dpy, win, dlg->gc, t_x, t_y, dlg->menu_items[i].text, strlen (dlg->menu_items[i].text));
		if (dlg->menu_items[i].key && strlen(dlg->menu_items[i].key) > 0) {
			int ks = 10;
			if (strlen(dlg->menu_items[i].key) > 0) {
				query_font_geometry(dpy, dlg->gc, dlg->menu_items[i].key, &ks, NULL, NULL, NULL);
				ks += 5;
			}
			XDrawString (dpy, win, dlg->gc, dlg->width - ks, t_y, dlg->menu_items[i].key, strlen (dlg->menu_items[i].key));
		}
		if (dlg->menu_items[i].enabled) {
			XFillArc (dpy, win, dlg->gc, 5, t_y - _dlg_font_ascent * .5 - 3, 7, 7, 0, 360*64);
		}
		if (dlg->menu_items[i].submenu) {
			XPoint pts[3] = { {dlg->width - 5, t_y - _dlg_font_ascent * .5 + 1}, {-4, -4}, {0, 8}};
			XFillPolygon (dpy, win, dlg->gc, pts, 3, Convex, CoordModePrevious);
		}
	}
	XFlush (dpy);
}

static void dialog_motion (Display *dpy, Window win, int x, int y) {
	struct XJDialog *dlg = NULL;
	XFindContext (dpy, win, _dlg_ctx, (XPointer*)&dlg);
	if (!dlg) return;

	x -= dlg->x0;
	y -= dlg->y0;

	int am;
	if (x <= 0 || y <= 0 || x >= dlg->width || y >= dlg->height) {
		am = -1;
	} else {
		am = y / (_dlg_font_height + 2); // TODO skip hidden 
		if (am < 0 || am >= dlg->menu_count
				|| !(dlg->menu_items[am].callback || dlg->menu_items[am].submenu)
				|| !(dlg->menu_items[am].sensitive)
			 )
		{
			am = -1;
		}
	}
#define HAS_SUBMENU(i) (i >= 0 && i <= dlg->menu_count && dlg->menu_items[i].submenu)

	if (am != dlg->menu_hover) {
		if (am == -1 && HAS_SUBMENU(dlg->menu_hover)) {
			; // keep submenu
		} else {
			dlg->menu_hover = am;
			if (win == _dlg_mwin) { // TOP LEVEL ONLY, not self
				if (HAS_SUBMENU(dlg->menu_hover)) {
					show_submenu(dpy, dlg->menu_items[dlg->menu_hover].submenu,
							dlg->x0, dlg->y0 + (am + .5) * (_dlg_font_height + 2), dlg->width - 1);
				} else {
					hide_submenu(dpy);
				}
			}
			dialog_expose (dpy, win);
		}
	}
}

static int dialog_click (Display *dpy, Window win, int x, int y, int b) {
	struct XJDialog *dlg = NULL;
	XFindContext (dpy, win, _dlg_ctx, (XPointer*)&dlg);
	if (!dlg) return -1;

	x -= dlg->x0;
	y -= dlg->y0;

	if (x <= 0 || y <= 0 || x >= dlg->width || y >= dlg->height) {
		return -1;
	}
	if (b != 1) return 0;

	if (dlg->menu_hover >= 0 && dlg->menu_hover < dlg->menu_count) {
		if (dlg->menu_items[dlg->menu_hover].callback && dlg->menu_items[dlg->menu_hover].sensitive) {
			dlg->menu_items[dlg->menu_hover].callback();
			close_x_dialog(dpy);
#ifdef XFIB
			if (dlg->menu_items[dlg->menu_hover].callback == &ui_open_file) {
				if (!(interaction_override&OVR_LOADFILE)) {
					x_fib_cfg_filter_callback(fib_filter_movie_filename);
					x_fib_show (dpy, _parent, 0, 0);
				}
			}
#endif
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

static int show_submenu(Display *dpy, struct XjxMenuItem *sm, int x, int y, int pw) {
	assert(_dlg_mwin);
	close_x_dialog_win(dpy, &_dlg_swin);

	x += pw;
	int items; for (items = 0; sm[items].text; ++items);
	return open_x_dialog_win(dpy, &_dlg_swin, x, y, pw, sm, items);
}

static void hide_submenu(Display *dpy) {
	assert(_dlg_mwin);
	close_x_dialog_win(dpy, &_dlg_swin);
}

int show_x_dialog (Display *dpy, Window parent, int x, int y) {
	if (_dlg_mwin) return -1;
	if (!_dlg_ctx) {
		_dlg_ctx = XUniqueContext();
	}
	_parent = parent;

	XColor dummy;
	Colormap colormap = DefaultColormap (dpy, DefaultScreen (dpy));
	if (!XAllocNamedColor (dpy, colormap, "LightGray", &_c_gray1, &dummy)) return -1;
	if (!XAllocNamedColor (dpy, colormap, "DarkGray", &_c_gray2, &dummy)) return -1;

	update_menus();

	int items; for (items = 0; mainmenu[items].text; ++items);
	if (open_x_dialog_win(dpy, &_dlg_mwin, x, y, 0, mainmenu, items)) {
		return -1;
	}
#if 1
	XGrabPointer (dpy, _dlg_mwin, True,
			ButtonReleaseMask | ButtonPressMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | StructureNotifyMask,
			GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XGrabKeyboard (dpy, _dlg_mwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	//XSetInputFocus (dpy, parent, RevertToNone, CurrentTime);
#endif
	return 0;
}

void close_x_dialog (Display *dpy) {
	if (!_dlg_mwin) {
		assert(!_dlg_swin);
		return;
	}
	XUngrabPointer (dpy, CurrentTime);
	XUngrabKeyboard (dpy, CurrentTime);
	XSync (dpy, False);

	close_x_dialog_win(dpy, &_dlg_mwin);
	close_x_dialog_win(dpy, &_dlg_swin);

	Colormap colormap = DefaultColormap (dpy, DefaultScreen (dpy));
	XFreeColors (dpy, colormap, &_c_gray1.pixel, 1, 0);
	XFreeColors (dpy, colormap, &_c_gray2.pixel, 1, 0);
	if (_dlgfont != None) XUnloadFont(dpy, _dlgfont);
	_dlgfont = None;
	force_redraw = 1;
}


int handle_xdlg_event (Display *dpy, XEvent *event) {
	if (!_dlg_mwin) return 0;
	assert(event->xany.window);
	if (event->xany.window != _dlg_mwin && event->xany.window != _dlg_swin) {
		switch (event->type) {
			case ButtonRelease:
			case KeyRelease:
			close_x_dialog (dpy);
				break;
			default:
				break;
		}
		return 0;
	}

	switch (event->type) {
		case ConfigureNotify:
			//printf("DLG ConfigureNotify %dx%d\n", event->xconfigure.width, event->xconfigure.height);
			break;
		case LeaveNotify:
			{
				struct XJDialog *dlg = NULL;
				XFindContext (dpy, event->xany.window, _dlg_ctx, (XPointer*)&dlg);
				if (dlg
						&& dlg->menu_hover >= 0 && dlg->menu_hover < dlg->menu_count
						&& !dlg->menu_items[dlg->menu_hover].submenu)
				{
					dlg->menu_hover = -1;
					dialog_expose (dpy, event->xany.window);
				}
			}
			break;
		case Expose:
			if (event->xexpose.count == 0) {
				dialog_expose (dpy, event->xany.window);
			}
			break;
		case MotionNotify:
			dialog_motion (dpy, event->xany.window, event->xmotion.x_root, event->xmotion.y_root);
			if (event->xmotion.is_hint == NotifyHint) {
				XGetMotionEvents (dpy, event->xany.window, CurrentTime, CurrentTime, NULL);
			}
			break;
		case ButtonPress:
			break;
		case ButtonRelease:
			if (dialog_click(dpy, event->xany.window, event->xbutton.x_root, event->xbutton.y_root, event->xbutton.button)) {
				close_x_dialog (dpy);
			}
			break;
		case KeyRelease:
			close_x_dialog (dpy);
			break;
	}
	return 1;
}

#endif // platform
#endif // XDLG
