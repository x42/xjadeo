/* xjadeo - X11 Drag/Drop support
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

#ifdef DND 
#if (defined HAVE_LIBXV || defined HAVE_IMLIB2 || (defined HAVE_GL && !defined PLATFORM_WINDOWS && !defined PLATFORM_OSX))

extern int 	interaction_override; // disable some options.

#define MAX_DND_FILES 8
void xapi_open(void *d); 

/*******************************************************************************
 * Drag and Drop - common X11 code
 */

static Atom xj_a_XdndDrop;   
static Atom xj_a_XdndFinished;   
static Atom xj_a_XdndActionCopy;   
static Atom xj_a_XdndLeave;   
static Atom xj_a_XdndPosition;   
static Atom xj_a_XdndStatus;   
static Atom xj_a_XdndEnter;   
static Atom xj_a_XdndAware;   
static Atom xj_a_XdndTypeList;   
static Atom xj_a_XdndSelection;   
static Atom xj_atom;
static int  dnd_source = 0;

static const int xdnd_version = 5;
static int dnd_enabled = 0;

static void HandleEnter(Display *dpy, XEvent * xe) {
	const long *l = xe->xclient.data.l;
	xj_atom = None;
	dnd_source = 0;

	const Atom ok0 = XInternAtom(dpy, "text/uri-list", False);
	const Atom ok1 = XInternAtom(dpy, "text/plain", False);
	const Atom ok2 = XInternAtom(dpy, "UTF8_STRING", False);

	const int version = (int)(((unsigned long)(l[1])) >> 24);

	if (version > xdnd_version) return;

	dnd_source = l[0];

	if (l[1] & 0x1UL) {
		Atom type = 0;
		int f;
		unsigned long n, a;
		unsigned char *data;
		a=1;
		while(a && xj_atom == None) {
			int ll;
			XGetWindowProperty(dpy, dnd_source, xj_a_XdndTypeList, 0, 0x8000000L, False, XA_ATOM, &type, &f, &n, &a, &data);
			if(data == NULL || type != XA_ATOM || f != 32) {
				if (data) XFree(data);
				return;
			}
			Atom *types = (Atom *) data;
			for (ll = 1; ll < n; ++ll) {
				//if (types[ll] != None) printf("DEBUG atomN:%s\n", XGetAtomName(dpy, types[ll]));
				if ((types[ll] == ok1) || (types[ll] == ok1) || (types[ll] == ok2)) {
					xj_atom = types[ll];
					break;
				}
			}
			if (data) XFree(data);
		}
	} else {
		int i;
		for(i = 2; i < 5; i++) {
			//if (l[i] != None) printf("DEBUG atom3:%s\n", XGetAtomName(dpy, l[i]));
			if ((l[i] == ok0) || (l[i] == ok1) || (l[i] == ok2)) xj_atom= l[i];
		}
	}
	if (want_debug)
		printf("DEBUG: DND ok: %i\n", xj_atom != None);
}

static void SendStatus (Display *dpy, Window win, XEvent * xe) {
	XEvent xev;
	memset(&xev, 0, sizeof(XEvent));
	xev.xany.type = ClientMessage;
	xev.xany.display = dpy;
	xev.xclient.window = dnd_source;
	xev.xclient.message_type = xj_a_XdndStatus;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = win; 
	xev.xclient.data.l[1] = (xj_atom != None) ? 1 : 0;
	xev.xclient.data.l[4] = xj_a_XdndActionCopy;
	XSendEvent(dpy, dnd_source, False, NoEventMask, &xev);
}

static void SendFinished (Display *dpy, Window win) {
	XEvent xev;
	memset(&xev,0,sizeof(XEvent));
	xev.xany.type = ClientMessage;
	xev.xany.display = dpy;
	xev.xclient.window = dnd_source;
	xev.xclient.message_type = xj_a_XdndFinished;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = win;
	XSendEvent(dpy, dnd_source, False, NoEventMask, &xev);
}

static void getDragData (Display *dpy, Window win, XEvent *xe) {
	Atom type;
	int f;
	unsigned long n, a;
	unsigned char *data;

	if(xe->xselection.property != xj_a_XdndSelection) return;

	XGetWindowProperty(dpy, xe->xselection.requestor,
			xe->xselection.property, 0, 65536, 
	    True, xj_atom, &type, &f, &n, &a, &data);

	SendFinished(dpy, win);

	if (!data) {
		if (!want_quiet)
			fprintf(stderr, "WARNING: drag-n-drop - no data received.\n"); 
		return;
	}

	/* Handle dropped files */
	int num = 0;
	char *retain = (char*) data;
	char *files[MAX_DND_FILES];

	while(retain < (char*)data + n) {
		int nl = 0;
		if (!strncmp(retain,"file:", 5)) { retain += 5; }
		files[num++] = retain;

		while(retain < (char*)data + n) {
			if(*retain == '\r' || *retain == '\n') {
				*retain=0;
				nl = 1;
			} else if (nl) {
				break;
			}
			++retain;
		}

		if (num >= MAX_DND_FILES) {
			break;
		}
	}

	if (want_debug) {
		for (f=0; f < num; ++f) {
			printf("DnD: recv: %i '%s'\n", f, files[f]);
		}
	}

	if (num > 0) {
		//translate %20 -> to whitespaces, etc.
		char *t = files[0];
		while ((t = strchr(t, '%')) && strlen(t) > 2) {
			int ti = 0;
			char tc= t[3];
			t[3] = 0;
			ti = (int)strtol(&(t[1]), NULL, 16);
			t[3] = tc; 
			memmove(t + 1, t + 3, strlen(t) - 2);
			tc = (char)ti;
			*t = tc;
			t[strlen(t)]='\0';
			t++;
		}
	}

	if (num > 0 && !(interaction_override&OVR_LOADFILE)) {
		xapi_open(files[0]);
		XSetInputFocus(dpy, win, RevertToNone, CurrentTime);
	}
	free(data);
}

void init_dnd (Display *dpy, Window win) {
	Atom atm = (Atom)xdnd_version;
	if ((xj_a_XdndDrop       = XInternAtom (dpy, "XdndDrop", False)) != None && 
	    (xj_a_XdndLeave      = XInternAtom (dpy, "XdndLeave", False)) != None && 
	    (xj_a_XdndEnter      = XInternAtom (dpy, "XdndEnter", False)) != None && 
	    (xj_a_XdndActionCopy = XInternAtom (dpy, "XdndActionCopy", False)) != None && 
	    (xj_a_XdndFinished   = XInternAtom (dpy, "XdndFinished", False)) != None && 
	    (xj_a_XdndPosition   = XInternAtom (dpy, "XdndPosition", False)) != None && 
	    (xj_a_XdndStatus     = XInternAtom (dpy, "XdndStatus", False)) != None && 
	    (xj_a_XdndTypeList   = XInternAtom (dpy, "XdndTypeList", False)) != None && 
	    (xj_a_XdndSelection  = XInternAtom (dpy, "XdndSelection", False)) != None && 
	    (xj_a_XdndAware      = XInternAtom (dpy, "XdndAware", False)) != None)
	{
		if(!want_quiet) printf("enabled drag-DROP support.\n");
		XChangeProperty(dpy, win, xj_a_XdndAware, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atm, 1);
		dnd_enabled = 1;
	}
}

void disable_dnd (Display *dpy, Window win) {
	if (!dnd_enabled) return;
	XDeleteProperty(dpy, win, xj_a_XdndAware);
	dnd_enabled = 0;
}

int handle_dnd_event (Display *dpy, Window win, XEvent *event) {
	int handled = 1;
	if (!dnd_enabled) return 0;
	switch (event->type) {
		case SelectionRequest:
			break;
		case SelectionNotify:
			if (want_debug) printf("DnD SelectionNotify\n");
			getDragData(dpy, win, event);
			break;
		case ClientMessage:
			if (event->xclient.message_type == xj_a_XdndPosition) {
				SendStatus(dpy, win, event);
			} else if (event->xclient.message_type == xj_a_XdndLeave) {
				if (want_debug) printf("DnD Leave\n");
				xj_atom = None;
			} else if (event->xclient.message_type == xj_a_XdndEnter) {
				if (want_debug) printf("DnD Enter\n");
				HandleEnter(dpy, event);
			} else if (event->xclient.message_type == xj_a_XdndDrop) {
				if ((event->xclient.data.l[0] != XGetSelectionOwner(dpy, xj_a_XdndSelection))
						|| (event->xclient.data.l[0] != dnd_source))
				{
					if (!want_quiet)
						fprintf(stderr,"[x11] DnD owner mismatch.");
					break;
				}
				if (xj_atom != None) {
					if(want_debug) printf("DnD Drop\n");
					XConvertSelection(dpy, xj_a_XdndSelection, xj_atom, xj_a_XdndSelection, win, CurrentTime);
				} else {
					if(want_debug) printf("DnD Drop Ignored\n");
				}
				SendFinished(dpy, win);
			} else {
				handled = 0;
			}
			break;
		default:
			handled = 0;
			break;
	}
	return handled;
}

# endif /* X11 */
# endif /* DND */
