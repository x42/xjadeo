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
extern int seekflags;

static Window _dlg_win = 0;
static GC _dlg_gc = 0;
static int _dlg_width = 120;
static int _dlg_height = 150;
static int _dlg_font_height = 12;
static int _dlg_font_ascent = 10;

static int menu_hover = -1;
static XColor _c_gray1, _c_gray2;

struct XjxMenuItem {
	char text[32];
	void (*callback)(void);
	int enabled; // TODO use a callback
	int sensitive;
};

static void cb_none () {
	printf("CALLBACK\n");
}

// TODO dynamically build menu.
static struct XjxMenuItem menuitems[] = {
	{"Sync", NULL, 0, 0},
	{"Jack", &ui_sync_to_jack, 0, 1},
	{"LTC", &ui_sync_to_ltc, 0, 1},
	{"MTC (JACK)", &ui_sync_to_mtc_jack, 0, 1},
	{"MTC (PortMidi)", &ui_sync_to_mtc_portmidi, 0, 1},
	{"MTC (ALSA Seq)", &ui_sync_to_mtc_alsaseq, 0, 1},
	{"MTC (ALSA Raw)", &ui_sync_to_mtc_alsaraw, 0, 1},
	{"", NULL, 0, 0},
	{"Seek", NULL, 0, 0},
	{"Key Frames Only", &ui_seek_key, 0, 1},
	{"Any Fame", &ui_seek_any, 0, 1},
	{"PTS", &ui_seek_cont, 1, 1},
	{"", NULL, 0, 0},
	{"(Drag&Drop File on", NULL, 0, 0},
	{" Window to Load)", NULL, 0, 0},
};

// TODO dynamically build menu. check OVR_*
static void update_menu() {
	const int items = sizeof(menuitems) / sizeof(struct XjxMenuItem);
	int i;
	for(i = 0; i < items; ++i) {
		menuitems[i].enabled = 0;
	}

#ifdef HAVE_LTC
	menuitems[2].sensitive = 1;
#else
	menuitems[2].sensitive = 0;
#endif
#ifdef HAVE_JACKMIDI
	menuitems[3].sensitive = 1;
#else
	menuitems[3].sensitive = 0;
#endif
#ifdef HAVE_PORTMIDI
	menuitems[4].sensitive = 1;
#else
	menuitems[4].sensitive = 0;
#endif
#ifdef ALSA_SEQ_MIDI
	menuitems[5].sensitive = 1;
#else
	menuitems[5].sensitive = 0;
#endif
#ifdef ALSA_RAW_MIDI
	menuitems[6].sensitive = 1;
#else
	menuitems[6].sensitive = 0;
#endif

	switch (ui_syncsource()) {
		case SYNC_JACK:
			menuitems[1].enabled = 1;
			break;
		case SYNC_LTC:
			menuitems[2].enabled = 1;
			break;
		case SYNC_MTC_JACK:
			menuitems[3].enabled = 1;
			break;
		case SYNC_MTC_PORTMIDI:
			menuitems[4].enabled = 1;
			break;
		case SYNC_MTC_ALSASEQ:
			menuitems[5].enabled = 1;
			break;
		case SYNC_MTC_ALSARAW:
			menuitems[6].enabled = 1;
			break;
		case SYNC_NONE:
			break;
	}

#if 0
	menuitems[9].sensitive = 0;
	menuitems[10].sensitive = 0;
	menuitems[11].sensitive = 0;
#endif

	if (seekflags == SEEK_KEY) {
		menuitems[9].enabled = 1;
	}
	else if (seekflags == SEEK_ANY) {
		menuitems[10].enabled = 1;
	}
	else {
		menuitems[11].enabled = 1;
	}
}

static int query_font_geometry (Display *dpy, const char *txt, int *w, int *h, int *a, int *d) {
	XCharStruct text_structure;
	int font_direction, font_ascent, font_descent;
	XFontStruct *fontinfo = XQueryFont (dpy, XGContextFromGC (_dlg_gc));

	if (!fontinfo) { return -1; }
	XTextExtents (fontinfo, txt, strlen (txt), &font_direction, &font_ascent, &font_descent, &text_structure);
	if (w) *w = XTextWidth (fontinfo, txt, strlen (txt));
	if (h) *h = text_structure.ascent + text_structure.descent;
	if (a) *a = text_structure.ascent;
	if (d) *d = text_structure.descent;
	XFreeFontInfo (NULL, fontinfo, 0);
	return 0;
}

static void dialog_expose (Display *dpy) {
	unsigned long whiteColor = WhitePixel (dpy, DefaultScreen (dpy));
	unsigned long blackColor = BlackPixel (dpy, DefaultScreen (dpy));

	XSetForeground (dpy, _dlg_gc, _c_gray1.pixel);
	XFillRectangle (dpy, _dlg_win, _dlg_gc, 0, 0, _dlg_width, _dlg_height);

	XFontStruct *fontinfo = XQueryFont (dpy, XGContextFromGC (_dlg_gc));
	if (!fontinfo) {
		return;
	}

	const int items = sizeof(menuitems) / sizeof(struct XjxMenuItem);
	int i;

	for (i=0; i < items; ++i) {
		int t_x = 15;
		int t_y = (i+1) * (_dlg_font_height + 2);
		if (strlen (menuitems[i].text) == 0) {
			t_y -= _dlg_font_ascent * .5;
			XSetForeground (dpy, _dlg_gc, _c_gray2.pixel);
			XDrawLine (dpy, _dlg_win, _dlg_gc, 5, t_y, _dlg_width - 6, t_y);
			continue; // space
		}

		if (!menuitems[i].callback) {
			// center align headings
			int t_w = 0;
			query_font_geometry (dpy, menuitems[i].text, &t_w, NULL, NULL, NULL);
			t_x = (_dlg_width - t_w) * .5;
		}

		if (menu_hover == i && menuitems[i].callback && menuitems[i].sensitive) {
			XSetForeground (dpy, _dlg_gc, blackColor);
			XFillRectangle (dpy, _dlg_win, _dlg_gc, 2, t_y - _dlg_font_ascent, _dlg_width - 4, _dlg_font_height);
			XSetForeground (dpy, _dlg_gc, whiteColor);
		}
		else if (!menuitems[i].sensitive && menuitems[i].callback) {
			XSetForeground (dpy, _dlg_gc, _c_gray2.pixel);
		}
		else {
			XSetForeground (dpy, _dlg_gc, blackColor);
		}

		XDrawString (dpy, _dlg_win, _dlg_gc, t_x, t_y, menuitems[i].text, strlen (menuitems[i].text));
		if (menuitems[i].enabled) {
			XFillArc (dpy, _dlg_win, _dlg_gc, 5, t_y - _dlg_font_ascent * .5 - 3, 7, 7, 0, 360*64);
		}
	}
	XFlush (dpy);
}

static void dialog_motion (Display *dpy, int x, int y) {
	int am = menu_hover;
	if (x < 0 || y < 0) {
		menu_hover = -1;
	} else {
		menu_hover = y / (_dlg_font_height + 2);
		if (!menuitems[menu_hover].callback || !menuitems[menu_hover].sensitive) {
			menu_hover = -1;
		}
	}
	if (am != menu_hover) {
		dialog_expose (dpy);
	}
}

static void dialog_click (Display *dpy, int x, int y) {
	const int items = sizeof(menuitems) / sizeof(struct XjxMenuItem);
	if (menu_hover >= 0 && menu_hover < items) {
		if (menuitems[menu_hover].callback && menuitems[menu_hover].sensitive) {
			menuitems[menu_hover].callback();
			close_x_dialog(dpy);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

int show_x_dialog (Display *dpy, Window parent, int x, int y) {
	if (_dlg_win) return -1;

	XColor dummy;

	Colormap colormap = DefaultColormap (dpy, DefaultScreen (dpy));
	if (!XAllocNamedColor (dpy, colormap, "LightGray", &_c_gray1, &dummy)) return -1;
	if (!XAllocNamedColor (dpy, colormap, "DarkGray", &_c_gray2, &dummy)) return -1;

	XSetWindowAttributes attr;
	memset (&attr, 0, sizeof(XSetWindowAttributes));
	attr.override_redirect = True;
	attr.save_under = True;
	attr.border_pixel = _c_gray2.pixel;

	attr.event_mask = ExposureMask | KeyPressMask
		| ButtonPressMask | ButtonReleaseMask
		| ConfigureNotify | StructureNotifyMask
		| PointerMotionMask ;


	// TODO check position, realign at edge of screen
	_dlg_win = XCreateWindow (
		dpy, DefaultRootWindow (dpy),
		x, y, _dlg_width, _dlg_height,
		1,  CopyFromParent, InputOutput, CopyFromParent,
		CWOverrideRedirect | CWSaveUnder | CWEventMask | CWBorderPixel, &attr);

	if (!_dlg_win) {
		return 1;
	}

	_dlg_gc = XCreateGC (dpy, _dlg_win, 0, NULL);
	query_font_geometry (dpy, "|0Yy", NULL, &_dlg_font_height, &_dlg_font_ascent, NULL);
	_dlg_font_height +=2;
	_dlg_font_ascent +=1;
	menu_hover = -1;

	const int items = sizeof(menuitems) / sizeof(struct XjxMenuItem);
	_dlg_height = (_dlg_font_height + 2) * (items + .5);

	int i;
	int max_w = 80;
	for(i = 0; i < items; ++i) {
		int cw;
		if (!query_font_geometry(dpy, menuitems[i].text, &cw, NULL, NULL, NULL)) {
			if (cw > max_w) max_w = cw;
		}
	}
	_dlg_width = max_w + 25;
	XResizeWindow (dpy, _dlg_win, _dlg_width, _dlg_height);

	XWindowAttributes wa;
	XGetWindowAttributes (dpy, _dlg_win, &wa);
	if (wa.screen) {
		int sc_w = WidthOfScreen (wa.screen) - _dlg_width - 5;
		int sc_h = HeightOfScreen (wa.screen) - _dlg_height - 5;
		if (x > sc_w) x = sc_w;
		if (y > sc_h) y = sc_h;
		XMoveWindow (dpy, _dlg_win, x, y);
	}

	XMapRaised (dpy, _dlg_win);

	XGrabPointer (dpy, _dlg_win, False,
			ButtonReleaseMask | ButtonPressMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | StructureNotifyMask,
			GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XGrabKeyboard (dpy, _dlg_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
	//XSetInputFocus (dpy, parent, RevertToNone, CurrentTime);

	update_menu();
	return 0;
}

void close_x_dialog (Display *dpy) {
	if (!_dlg_win) return;
	Colormap colormap = DefaultColormap (dpy, DefaultScreen (dpy));
	XUngrabPointer (dpy, CurrentTime);
	XUngrabKeyboard (dpy, CurrentTime);
	XSync (dpy, False);
	XFreeColors (dpy, colormap, &_c_gray1.pixel, 1, 0);
	XFreeColors (dpy, colormap, &_c_gray2.pixel, 1, 0);
	XFreeGC (dpy, _dlg_gc);
	XDestroyWindow (dpy, _dlg_win);
	_dlg_win = 0;
	force_redraw = 1;
}


int handle_xdlg_event (Display *dpy, XEvent *event) {
	if (!_dlg_win) return 0;
	if (event->xany.window != _dlg_win) {
		return 0;
	}

	switch (event->type) {
		case ConfigureNotify:
			//printf("DLG ConfigureNotify %dx%d\n", event->xconfigure.width, event->xconfigure.height);
			break;
		case Expose:
			if (event->xexpose.count == 0) {
				dialog_expose (dpy);
			}
			break;
		case MotionNotify:
			if (event->xmotion.x > 0 && event->xmotion.y > 0
					&& event->xmotion.x < _dlg_width
					&& event->xmotion.y < _dlg_height)
			{
				dialog_motion (dpy, event->xmotion.x, event->xmotion.y);
			} else {
				dialog_motion (dpy, -1, -1);
			}
			if (event->xmotion.is_hint == NotifyHint) {
				XGetMotionEvents (dpy, _dlg_win, CurrentTime, CurrentTime, NULL);
			}
			break;
		case ButtonPress:
			break;
		case ButtonRelease:
			assert(_dlg_win);
			if (event->xbutton.x > 0 && event->xbutton.y > 0
					&& event->xbutton.x < _dlg_width
					&& event->xbutton.y < _dlg_height)
			{
				if (event->xbutton.button == 1)
					dialog_click (dpy, event->xbutton.x, event->xbutton.y);
			} else if (_dlg_win) {
				close_x_dialog (dpy);
			}
			break;
		case KeyRelease:
			assert(_dlg_win);
			close_x_dialog (dpy);
			break;
	}
	return 1;
}
#endif // platform
#endif // XDLG
