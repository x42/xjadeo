/* xjadeo - openGL display for X11
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
#if (defined HAVE_GL && !defined PLATFORM_WINDOWS && !defined PLATFORM_OSX)

#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>

#include "icons/xjadeo8.xpm"

void xapi_open (void *d);
void xapi_close (void *d);

static Display*   _gl_display;
static int        _gl_screen;
static Window     _gl_win;
static GLXContext _gl_ctx;

extern double framerate;  // used for screensaver

static void gl_sync_lock() { }
static void gl_sync_unlock() { }

static void gl_make_current() {
	glXMakeCurrent(_gl_display, _gl_win, _gl_ctx);
}

static void gl_swap_buffers() {
	glXSwapBuffers(_gl_display, _gl_win);
}

void gl_newsrc () {
	gl_reallocate_texture(movie_width, movie_height);
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
	XpmCreatePixmapFromData(dpy, parent, xjadeo8_xpm, &wmhints.icon_pixmap, &wmhints.icon_mask, NULL);
	wmhints.flags = InputHint | IconPixmapHint | IconMaskHint;

	if (XStringListToTextProperty (&w_name, 1, &x_wname) &&
			XStringListToTextProperty (&i_name, 1, &x_iname))
	{
		XSetWMProperties (dpy, win, &x_wname, &x_iname, NULL, 0, &hints, &wmhints, NULL);
		XFree (x_wname.value);
		XFree (x_iname.value);
	}
}

static int check_glx_extention(const char *ext) {
	if (!ext || strchr(ext, ' ') || *ext == '\0') {
		return 0;
	}
	const char *exts = glXQueryExtensionsString(_gl_display, _gl_screen);
	if (!exts) {
		return 0;
	}

	const char *start = exts;
	while (1) {
		const char *tmp = strstr(start, ext);
		if (!tmp) break;
		const char *end = tmp + strlen(ext);
		if (tmp == start || *(tmp - 1) == ' ')
			if (*end == ' ' || *end == '\0') return 1;
		start = end;
	}
	return 0;
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

	if (!vi) {
		fprintf(stderr, "GLX visual is not supported\n");
		XCloseDisplay(_gl_display);
		return 1;
	}

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

	_gl_width = ffctv_width;
	_gl_height = ffctv_height;

	attr.event_mask = ExposureMask | KeyPressMask
		| Button1MotionMask
		| ButtonPressMask | ButtonReleaseMask
		| StructureNotifyMask;

	_gl_win = XCreateWindow(
		_gl_display, xParent,
		0, 0, _gl_width, _gl_height, 0, vi->depth, InputOutput, vi->visual,
		CWBorderPixel | CWColormap | CWEventMask, &attr);

	if (!_gl_win) {
		XCloseDisplay(_gl_display);
		return 1;
	}

	XStoreName(_gl_display, _gl_win, "xjadeo");

	Atom wmDelete = XInternAtom(_gl_display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(_gl_display, _gl_win, &wmDelete, 1);

	setup_window_hints_and_icon(_gl_display, _gl_win, xParent, 4096 /*TODO query max texture size*/);

	_gl_ctx = glXCreateContext(_gl_display, vi, 0, GL_TRUE);

	if (!_gl_ctx) {
		XDestroyWindow(_gl_display, _gl_win);
		XCloseDisplay(_gl_display);
		return 1;
	}

#ifdef DND
	init_dnd(_gl_display, _gl_win);
#endif
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
	if (gl_reallocate_texture(movie_width, movie_height)) {
		gl_close_window ();
		return 1;
	}

#if 1 // check for VBlank sync
	/* https://www.opengl.org/wiki/Swap_Interval ; -1 for adaptive */

	int (*glXSwapIntervalSGI)(int interval) = (int (*)(int))
		glXGetProcAddress((const GLubyte *)"glXSwapIntervalSGI");
	GLint (*glXSwapIntervalMESA) (unsigned interval) = (GLint (*)(unsigned))
		glXGetProcAddress((const GLubyte *)"glXSwapIntervalMESA");
	int (*glXSwapIntervalEXT)(Display *dpy, GLXDrawable drw, int interval) =
		(int (*)(Display*, GLXDrawable, int))
		glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");

	int vblank = -1;
	if (glXSwapIntervalSGI && check_glx_extention("GLX_SGI_swap_control")) {
		vblank = glXSwapIntervalSGI(1);
		if (want_verbose)
			printf("GLX: use SGI Vblank\n");
	}
	else if (glXSwapIntervalMESA && check_glx_extention("GLX_MESA_swap_control")) {
		vblank = glXSwapIntervalMESA(1);
		if (want_verbose)
			printf("GLX: use MESA Vblank\n");
	}
	else if (glXSwapIntervalEXT && check_glx_extention("GLX_EXT_swap_control")) {
		GLXDrawable drawable = glXGetCurrentDrawable();
		if (drawable) {
			vblank = glXSwapIntervalEXT(_gl_display, drawable, 1);
			if (want_verbose)
				printf("GLX: use EXT Vblank\n");
		}
	} else {
		if (!want_quiet) {
			fprintf(stderr, "openGL VBlank not synced\n");
		}
	}
	// https://www.opengl.org/wiki/Swap_Interval#GPU_vs_CPU_synchronization
	//_gl_vblank_sync = (vblank == 0) ? 1 : 0;
#endif
	return 0;
}

void gl_close_window() {
#ifdef XDLG
	close_x_dialog(_gl_display);
#endif
#ifdef XFIB
	x_fib_close (_gl_display);
#endif
	glXDestroyContext(_gl_display, _gl_ctx);
	XDestroyWindow(_gl_display, _gl_win);
	XCloseDisplay(_gl_display);
}

void gl_handle_events () {
	XEvent event;
	while (XPending(_gl_display) > 0) {
		XNextEvent(_gl_display, &event);
#ifdef XDLG
		if (handle_xdlg_event(_gl_display, &event)) continue;
#endif
#ifdef XFIB
		if (x_fib_handle_events (_gl_display, &event)) {
			if (x_fib_status () > 0) {
				char *fn = x_fib_filename ();
				xapi_open (fn);
				free (fn);
			}
		}
#endif
		if (event.xany.window != _gl_win) {
			continue;
		}
#ifdef DND
		if (handle_dnd_event(_gl_display, _gl_win, &event)) continue;
#endif
		switch (event.type) {
			case MapNotify:
				loop_run=1;
				break;
			case UnmapNotify:
				loop_run=0;
				break;

			case ConfigureNotify:
				if (
						(event.xconfigure.width > 1 && event.xconfigure.height > 1)
						&&
						(event.xconfigure.width != _gl_width || event.xconfigure.height != _gl_height)
					 )
				{
					gl_reshape(event.xconfigure.width, event.xconfigure.height);
				}
				break;
			case Expose:
				if (event.xexpose.count != 0) {
					break;
				}
				_gl_reexpose = true;
				break;
			case MotionNotify:
				if (osd_seeking && ui_syncsource() == SYNC_NONE && OSD_mode & OSD_POS) {
					const float sk = calc_slider (event.xmotion.x, event.xmotion.y);
					if (sk >= 0)
						ui_sync_manual (sk);
				}
				break;
			case ButtonPress:
				if (event.xbutton.button == 1 && ui_syncsource() == SYNC_NONE && OSD_mode & OSD_POS) {
					const float sk = calc_slider (event.xbutton.x, event.xbutton.y);
					if (sk >= 0) {
						ui_sync_manual (sk);
						osd_seeking = 1;
						force_redraw = 1;
					}
				}
				break;
			case ButtonRelease:
				if (osd_seeking) {
					osd_seeking = 0;
					force_redraw = 1;
				} else
#ifdef XDLG
				if (event.xbutton.button == 3) {
					if (event.xbutton.x >= 0 && event.xbutton.x < _gl_width
							&& event.xbutton.y >= 0 && event.xbutton.y < _gl_height)
						show_x_dialog(_gl_display, _gl_win,
								event.xbutton.x_root, event.xbutton.y_root
								);
				} else
#endif
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
				if (event.xkey.state & ControlMask && n == 1 && sym == XK_o) {
#ifdef XFIB
					if (!(interaction_override & OVR_LOADFILE)) {
						x_fib_cfg_filter_callback(fib_filter_movie_filename);
						x_fib_show (_gl_display, _gl_win, 0, 0);
					}
#endif
				}
				else if (event.xkey.state & ControlMask && n == 1 && sym == XK_w) {
					if (!(interaction_override & OVR_LOADFILE))
						xapi_close (NULL);
				}
				else if (event.xkey.state & ControlMask && n == 1 && sym == XK_q) {
					if (!(interaction_override&OVR_QUIT_WMG))
						loop_flag = 0;
				}
				else if (n == 1) {
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
		force_redraw = true;
	}

	static int periodic = 10;
	if (--periodic == 0) {
		periodic = 50 * framerate; // we should use 1/delay if delay > 0
		XResetScreenSaver(_gl_display);
		// ..or spawn `xdg-screensaver`
	}
}

void gl_render (uint8_t *mybuffer) {
	xjglExpose(NULL);
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
	XTranslateCoordinates (_gl_display, _gl_win, DefaultRootWindow(_gl_display), 0, 0, rx, ry, &dummy);
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
	if (action==2) hide_mouse ^= 1;
	else hide_mouse = action ? 1 : 0;

	if (hide_mouse) {
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
