/* xjadeo - very simple X11 file browser
 *        - oh dear, why do this in 2014?
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

#ifndef XFIB_TEST
#include "xjadeo.h"
#else
#define XFIB
#define HAVE_LIBXV
#endif

#ifdef XFIB
#if (defined HAVE_LIBXV || defined HAVE_IMLIB2 || (defined HAVE_GL && !defined PLATFORM_WINDOWS && !defined PLATFORM_OSX))

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xos.h>

#include <assert.h>

#ifndef MIN
#define MIN(A,B) ( (A) < (B) ? (A) : (B) )
#endif

#ifndef MAX
#define MAX(A,B) ( (A) < (B) ? (B) : (A) )
#endif

static Window   _fib_win = 0;
static GC       _fib_gc = 0;
static XColor   _c_gray1, _c_gray2, _c_gray3, _c_gray4;

static int      _fib_width  = 100;
static int      _fib_height = 100;
static int      _btn_w = 0;

static int      _fib_font_height = 0;
static int      _fib_dir_indent  = 0;
static int      _fib_font_ascent = 0;
static int      _fib_font_vsep = 0;
static int      _fib_font_size_width = 0;
static int      _fib_font_time_width = 0;

static int      _scrl_f = 0;
static int      _scrl_y0 = -1;
static int      _scrl_y1 = -1;
static int      _scrl_my = -1;
static int      _scrl_mf = -1;
static int      _view_p = -1;

static int      _fsel = -1;
static int      _hov_b = -1;
static int      _hov_f = -1;
static int      _hov_p = -1;
static int      _sort = 0;
static int      _columns = 0;

static uint8_t  _fib_mapped = 0;
static uint8_t  _fib_resized = 0;
static unsigned long _dblclk = 0;

static int      _status = 0;
static char     _rv_open[1024];

typedef struct {
	char name[256];
	int x0;
	int xw;
	uint8_t flags; // 1: hover
} FibPathButton;

typedef struct {
	char name[256];
	char strtime[32];
	char strsize[32];
	int ssizew;
	off_t size;
	time_t mtime;
	uint8_t flags; // 1: hover, 2: selected, 4: isdir
} FibFileEntry;

typedef struct {
	char text[24];
	uint8_t flags; // 1: hover, 2: selected, 4 sensitive
	int x0;
	int tw;
	void (*callback)(Display*);
} FibButton;

static char           _cur_path[1024] = "";
static FibFileEntry  *_dirlist = NULL;
static FibPathButton *_pathbtn = NULL;
static int            _dircount = 0;
static int            _pathparts = 0;

static FibButton     _btn_ok;
static FibButton     _btn_cancel;
static FibButton     _btn_filter;
static FibButton    *_btns[] = { &_btn_cancel, &_btn_ok};

/* hardcoded layout */
#define DSEP 6 // px; horiz space beween elements, also l+r margin for file-list
#define FILECOLUMN 200 //px;  min width of file-column
#define LISTTOP 2.6 //em;  top of the file-browser list
#define LISTBOT 4.75 //em;  bottom of the file-browers list
#define BTNLRMARGIN 3 // px top/bottom row l+r border also (DSEP /2)
#define BTNBTMMARGIN 0.75 //em;  height/margin of the button row
#define BTNPADDING 2 // px - only used for open/cancel buttons
#define SCROLLBARW 9 //px; - NB. arrows are hardcoded.
#define SCROLLBOXH 10 //px; arrow box top+bottom
#define PATHBTNTOP _fib_font_vsep //px; offset by (_fib_font_ascent);
#define FAREALRMRG 6 //px; left/right margin of file-area
#define TEXTSEP 4 //px;
#define FAREATEXTL 10 //px; filename text-left FAREALRMRG + TEXTSEP
#define SORTBTNOFF -10 //px;

#define DBLCLKTME 200 //msec; double click time

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

static void fib_expose (Display *dpy, Window win) {
	int i;
	const unsigned long whiteColor = WhitePixel (dpy, DefaultScreen (dpy));
	const unsigned long blackColor = BlackPixel (dpy, DefaultScreen (dpy));
	if (!_fib_mapped) return;

	XSync (dpy, False);

	if (_fib_resized) {
		XSetForeground (dpy, _fib_gc, _c_gray1.pixel);
		XFillRectangle (dpy, win, _fib_gc, 0, 0, _fib_width, _fib_height);
		_fib_resized = 0;
	}

	// Top Row: dirs and up navigation
	assert (_pathparts > 0);

	int ppw = 0;
	int ppx = BTNLRMARGIN;

	for (i = _pathparts - 1; i >= 0; --i) {
		ppw += _pathbtn[i].xw + DSEP;
		if (ppw >= _fib_width - DSEP - 9) break; // XXX, first change is from "/" to  "<", NOOP
	}
	++i;
	// border-less "<" parent/up, IFF space is limited
	if (i > 0) {
		if (_pathbtn[0].flags & 1 || (_hov_p > 0 && _hov_p < _pathparts - 1)) {
			XSetForeground (dpy, _fib_gc, _c_gray4.pixel);
		} else {
			XSetForeground (dpy, _fib_gc, blackColor);
		}
		XDrawString (dpy, win, _fib_gc, ppx, PATHBTNTOP, "<", 1);
		ppx += _pathbtn[0].xw + DSEP;
		if (i == _pathparts) --i;
	}

	_view_p = i;

	while (i < _pathparts) {
		if (_pathbtn[i].flags & 1) {
			XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
		} else {
			XSetForeground (dpy, _fib_gc, _c_gray1.pixel);
		}
		XFillRectangle (dpy, win, _fib_gc,
				ppx, PATHBTNTOP - _fib_font_ascent,
				_pathbtn[i].xw + BTNPADDING + BTNPADDING, _fib_font_height);
		XSetForeground (dpy, _fib_gc, blackColor);
		XDrawRectangle (dpy, win, _fib_gc,
				ppx, PATHBTNTOP - _fib_font_ascent,
				_pathbtn[i].xw + BTNPADDING + BTNPADDING, _fib_font_height);
		XDrawString (dpy, win, _fib_gc, ppx + 1 + BTNPADDING, PATHBTNTOP,
				_pathbtn[i].name, strlen (_pathbtn[i].name));
		_pathbtn[i].x0 = ppx; // current position
		ppx += _pathbtn[i].xw + DSEP;
		++i;
	}

	// middle, scroll list of file names
	const int ltop = LISTTOP * _fib_font_vsep;
	const int btop = _fib_height - BTNBTMMARGIN * _fib_font_vsep - BTNPADDING;
	const int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
	const int fsel_height = 4 + llen * _fib_font_vsep;
	const int fsel_width = _fib_width - FAREALRMRG - FAREALRMRG - (llen < _dircount ? SCROLLBARW : 0);
	const int t_x = FAREATEXTL;
	int t_s = FAREATEXTL + fsel_width;
	int t_t = FAREATEXTL + fsel_width;

	// check which colums can be visible
	// depending on available width of window.
	_columns = 0;
	if (fsel_width > FILECOLUMN + _fib_font_size_width + _fib_font_time_width) {
		_columns |= 2;
		t_s = FAREALRMRG + fsel_width - _fib_font_time_width - TEXTSEP;
	}
	if (fsel_width > FILECOLUMN + _fib_font_size_width) {
		_columns |= 1;
		t_t = t_s - _fib_font_size_width - TEXTSEP;
	}

	int fstop = _scrl_f; // first entry in scroll position
	const int ttop = ltop - _fib_font_height + _fib_font_ascent;

	if (fstop > 0 && fstop + llen > _dircount) {
		fstop = MAX (0, _dircount - llen);
		_scrl_f = fstop;
	}

	// list header
	XSetForeground (dpy, _fib_gc, _c_gray4.pixel);
	XFillRectangle (dpy, win, _fib_gc, FAREALRMRG, ltop - _fib_font_vsep, fsel_width, _fib_font_vsep);

	// draw background of file list
	XSetForeground (dpy, _fib_gc, _c_gray3.pixel);
	XFillRectangle (dpy, win, _fib_gc, FAREALRMRG, ltop, fsel_width, fsel_height);

	// column headings and sort order
	XPoint ptri[4] = { {0, ttop - 1}, { 3, -7}, {-6, 0}, { 3, 7}};
	if (_sort & 1) {
		ptri[0].y = ttop - 8;
		ptri[1].x =  3; ptri[1].y =  7;
		ptri[2].x = -6; ptri[2].y =  0;
		ptri[3].x =  3; ptri[3].y = -7;
	}
	XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
	switch (_sort) {
		case 0:
		case 1:
			XFillRectangle (dpy, win, _fib_gc, t_x + _fib_dir_indent - TEXTSEP, ltop - _fib_font_vsep, t_t - t_x - _fib_dir_indent, _fib_font_vsep);
			ptri[0].x = t_t + SORTBTNOFF;
			XSetForeground (dpy, _fib_gc, blackColor);
			XFillPolygon (dpy, win, _fib_gc, ptri, 3, Convex, CoordModePrevious);
			XDrawLines (dpy, win, _fib_gc, ptri, 4, CoordModePrevious);
			break;
		case 2:
		case 3:
			if (_columns & 1) {
				XFillRectangle (dpy, win, _fib_gc, t_t - TEXTSEP, ltop - _fib_font_vsep, _fib_font_size_width + TEXTSEP, _fib_font_vsep);
				ptri[0].x = t_s + SORTBTNOFF;
				XSetForeground (dpy, _fib_gc, blackColor);
				XFillPolygon (dpy, win, _fib_gc, ptri, 3, Convex, CoordModePrevious);
				XDrawLines (dpy, win, _fib_gc, ptri, 4, CoordModePrevious);
			}
			break;
		case 4:
		case 5:
			if (_columns & 2) {
				XFillRectangle (dpy, win, _fib_gc, t_s - TEXTSEP, ltop - _fib_font_vsep, TEXTSEP + TEXTSEP + _fib_font_time_width, _fib_font_vsep);
				ptri[0].x = FAREATEXTL + fsel_width + SORTBTNOFF;
				XSetForeground (dpy, _fib_gc, blackColor);
				XFillPolygon (dpy, win, _fib_gc, ptri, 3, Convex, CoordModePrevious);
				XDrawLines (dpy, win, _fib_gc, ptri, 4, CoordModePrevious);
			}
			break;
	}

	XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
	XDrawLine (dpy, win, _fib_gc,
			t_x + _fib_dir_indent - TEXTSEP, ltop - _fib_font_vsep,
			t_x + _fib_dir_indent - TEXTSEP, ltop);

	XSetForeground (dpy, _fib_gc, blackColor);
	XDrawString (dpy, win, _fib_gc, t_x + _fib_dir_indent, ttop, "Name", 4);

	if (_columns & 1) {
		XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
		XDrawLine (dpy, win, _fib_gc,
				t_t - TEXTSEP, ltop - _fib_font_vsep,
				t_t - TEXTSEP, ltop);
		XSetForeground (dpy, _fib_gc, blackColor);
		XDrawString (dpy, win, _fib_gc, t_t, ttop, "Size", 4);
	}

	if (_columns & 2) {
		XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
		XDrawLine (dpy, win, _fib_gc,
				t_s - TEXTSEP, ltop - _fib_font_vsep,
				t_s - TEXTSEP, ltop);
		XSetForeground (dpy, _fib_gc, blackColor);
		XDrawString (dpy, win, _fib_gc, t_s, ttop, "Last Modified", 13);
	}

	// clip area for file-name
	XRectangle clp = {FAREALRMRG + 1, ltop, t_t - FAREALRMRG - TEXTSEP - TEXTSEP, fsel_height};

	// list files in view
	for (i = 0; i < llen; ++i) {
		const int j = i + fstop;
		if (j >= _dircount) break;

		const int t_y = ltop + (i+1) * _fib_font_vsep;

		XSetForeground (dpy, _fib_gc, blackColor);
		if (_dirlist[j].flags & 2) {
			XSetForeground (dpy, _fib_gc, blackColor);
			XFillRectangle (dpy, win, _fib_gc,
					FAREALRMRG, t_y - _fib_font_ascent, fsel_width, _fib_font_height);
			XSetForeground (dpy, _fib_gc, whiteColor);
		}
		if ((_dirlist[j].flags & 3) == 1) {
			XSetForeground (dpy, _fib_gc, _c_gray4.pixel);
		}
		if (_dirlist[j].flags & 4) {
			XDrawString (dpy, win, _fib_gc, t_x, t_y, "D", 1);
		}
		XSetClipRectangles (dpy, _fib_gc, 0, 0, &clp, 1, Unsorted);
		XDrawString (dpy, win, _fib_gc,
				t_x + _fib_dir_indent, t_y,
				_dirlist[j].name, strlen (_dirlist[j].name));
		XSetClipMask (dpy, _fib_gc, None);

		if (_columns & 1)  // right-aligned 'size'
			XDrawString (dpy, win, _fib_gc,
					t_s - TEXTSEP - 2 - _dirlist[j].ssizew, t_y,
					_dirlist[j].strsize, strlen (_dirlist[j].strsize));
		if (_columns & 2)
			XDrawString (dpy, win, _fib_gc,
					t_s, t_y,
					_dirlist[j].strtime, strlen (_dirlist[j].strtime));
	}

	// scrollbar
	if (llen < _dircount) {
		float sl = (fsel_height + _fib_font_vsep - (SCROLLBOXH + SCROLLBOXH)) / (float) _dircount;
		sl = MAX ((8. / llen), sl); // 8px min height of scroller
		const int sy1 = llen * sl;
		const float mx = (fsel_height + _fib_font_vsep - (SCROLLBOXH + SCROLLBOXH) - sy1) / (float)(_dircount - llen);
		const int sy0 = fstop * mx;
		const int sx0 = _fib_width - SCROLLBARW - FAREALRMRG;
		const int stop = ltop - _fib_font_vsep;

		_scrl_y0 = stop + SCROLLBOXH + sy0;
		_scrl_y1 = _scrl_y0 + sy1;

		assert (fstop + llen <= _dircount);
		// scroll-bar background
		XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
		XFillRectangle (dpy, win, _fib_gc, sx0, stop, SCROLLBARW, fsel_height + _fib_font_vsep);

		// scroller
		XSetForeground (dpy, _fib_gc, _c_gray1.pixel);
		XFillRectangle (dpy, win, _fib_gc, sx0 + 1, stop + SCROLLBOXH + sy0, SCROLLBARW - 2, sy1);

		// arrows top and bottom
		XSetForeground (dpy, _fib_gc, _c_gray1.pixel);
		XPoint ptst[4] = { {sx0 + 1, stop + 8}, {3, -7}, {3, 7}, {-6, 0}};
		XFillPolygon (dpy, win, _fib_gc, ptst, 3, Convex, CoordModePrevious);
		XDrawLines (dpy, win, _fib_gc, ptst, 4, CoordModePrevious);

		XPoint ptsb[4] = { {sx0 + 1, ltop + fsel_height - 9}, {6, 0}, {-3, 7}, {-3, -7}};
		XFillPolygon (dpy, win, _fib_gc, ptsb, 3, Convex, CoordModePrevious);
		XDrawLines (dpy, win, _fib_gc, ptsb, 4, CoordModePrevious);

	} else {
		_scrl_y0 = _scrl_y1 = -1;
	}

	// Bottom Buttons
	const int numb = sizeof(_btns) / sizeof(FibButton*);
	const int xtra = (numb > 1) ? (_fib_width - (BTNLRMARGIN + BTNLRMARGIN) - (_btn_w + DSEP) * numb) / (numb - 1) : 0 ;
	for (i = 0; i < sizeof(_btns) / sizeof(FibButton*); ++i) {
		const int bx = BTNLRMARGIN + i * (_btn_w + DSEP + xtra);
		uint8_t can_hover = 1;
		if (_btns[i] == &_btn_ok) {
			if (_fsel < 0 || _fsel >= _dircount) {
				can_hover = 0;
			}
		}

		if (can_hover && _btns[i]->flags & 1) {
			XSetForeground (dpy, _fib_gc, _c_gray2.pixel);
		} else {
			XSetForeground (dpy, _fib_gc, _c_gray1.pixel);
		}
		XFillRectangle (dpy, win, _fib_gc,
				bx, btop - _fib_font_ascent,
				_btn_w + BTNPADDING + BTNPADDING, _fib_font_height + BTNPADDING + BTNPADDING);
		XSetForeground (dpy, _fib_gc, blackColor);
		XDrawRectangle (dpy, win, _fib_gc,
				bx, btop - _fib_font_ascent,
				_btn_w + BTNPADDING + BTNPADDING, _fib_font_height + BTNPADDING + BTNPADDING);
		XDrawString (dpy, win, _fib_gc, BTNPADDING + 1 + bx + (_btn_w - _btns[i]->tw) * .5, btop + BTNPADDING,
				_btns[i]->text, strlen (_btns[i]->text));
		_btns[i]->x0 = bx;
	}
	XFlush (dpy);
}

static void fib_reset () {
	_hov_p = _hov_f = _fsel = -1;
	_scrl_f = 0;
	_fib_resized = 1;
}

static int cmp_n_up (const void *p1, const void *p2) {
	FibFileEntry *a = (FibFileEntry*) p1;
	FibFileEntry *b = (FibFileEntry*) p2;
	if ((a->flags & 4) && !(b->flags & 4)) return -1;
	if (!(a->flags & 4) && (b->flags & 4)) return 1;
	return strcmp (a->name, b->name);
}

static int cmp_n_down (const void *p1, const void *p2) {
	FibFileEntry *a = (FibFileEntry*) p1;
	FibFileEntry *b = (FibFileEntry*) p2;
	if ((a->flags & 4) && !(b->flags & 4)) return -1;
	if (!(a->flags & 4) && (b->flags & 4)) return 1;
	return strcmp (b->name, a->name);
}

static int cmp_t_up (const void *p1, const void *p2) {
	FibFileEntry *a = (FibFileEntry*) p1;
	FibFileEntry *b = (FibFileEntry*) p2;
	if ((a->flags & 4) && !(b->flags & 4)) return -1;
	if (!(a->flags & 4) && (b->flags & 4)) return 1;
	if (a->size == b->size) return 0;
	return a->mtime > b->mtime ? -1 : 1;
}

static int cmp_t_down (const void *p1, const void *p2) {
	FibFileEntry *a = (FibFileEntry*) p1;
	FibFileEntry *b = (FibFileEntry*) p2;
	if ((a->flags & 4) && !(b->flags & 4)) return -1;
	if (!(a->flags & 4) && (b->flags & 4)) return 1;
	if (a->size == b->size) return 0;
	return a->mtime > b->mtime ? 1 : -1;
}

static int cmp_s_up (const void *p1, const void *p2) {
	FibFileEntry *a = (FibFileEntry*) p1;
	FibFileEntry *b = (FibFileEntry*) p2;
	if ((a->flags & 4) && (b->flags & 4)) return 0; // dir, no size, retain order
	if ((a->flags & 4) && !(b->flags & 4)) return -1;
	if (!(a->flags & 4) && (b->flags & 4)) return 1;
	if (a->size == b->size) return 0;
	return a->size > b->size ? -1 : 1;
}

static int cmp_s_down (const void *p1, const void *p2) {
	FibFileEntry *a = (FibFileEntry*) p1;
	FibFileEntry *b = (FibFileEntry*) p2;
	if ((a->flags & 4) && (b->flags & 4)) return 0; // dir, no size, retain order
	if ((a->flags & 4) && !(b->flags & 4)) return -1;
	if (!(a->flags & 4) && (b->flags & 4)) return 1;
	if (a->size == b->size) return 0;
	return a->size > b->size ? 1 : -1;
}

static void fmt_size (Display *dpy, FibFileEntry *f) {
	if (f->size > 10995116277760) {
		sprintf(f->strsize, "%.0f TB", f->size / 1099511627776.f);
	}
	if (f->size > 1099511627776) {
		sprintf (f->strsize, "%.1f TB", f->size / 1099511627776.f);
	}
	else if (f->size > 10737418240) {
		sprintf (f->strsize, "%.0f GB", f->size / 1073741824.f);
	}
	else if (f->size > 1073741824) {
		sprintf (f->strsize, "%.1f GB", f->size / 1073741824.f);
	}
	else if (f->size > 10485760) {
		sprintf (f->strsize, "%.0f MB", f->size / 1048576.f);
	}
	else if (f->size > 1048576) {
		sprintf (f->strsize, "%.1f MB", f->size / 1048576.f);
	}
	else if (f->size > 10240) {
		sprintf (f->strsize, "%.0f KB", f->size / 1024.f);
	}
	else if (f->size >= 1000) {
		sprintf (f->strsize, "%.1f KB", f->size / 1024.f);
	}
	else {
		sprintf (f->strsize, "%.0f  B", f->size / 1.f);
	}
	int sw;
	query_font_geometry (dpy, _fib_gc, f->strsize, &sw, NULL, NULL , NULL);
	if (sw > _fib_font_size_width) {
		_fib_font_size_width = sw;
	}
	f->ssizew = sw;
}

static void fmt_time (Display *dpy, FibFileEntry *f) {
	struct tm *tmp;
	tmp = localtime (&f->mtime);
	if (!tmp) {
		return;
	}
	strftime (f->strtime, sizeof(f->strtime), "%F %H:%M", tmp);

	int tw;
	query_font_geometry (dpy, _fib_gc, f->strtime, &tw, NULL, NULL , NULL);
	if (tw > _fib_font_time_width) {
		_fib_font_time_width = tw;
	}
}

static void fib_resort () {
	if (_dircount < 1) { return; }
	int (*sortfn)(const void *p1, const void *p2);
	switch (_sort) {
		case 1: sortfn = &cmp_n_down; break;
		case 2: sortfn = &cmp_s_down; break;
		case 3: sortfn = &cmp_s_up; break;
		case 4: sortfn = &cmp_t_down; break;
		case 5: sortfn = &cmp_t_up; break;
		default:
						sortfn = &cmp_n_up;
						break;
	}
	qsort (_dirlist, _dircount, sizeof(_dirlist[0]), sortfn);
}

static int fib_opendir (Display *dpy, const char* path) {
	char *t0, *t1;
	int i, x0;
	assert (strlen (path) < 1024);
	assert (strlen (path) > 0);
	assert (strstr (path, "//") == NULL);

	if (_dirlist) free (_dirlist);
	if (_pathbtn) free (_pathbtn);
	_dirlist = NULL;
	_pathbtn = NULL;
	_dircount = 0;
	_pathparts = 0;
	query_font_geometry (dpy, _fib_gc, "Size  ", &_fib_font_size_width, NULL, NULL , NULL);
	query_font_geometry (dpy, _fib_gc, "Last Modified", &_fib_font_time_width, NULL, NULL , NULL);
	fib_reset ();

	DIR *dir = opendir (path);
	if (!dir) {
		strcpy (_cur_path, "/");
	} else {
		int i;
		char tp[1024];
		struct dirent *de;
		strcpy (_cur_path, path);

		if (_cur_path[strlen (_cur_path) -1] != '/')
			strcat (_cur_path, "/");

		while ((de = readdir (dir))) {
			if (de->d_name[0] == '.') continue;
			++_dircount;
		}

		if (_dircount > 0)
			_dirlist = calloc (_dircount, sizeof(FibFileEntry));

		rewinddir (dir);

		i = 0;
		while ((de = readdir (dir))) {
			struct stat fs;
			if (de->d_name[0] == '.') continue;
			strcpy (tp, _cur_path);
			strcat (tp, de->d_name);
			if (access (tp, R_OK)) {
				continue;
			}
			if (stat (tp, &fs)) {
				continue;
			}
			if (S_ISDIR (fs.st_mode)) {
				_dirlist[i].flags |= 4;
			}
			else if (S_ISREG (fs.st_mode)) {
				;
			}
			else if (S_ISLNK (fs.st_mode)) {
				;
			}
			else {
				continue;
			}
			strcpy (_dirlist[i].name, de->d_name);
			_dirlist[i].mtime = fs.st_mtime;
			_dirlist[i].size = fs.st_size;
			if (!(_dirlist[i].flags & 4))
				fmt_size(dpy, &_dirlist[i]);
			fmt_time(dpy, &_dirlist[i]);
			++i;
		}
		_dircount = i;
		closedir (dir);
		fib_resort();
	}

	t0 = _cur_path;
	while (*t0 && (t0 = strchr (t0, '/'))) {
		++_pathparts;
		++t0;
	}
	assert (_pathparts > 0);
	_pathbtn = calloc (_pathparts + 1, sizeof(FibPathButton));

	t1 = _cur_path;
	i = 0;
	x0 = 0;
	while (*t1 && (t0 = strchr (t1, '/'))) {
		if (i == 0) {
			strcpy (_pathbtn[i].name, "/");
		} else {
			*t0 = 0;
			strcpy (_pathbtn[i].name, t1);
		}
		query_font_geometry (dpy, _fib_gc, _pathbtn[i].name, &_pathbtn[i].xw, NULL, NULL, NULL);
		_pathbtn[i].x0 = x0;
		x0 += _pathbtn[i].xw + DSEP;
		*t0 = '/';
		t1 = t0 + 1;
		++i;
	}
	fib_expose (dpy, _fib_win);
	return _dircount;
}

static int fib_open (Display *dpy, int item) {
	char tp[1024];
	strcpy (tp, _cur_path);
	strcat (tp, _dirlist[item].name);
	if (_dirlist[item].flags & 4) {
		fib_opendir (dpy, tp);
		return 0;
	} else {
		_status = 1;
		strcpy (_rv_open, tp);
	}
	return 0;
}

static void cb_cancel (Display *dpy) {
	_status = -1;
}

static void cb_open (Display *dpy) {
	if (_fsel >= 0 && _fsel < _dircount) {
		fib_open (dpy, _fsel);
	}
}

static int fib_widget_at_pos (Display *dpy, int x, int y, int *it) {
	const int btop = _fib_height - BTNBTMMARGIN * _fib_font_vsep - _fib_font_ascent - BTNPADDING;
	const int bbot = btop +  _fib_font_height + BTNPADDING + BTNPADDING;
	const int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
	const int ltop = LISTTOP * _fib_font_vsep;
	const int fbot = ltop + 4 + llen * _fib_font_vsep;
	const int ptop = PATHBTNTOP - _fib_font_ascent;

	int hov_p = -1;
	int hov_f = -1;
	int hov_b = -1;
	int hov_s = -1;

	// paths at top
	if (y > ptop && y < ptop + _fib_font_height && _view_p >= 0) {
		int i = _view_p;
		int ppx = BTNLRMARGIN;
		if (i > 0) {
			if (x >= ppx && x <= _pathbtn[0].xw + BTNPADDING + BTNPADDING) {
				hov_p = _view_p - 1;
				i = _pathparts;
			}
			ppx += _pathbtn[0].xw + DSEP;
		}
		while (i < _pathparts) {
			if (x >= ppx && x <= ppx + _pathbtn[i].xw + BTNPADDING + BTNPADDING) {
				hov_p = i;
				break;
			}
			ppx += _pathbtn[i].xw + DSEP;
			++i;
		}
	}

	// buttons at bottom
	if (y > btop && y < bbot) {
		int i;
		for (i = 0; i < sizeof(_btns) / sizeof(FibButton*); ++i) {
			const int bx = _btns[i]->x0;
			if (x > bx && x < bx + _btn_w + BTNPADDING + BTNPADDING) {
				hov_b = i;
			}
		}
	}

	// main file area
	if (y >= ltop - _fib_font_vsep && y < fbot && x > FAREALRMRG && x < _fib_width - FAREALRMRG) {
		// scrollbar
		if (_scrl_y0 > 0 && x >= _fib_width - (FAREALRMRG + SCROLLBARW) && x <= _fib_width - DSEP) {
			if (y >= _scrl_y0 && y < _scrl_y1) {
				hov_s = 0;
			} else if (y >= _scrl_y1) {
				hov_s = 2;
			} else {
				hov_s = 1;
			}
		}
		// file-list
		else if (y >= ltop) {
			const int item = (y - ltop) / _fib_font_vsep + _scrl_f;
			if (item >= 0 && item < _dircount) {
				hov_f = item;
			}
		}
		else {
			*it = -1;
			const int fsel_width = _fib_width - FAREALRMRG - FAREALRMRG - (llen < _dircount ? SCROLLBARW : 0);
			const int t_s = FAREALRMRG + fsel_width - _fib_font_time_width - TEXTSEP - TEXTSEP;
			const int t_t = FAREALRMRG + fsel_width - TEXTSEP - _fib_font_size_width - ((_columns & 2) ? ( _fib_font_time_width + TEXTSEP + TEXTSEP) : 0);
			if (x >= fsel_width + FAREALRMRG) ;
			else if ((_columns & 2) && x >= t_s) *it = 3;
			else if ((_columns & 1) && x >= t_t) *it = 2;
			else if (x >= FAREATEXTL + _fib_dir_indent - TEXTSEP) *it = 1;
			return 5;
		}
	}

	if (hov_p >= 0) {
		*it = hov_p;
		return 1;
	}
	if (hov_f >= 0) {
		*it = hov_f;
		return 2;
	}
	if (hov_b >= 0) {
		*it = hov_b;
		return 3;
	}
	if (hov_s >= 0) {
		*it = hov_s;
		return 4;
	}
	return 0;
}

static void fib_update_hover (Display *dpy, int need_expose, int hov_p, int hov_f, int hov_b) {
	if (hov_f != _hov_f) {
#if 1 // FILE HOVER
		if (_hov_f >=0) {
			_dirlist[_hov_f].flags &= ~1;
		}
		_hov_f = hov_f;
		if (hov_f >=0) {
			_dirlist[_hov_f].flags |= 1;
		}
		need_expose = 1;
#endif
	}
	if (hov_b != _hov_b) {
		if (_hov_b >= 0) {
			_btns[_hov_b]->flags &= ~1;
		}
		_hov_b = hov_b;
		if (_hov_b >= 0) {
			_btns[_hov_b]->flags |= 1;
		}
		need_expose = 1;
	}
	if (hov_p != _hov_p) {
		if (_hov_p >= 0) {
			_pathbtn[_hov_p].flags &= ~1;
		}
		_hov_p = hov_p;
		if (_hov_p >= 0) {
			_pathbtn[_hov_p].flags |= 1;
		}
		need_expose = 1;
	}

	if (need_expose) {
		fib_expose (dpy, _fib_win);
	}
}

static void fib_motion (Display *dpy, int x, int y) {
	int it;
	int hov_p = -1;
	int hov_f = -1;
	int hov_b = -1;

	if (_scrl_my >= 0) {
		const int sdiff = y - _scrl_my;
		const int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
		const int fsel_height = 4 + llen * _fib_font_vsep;
		const float sl = (fsel_height + _fib_font_vsep - (SCROLLBOXH + SCROLLBOXH)) / (float) _dircount;

		int news = _scrl_mf + sdiff / sl;
		if (news < 0) news = 0;
		if (news >= (_dircount - llen)) news = _dircount - llen;
		if (news != _scrl_f) {
			_scrl_f = news;
			fib_expose (dpy, _fib_win);
		}
		return;
	}

	switch (fib_widget_at_pos (dpy, x, y, &it)) {
		case 1: hov_p = it; break;
		case 2: hov_f = it; break;
		case 3: hov_b = it; break;
		default: break;
	}

	fib_update_hover (dpy, 0, hov_p, hov_f, hov_b);
}

static void fib_select (Display *dpy, int item) {
	if (_fsel >= 0) {
		_dirlist[_fsel].flags &= ~2;
	}
	_fsel = item;
	if (_fsel >= 0 && _fsel < _dircount) {
		_dirlist[_fsel].flags |= 2;
		const int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
		if (_fsel < _scrl_f) {
			_scrl_f = _fsel;
		}
		else if (_fsel >= _scrl_f + llen) {
			_scrl_f = 1 + _fsel - llen;
		}
	}

	fib_expose (dpy, _fib_win);
}

static void fib_mousedown (Display *dpy, int x, int y, int btn, unsigned long time) {
	int it;
	switch (fib_widget_at_pos (dpy, x, y, &it)) {
		case 4: // scrollbar
			if (btn == 1) {
				_dblclk = 0;
				if (it == 0) {
					_scrl_my = y;
					_scrl_mf = _scrl_f;
				} else {
					int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
					if (llen < 2) llen = 2;
					int news = _scrl_f;
					if (it == 1) {
						news -= llen - 1;
					} else {
						news += llen - 1;
					}
					if (news < 0) news = 0;
					if (news >= (_dircount - llen)) news = _dircount - llen;
					if (news != _scrl_f && _scrl_y0 >= 0) {
						assert (news >=0);
						_scrl_f = news;
						fib_update_hover (dpy, 1, -1, -1, -1);
					}
				}
			}
			break;
		case 2: // file-list
			if (btn == 4 || btn == 5) {
				const int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
				int news = _scrl_f + ((btn == 4) ? - 1 : 1);
				if (news < 0) news = 0;
				if (news >= (_dircount - llen)) news = _dircount - llen;
				if (news != _scrl_f && _scrl_y0 >= 0) {
					assert (news >=0);
					_scrl_f = news;
					fib_update_hover (dpy, 1, -1, -1, -1);
				}
				_dblclk = 0;
			}
			else if (btn == 1 && it >= 0 && it < _dircount) {
				if (_fsel == it) {
					if (time - _dblclk < DBLCLKTME) {
						fib_open (dpy, it);
						_dblclk = 0;
					}
					_dblclk = time;
				} else {
					fib_select (dpy, it);
					_dblclk = time;
				}
				if (_fsel >= 0) {
					if (!(_dirlist[_fsel].flags & 4));
				}
			}
			break;
		case 1: // paths
			assert (it >= 0 && it < _pathparts);
			{
				int i = 0;
				char path[1024] = "/";
				while (++i <= it) {
					strcat (path, _pathbtn[i].name);
					strcat (path, "/");
				}
				fib_opendir (dpy, path);
			}
			break;
		case 3: // btn
			if (btn == 1 && _btns[it]->callback) {
				_btns[it]->callback (dpy);
			}
			break;
		case 5: // sort
			if (btn == 1) {
				switch (it) {
					case 1: if (_sort == 0) _sort = 1; else _sort = 0; break;
					case 2: if (_sort == 2) _sort = 3; else _sort = 2; break;
					case 3: if (_sort == 4) _sort = 5; else _sort = 4; break;
				}
				if (_fsel >= 0) {
					_dirlist[_fsel].flags &= ~2;
				}
				fib_resort ();
				fib_reset ();
				fib_select (dpy, -1);
			}
		default:
			break;
	}
}

static void fib_mouseup (Display *dpy, int x, int y, int btn, unsigned long time) {
	_scrl_my = -1;
}

int show_x_fib (Display *dpy, Window parent, int x, int y) {
	if (_fib_win) return -1;

	_status = 0;
	_rv_open[0] = '\0';

	XColor dummy;
	Colormap colormap = DefaultColormap (dpy, DefaultScreen (dpy));
	if (!XAllocNamedColor (dpy, colormap, "LightGray", &_c_gray1, &dummy)) return -1;
	if (!XAllocNamedColor (dpy, colormap, "DarkGray", &_c_gray2, &dummy)) return -1;
	if (!XAllocNamedColor (dpy, colormap, "Gray", &_c_gray3, &dummy)) return -1;
	if (!XAllocNamedColor (dpy, colormap, "DimGray", &_c_gray4, &dummy)) return -1;

	XSetWindowAttributes attr;
	memset (&attr, 0, sizeof(XSetWindowAttributes));
	attr.border_pixel = _c_gray2.pixel;

	attr.event_mask = ExposureMask | KeyPressMask
		| ButtonPressMask | ButtonReleaseMask
		| ConfigureNotify | StructureNotifyMask
		| PointerMotionMask;

	_fib_win = XCreateWindow (
			dpy, DefaultRootWindow (dpy),
			x, y, _fib_width, _fib_height,
			1, CopyFromParent, InputOutput, CopyFromParent,
			CWEventMask | CWBorderPixel, &attr);

	if (!_fib_win) { return 1; }

	if (parent)
		XSetTransientForHint (dpy, _fib_win, parent);

	XStoreName (dpy, _fib_win, "Select File");

	Atom wmDelete = XInternAtom (dpy, "WM_DELETE_WINDOW", True);
	XSetWMProtocols (dpy, _fib_win, &wmDelete, 1);

	XGCValues gcv;
	_fib_gc = XCreateGC (dpy, _fib_win, 0, &gcv);

	if (_fib_font_height == 0) { // 1st time only
		query_font_geometry (dpy, _fib_gc, "D ", &_fib_dir_indent, NULL, NULL , NULL);
		if (query_font_geometry (dpy, _fib_gc, "|0Yy", NULL, &_fib_font_height, &_fib_font_ascent, NULL)) {
			XFreeGC (dpy, _fib_gc);
			XDestroyWindow (dpy, _fib_win);
			_fib_win = 0;
			return -1;
		}
		_fib_font_height += 3;
		_fib_font_ascent += 2;
		_fib_font_vsep = _fib_font_height + 2;
	}

	strcpy (_btn_ok.text, "Open");
	strcpy (_btn_cancel.text, "Cancel");
	strcpy (_btn_filter.text, "Filter");

	_btn_cancel.callback = &cb_cancel;
	_btn_ok.callback = &cb_open;

	int i;
	for (i = 0; i < sizeof(_btns) / sizeof(FibButton*); ++i) {
		query_font_geometry (dpy, _fib_gc, _btns[i]->text, &_btns[i]->tw, NULL, NULL, NULL);
		if (_btns[i]->tw > _btn_w)
			_btn_w = _btns[i]->tw + 4;
	}
	int minw = 5 + (_btn_w + DSEP) * sizeof(_btns) / sizeof(FibButton*);

	_fib_height = _fib_font_vsep * (15.8);
	_fib_width  = MAX (minw, 420);

	XResizeWindow (dpy, _fib_win, _fib_width, _fib_height);

	XTextProperty x_wname, x_iname;
	XSizeHints hints;
	XWMHints wmhints;
	char *w_name ="xjadeo - Open Video File"; // TODO API
	char *i_name ="xjadeo - Open Video File";

	/* default settings which allow arbitraray resizing of the window */
	hints.flags = PSize | PMaxSize | PMinSize;
	hints.min_width = minw;
	hints.min_height = 8 * _fib_font_vsep;
	hints.max_width = 1024;
	hints.max_height = 1024;

	// TODO Icon..
	wmhints.input = True;
	wmhints.flags = InputHint;
	if (XStringListToTextProperty (&w_name, 1, &x_wname) &&
			XStringListToTextProperty (&i_name, 1, &x_iname))
	{
		XSetWMProperties (dpy, _fib_win, &x_wname, &x_iname, NULL, 0, &hints, &wmhints, NULL);
		XFree (x_wname.value);
		XFree (x_iname.value);
	}

	XSetWindowBackground (dpy, _fib_win, _c_gray1.pixel);

	_fib_mapped = 0;
	XMapRaised (dpy, _fib_win);

	if (!strlen (_cur_path) || !fib_opendir (dpy, _cur_path)) {
		fib_opendir (dpy, getenv ("HOME") ? getenv ("HOME") : "/");
	}

#if 0
	XGrabPointer (dpy, _fib_win, True,
			ButtonReleaseMask | ButtonPressMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | StructureNotifyMask,
			GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XGrabKeyboard (dpy, _fib_win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	//XSetInputFocus (dpy, parent, RevertToNone, CurrentTime);
#endif
	return 0;
}

void close_x_fib (Display *dpy) {
	if (!_fib_win) return;
	XFreeGC (dpy, _fib_gc);
	XDestroyWindow (dpy, _fib_win);
	_fib_win = 0;
	if (_dirlist) free (_dirlist);
	if (_pathbtn) free (_pathbtn);
	_dirlist = NULL;
	_pathbtn = NULL;
}

int handle_xfib_event (Display *dpy, XEvent *event) {
	if (!_fib_win) return 0;
	if (_status) return 0;
	if (event->xany.window != _fib_win) {
		return 0;
	}

	switch (event->type) {
		case MapNotify:
			_fib_mapped = 1;
			break;
		case UnmapNotify:
			_fib_mapped = 0;
			break;
		case ClientMessage:
			if (!strcmp (XGetAtomName (dpy, event->xclient.message_type), "WM_PROTOCOLS")) {
				_status = -1;
			}
		case ConfigureNotify:
			if (event->xconfigure.width != _fib_width || event->xconfigure.height != _fib_height) {
				_fib_width = event->xconfigure.width;
				_fib_height = event->xconfigure.height;
				_fib_resized = 1;
			}
			break;
		case Expose:
			if (event->xexpose.count == 0) {
				fib_expose (dpy, event->xany.window);
			}
			break;
		case MotionNotify:
			fib_motion (dpy, event->xmotion.x, event->xmotion.y);
			if (event->xmotion.is_hint == NotifyHint) {
				XGetMotionEvents (dpy, event->xany.window, CurrentTime, CurrentTime, NULL);
			}
			break;
		case ButtonPress:
			fib_mousedown (dpy, event->xbutton.x, event->xbutton.y, event->xbutton.button, event->xbutton.time);
			break;
		case ButtonRelease:
			fib_mouseup (dpy, event->xbutton.x, event->xbutton.y, event->xbutton.button, event->xbutton.time);
			break;
		case KeyRelease:
			break;
		case KeyPress:
			{
				KeySym key;
				char buf[100];
				static XComposeStatus stat;
				XLookupString (&event->xkey, buf, sizeof(buf), &key, &stat);
				switch (key) {
					case XK_Escape:
						_status = -1;
						break;
					case XK_Up:
						if (_fsel > 0) {
							fib_select (dpy, _fsel - 1);
						}
						break;
					case XK_Down:
						if (_fsel < _dircount -1) {
							fib_select ( dpy, _fsel + 1);
						}
						break;
					case XK_Page_Up:
						if (_fsel > 0) {
							int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
							if (llen < 1) llen = 1; else --llen;
							int fs = MAX (0, _fsel - llen);
							fib_select ( dpy, fs);
						}
						break;
					case XK_Page_Down:
						if (_fsel < _dircount) {
							int llen = (_fib_height - LISTBOT * _fib_font_vsep) / _fib_font_vsep;
							if (llen < 1) llen = 1; else --llen;
							int fs = MIN (_dircount - 1, _fsel + llen);
							fib_select ( dpy, fs);
						}
						break;
					case XK_Left:
						if (_pathparts > 1) {
							int i = 0;
							char path[1024] = "/";
							while (++i < _pathparts - 1) {
								strcat (path, _pathbtn[i].name);
								strcat (path, "/");
							}
							fib_opendir (dpy, path);
						}
						break;
					case XK_Right:
						if (_fsel >= 0 && _fsel < _dircount) {
							if (_dirlist[_fsel].flags & 4) {
								cb_open (dpy);
							}
						}
						break;
					case XK_Return:
						cb_open (dpy);
						break;
					default:
						if ((key >= XK_a && key <= XK_z) || (key >= XK_0 && key <= XK_9)) {
							int i;
							for (i = 0; i < _dircount; ++i) {
								int j = (_fsel + i + 1) % _dircount;
								char kcmp = _dirlist[j].name[0];
								if (kcmp > 0x40 && kcmp <= 0x5A) kcmp |= 0x20;
								if (kcmp == key) {
									fib_select ( dpy, j);
									break;
								}
							}
						}
						break;
				}
			}
			break;
	}

	if (_status) {
		close_x_fib (dpy);
	}
	return _status;
}

int status_x_fib () {
	return _status;
}

char *filename_x_fib () {
	if (_status > 0)
		return strdup (_rv_open);
	else
		return NULL;
}
#endif // platform
#endif // XDLG


#ifdef XFIB_TEST // gcc -Wall -D XFIB_TEST -g -o xvesifib xvesifib.c -lX11
int main (int argc, char **argv) {
	Display* dpy = XOpenDisplay (0);
	if (!dpy) return -1;

	show_x_fib (dpy, 0, 300, 300);

	while (1) {
		XEvent event;
		while (XPending (dpy) > 0) {
			XNextEvent (dpy, &event);
			if (handle_xfib_event (dpy, &event)) {
				if (status_x_fib () > 0) {
					char *fn = filename_x_fib ();
					printf ("OPEN '%s'\n", fn);
					free (fn);
				}
			}
		}
		if (status_x_fib ()) {
			break;
		}
		usleep (80000);
	}
	close_x_fib (dpy);
	XCloseDisplay (dpy);
	return 0;
}
#endif
