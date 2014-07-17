/* xjadeo - jack video monitor
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

#include "display_gl_common.h"
#if (defined HAVE_GL && !defined PLATFORM_WINDOWS)

#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

static Display*   _gl_display;
static int        _gl_screen;
static Window     _gl_win;
static GLXContext _gl_ctx;

static void gl_make_current() {
	glXMakeCurrent(_gl_display, _gl_win, _gl_ctx);
}

static void gl_swap_buffers() {
	glXSwapBuffers(_gl_display, _gl_win);
}

static void glx_netwm(const char *atom, const int onoff) {
	const int set = onoff ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;

	Atom type = XInternAtom(_gl_display,"_NET_WM_STATE", 0);
	if (type == None) return;
	Atom property = XInternAtom(_gl_display, atom, 0);
	if (property == None) return;

	XEvent xev;
	xev.type = ClientMessage;
	xev.xclient.type = ClientMessage;
	xev.xclient.window = _gl_win;
	xev.xclient.message_type = type;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = set;
	xev.xclient.data.l[1] = property;
	xev.xclient.data.l[2] = 0;

	if (!XSendEvent(_gl_display, DefaultRootWindow(_gl_display), False,
				SubstructureRedirectMask|SubstructureNotifyMask, &xev))
	{
			fprintf(stderr,"error changing X11 property\n");
	}
}

#ifdef HAVE_XPM
#include <X11/xpm.h>
#include "icons/xjadeo-color.xpm"
#else
#include "icons/xjadeo.bitmap"
#include "icons/xjadeo_mask.xbm"
#endif

static void setup_window_hints_and_icon(Display* dpy, Window win, Window parent, const int maxsize) {
	XTextProperty	x_wname, x_iname;
	XSizeHints	hints;
	XWMHints	wmhints;
	char *w_name ="xjadeo";
	char *i_name ="xjadeo";

	/* default settings which allow arbitraray resizing of the window */
	hints.flags = PSize | PMaxSize | PMinSize;
	hints.min_width = 32;
	hints.min_height = 18;
	hints.max_width = maxsize;
	hints.max_height = maxsize;

	wmhints.input = True;
#ifdef HAVE_XPM
	XpmCreatePixmapFromData(dpy, parent, xjadeo_color_xpm, &wmhints.icon_pixmap, &wmhints.icon_mask, NULL);
#else
	wmhints.icon_pixmap = XCreateBitmapFromData(dpy, parent, (char *)xjadeo_bits , xjadeo_width, xjadeo_height);
	wmhints.icon_mask  = XCreateBitmapFromData(dpy, parent, (char *)xjadeo_mask_bits , xjadeo_mask_width, xjadeo_mask_height);
#endif
	wmhints.flags = InputHint | IconPixmapHint | IconMaskHint;

	XStringListToTextProperty(&w_name, 1 ,&x_wname);
	XStringListToTextProperty(&i_name, 1 ,&x_iname);

	XSetWMProperties(dpy, win, &x_wname, &x_iname, NULL, 0, &hints, &wmhints, NULL);
}


int gl_open_window () {
	_gl_display = XOpenDisplay(0);
	if (!_gl_display) {
		fprintf( stderr, "Cannot connect to X server\n");
		return 1;
	}
	_gl_screen = DefaultScreen(_gl_display);

	int attrList[] = {
		GLX_RGBA, GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 4,
		GLX_GREEN_SIZE, 4,
		GLX_BLUE_SIZE, 4,
		GLX_DEPTH_SIZE, 16,
		GLX_ARB_multisample, 1,
		None
	};

	XVisualInfo* vi = glXChooseVisual(_gl_display, _gl_screen, attrList);

	int glxMajor, glxMinor;
	glXQueryVersion(_gl_display, &glxMajor, &glxMinor);
	if (want_verbose) {
		printf("GLX-Version : %d.%d\n", glxMajor, glxMinor);
	}

	Window xParent = RootWindow(_gl_display, _gl_screen);

	Colormap cmap = XCreateColormap(
		_gl_display, xParent, vi->visual, AllocNone);

	XSetWindowAttributes attr;
	memset(&attr, 0, sizeof(XSetWindowAttributes));
	attr.colormap     = cmap;
	attr.border_pixel = 0;

	_gl_width = movie_width;
	_gl_height = movie_height;

	attr.event_mask = ExposureMask | KeyPressMask
		| ButtonPressMask | ButtonReleaseMask
		| StructureNotifyMask;

	_gl_win = XCreateWindow(
		_gl_display, xParent,
		0, 0, _gl_width, _gl_height, 0, vi->depth, InputOutput, vi->visual,
		CWBorderPixel | CWColormap | CWEventMask, &attr);

	XStoreName(_gl_display, _gl_win, "xjadeo");

	Atom wmDelete = XInternAtom(_gl_display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(_gl_display, _gl_win, &wmDelete, 1);

	setup_window_hints_and_icon(_gl_display, _gl_win, xParent, 4096 /*TODO query max texture size*/);

	_gl_ctx = glXCreateContext(_gl_display, vi, 0, GL_TRUE);

	XMapRaised(_gl_display, _gl_win);

	if (want_verbose) {
		if (glXIsDirect(_gl_display, _gl_ctx)) {
			printf("GLX: DRI enabled\n");
		} else {
			printf("GLX: No DRI available\n");
		}
	}

	XFree(vi);

	if (start_fullscreen) { gl_set_fullscreen(1); }
	if (start_ontop) { gl_set_ontop(1); }

	glXMakeCurrent(_gl_display, _gl_win, _gl_ctx);
	gl_init();
	gl_reallocate_texture(movie_width, movie_height);
	return 0;
}

void gl_close_window() {
	glXDestroyContext(_gl_display, _gl_ctx);
	XDestroyWindow(_gl_display, _gl_win);
	XCloseDisplay(_gl_display);
}

void gl_handle_events () {
	XEvent event;
	while (XPending(_gl_display) > 0) {
		XNextEvent(_gl_display, &event);

		switch (event.type) {
			case MapNotify:
				loop_run=1;
				break;
			case UnmapNotify:
				loop_run=0;
				break;

			case ConfigureNotify:
				if ((event.xconfigure.width != _gl_width) ||
						(event.xconfigure.height != _gl_height)) {
					gl_reshape(event.xconfigure.width, event.xconfigure.height);
				}
				break;
			case Expose:
				if (event.xexpose.count != 0) {
					break;
				}
				_gl_reexpose = true;
				break;
			case ButtonPress:
				break;
			case ButtonRelease:
				xjglButton(event.xbutton.button);
				break;
			case ReparentNotify:
				break;
			case KeyPress:
				{
				KeySym  sym;
				char    buf[6] = {0,0,0,0,0,0};
				static XComposeStatus stat;
				int n = XLookupString(&event.xkey, buf, sizeof(buf), &sym, &stat);
				if (n == 1) {
					xjglKeyPress(sym, buf);
				}
				}
				break;
			case ClientMessage:
				if (!strcmp(XGetAtomName(_gl_display, event.xclient.message_type), "WM_PROTOCOLS")) {
					if ((interaction_override&OVR_QUIT_WMG) == 0) loop_flag=0;
				}
				break;
			default:
				break;
		}
	}
	if (_gl_reexpose) {
		_gl_reexpose = false;
		xjglExpose();
	}
}

void gl_render (uint8_t *mybuffer) {
	xjglExpose();
}

void gl_newsrc () {
}

void gl_resize (unsigned int x, unsigned int y) {
	XResizeWindow(_gl_display, _gl_win, x, y);
	XFlush(_gl_display);
}

void gl_position (int x, int y) {
	XMoveWindow(_gl_display, _gl_win, x, y);
}

void gl_get_window_size (unsigned int *w, unsigned int *h) {
	*w = _gl_width;
	*h = _gl_height;
}

void gl_get_window_pos (int *rx, int *ry) {
	Window	dummy;
	XTranslateCoordinates (_gl_display, _gl_win, DefaultRootWindow(_gl_win), 0, 0, rx, ry, &dummy);
	while (dummy !=None) {
		int x = 0;
		int y = 0;
		XTranslateCoordinates (_gl_display, _gl_win, dummy, 0, 0, &x, &y, &dummy);
		if (dummy!=None) {
			(*rx)-=x; (*ry)-=y;
		} else {
			(*rx)+=x; (*ry)+=y;
		}
	}
}

void gl_set_ontop (int action) {
	if (action==2) _gl_ontop ^= 1;
	else _gl_ontop = action ? 1 : 0;
	glx_netwm("_NET_WM_STATE_ABOVE", _gl_ontop);
}

int gl_get_ontop () {
	return _gl_ontop;
}

void gl_set_fullscreen (int action) {
	if (action==2) _gl_fullscreen^=1;
	else _gl_fullscreen = action ? 1 : 0;
	glx_netwm("_NET_WM_STATE_FULLSCREEN", _gl_fullscreen);
}

int  gl_get_fullscreen () {
	return _gl_fullscreen;
}

void gl_mousepointer (int action) {
	if (action==2) _gl_mousepointer^=1;
	else _gl_mousepointer = action ? 1 : 0;

	if (_gl_mousepointer) {
		/* hide */
		Cursor no_ptr;
		Pixmap bm_no;
		XColor black, dummy;
		Colormap colormap;
		static char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		colormap = DefaultColormap (_gl_display, _gl_screen);
		if (!XAllocNamedColor (_gl_display, colormap, "black", &black, &dummy) ) return;
		bm_no = XCreateBitmapFromData (_gl_display, _gl_win, bm_no_data, 8, 8);
		no_ptr = XCreatePixmapCursor (_gl_display, bm_no, bm_no, &black, &black, 0, 0);
		XDefineCursor (_gl_display, _gl_win, no_ptr);
		XFreeCursor (_gl_display, no_ptr);
		if (bm_no != None) XFreePixmap (_gl_display, bm_no);
		XFreeColors (_gl_display,colormap, &black.pixel, 1, 0);
	} else {
		/* show */
		XDefineCursor(_gl_display, _gl_win, 0);
	}
}
#endif
