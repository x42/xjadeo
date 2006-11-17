/* xjadeo - jack video monitor
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
 *
 * (c) 2006 
 *  Robin Gareus <robin@gareus.org>
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 *
 * this file was inspired by playdv source code of http://libdv.sourceforge.net/.
 *  - (c) 2000 Charles 'Buck' Krasic 
 *  - (c) 2000 Erik Walthinsen 
 *
 * EWMH fullscreen code - from mplayer 
 *  - Strasser, Alexander (beastd) <eclipse7@gmx.net>  ?!
 */

#include "xjadeo.h"
#include "display.h"

#define DND // drag-drop support
#define IMC // cache imlib2 image


#if (HAVE_LIBXV || HAVE_IMLIB || HAVE_IMLIB2)

Display      		*xj_dpy = NULL;
Window			xj_rwin, xj_win;
int			xj_screen; 
GC           		xj_gc;
Atom      		xj_del_atom; 
int			xj_ontop = 0;  
int			xj_fullscreen = 0;  
int			xj_mouse = 0; 

int          		xj_dwidth, xj_dheight; // cache window size for rendering currently only Xv
int			xj_box[4]; // letterbox site - currently only Xv


/*******************************************************************************
 * common X11 code 
 */

/* blatantly ripped off mplayer's libvo/x11_common.c - THX. */
static int x11_get_property(Atom type, Atom ** args, unsigned long *nitems) {
	int format;
	unsigned long bytesafter;
	return (Success == XGetWindowProperty(xj_dpy, xj_rwin, type, 0, 16384, False, AnyPropertyType, &type, &format, nitems, &bytesafter, (unsigned char **) args) && *nitems > 0);
}

#define NET_WM_STATE_TEST(ARGX,ARGY) { \
	Atom type= XInternAtom(xj_dpy, ARGX,0);\
	if (atom == type) { \
		if (!want_quiet) fprintf(stderr,"[x11] Detected wm supports " #ARGX " state.\n" );\
		return ARGY; } }

static int net_wm_support_state_test(Atom atom) {
	NET_WM_STATE_TEST("_NET_WM_STATE_FULLSCREEN",1);
	NET_WM_STATE_TEST("_NET_WM_STATE_ABOVE",2);
	NET_WM_STATE_TEST("_NET_WM_STATE_STAYS_ON_TOP",4);
	return 0;
}

void check_wm_atoms(void) {
	Atom *args;
	int i;
	int wm=0;
	unsigned long nitems;
	if (x11_get_property(XInternAtom(xj_dpy, "_NET_SUPPORTED",0), &args, &nitems)) {
		if (!want_quiet) fprintf(stderr,"[x11] Detected wm supports NetWM.\n");
		for (i = 0; i < nitems; i++) wm |= net_wm_support_state_test(args[i]);
	}
	XFree(args);
}

static void net_wm_set_property(Window window, char *atom, int state) {
	XEvent xev;
	int set = _NET_WM_STATE_ADD;
	Atom type, property;

	if (state == _NET_WM_STATE_TOGGLE) set = _NET_WM_STATE_TOGGLE;
	else if (!state) set = _NET_WM_STATE_REMOVE;

	type = XInternAtom(xj_dpy,"_NET_WM_STATE", 0);
	property = XInternAtom(xj_dpy,atom, 0);

	xev.type = ClientMessage;
	xev.xclient.type = ClientMessage;
	xev.xclient.window = window ; //xj_win;
	xev.xclient.message_type = type;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = set;
	xev.xclient.data.l[1] = property;
	xev.xclient.data.l[2] = 0;
	
        if (!XSendEvent(xj_dpy, DefaultRootWindow(xj_dpy), False,
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

void xj_set_hints (void) {
	XTextProperty	x_wname, x_iname;
	XSizeHints	hints;
	XWMHints	wmhints;
	char *w_name ="xjadeo";
	char *i_name ="xjadeo";

    /* default settings which allow arbitraray resizing of the window */
	hints.flags = PSize | PMaxSize | PMinSize;
	hints.min_width = movie_width / 16;
	hints.min_height = movie_height / 16;

    /* maximum dimensions for Xv support are about 2048x2048 */
	hints.max_width = 2048;
	hints.max_height = 2048;

	wmhints.input = True;
#ifdef HAVE_XPM
	XpmCreatePixmapFromData(xj_dpy, xj_rwin, xjadeo_color_xpm, &wmhints.icon_pixmap, &wmhints.icon_mask, NULL);
#else
	wmhints.icon_pixmap = XCreateBitmapFromData(xj_dpy, xj_rwin, (char *)xjadeo_bits , xjadeo_width, xjadeo_height);
	wmhints.icon_mask  = XCreateBitmapFromData(xj_dpy, xj_rwin, (char *)xjadeo_mask_bits , xjadeo_mask_width, xjadeo_mask_height);
#endif
	wmhints.flags = InputHint | IconPixmapHint | IconMaskHint ;// | StateHint

	XStringListToTextProperty(&w_name, 1 ,&x_wname);
	XStringListToTextProperty(&i_name, 1 ,&x_iname);

	XSetWMProperties(xj_dpy, xj_win, &x_wname, &x_iname, NULL, 0, &hints, &wmhints, NULL);
}

void xj_set_ontop (int action) {
	if (action==2) xj_ontop^=1;
	else xj_ontop=action;
	net_wm_set_property(xj_win, "_NET_WM_STATE_ABOVE", action); 
//	net_wm_set_property(xj_win, "_NET_WM_STATE_STAYS_ON_TOP", action);
}

void xj_set_fullscreen (int action) {
	if (action==2) xj_fullscreen^=1;
	else xj_fullscreen=action;
	net_wm_set_property(xj_win, "_NET_WM_STATE_FULLSCREEN", action);
}

/* also from mplayer's libvo/x11_common.c - thanks GPL !*/
void xj_hidecursor (void) {
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black, dummy;
	Colormap colormap;
	static char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	colormap = DefaultColormap(xj_dpy, xj_screen);
	if (!XAllocNamedColor(xj_dpy, colormap, "black", &black, &dummy) ) return; 
	bm_no = XCreateBitmapFromData(xj_dpy, xj_win, bm_no_data, 8, 8);
	no_ptr = XCreatePixmapCursor(xj_dpy, bm_no, bm_no, &black, &black, 0, 0);
	XDefineCursor(xj_dpy, xj_win, no_ptr);
	XFreeCursor(xj_dpy, no_ptr);
	if (bm_no != None) XFreePixmap(xj_dpy, bm_no);
	XFreeColors(xj_dpy,colormap,&black.pixel,1,0);
}


void xj_showcursor (void) {
	XDefineCursor(xj_dpy,xj_win, 0);
}


#if 1
void xj_get_window_pos (int *x,  int *y) {
	XWindowAttributes attr;
	XGetWindowAttributes(xj_dpy, xj_win, &attr);
	if (x) *x=attr.x;
	if (y) *y=attr.y;
}
#else // old X11 debug code
void xj_get_window_pos (int *x,  int *y) {
	unsigned int dummy_u0, dummy_u1;
	unsigned int dummy_W, dummy_H;
	Window dummy_w;
	// this returns the position of the video in the xjadeo-window
	// should return the pos of the xjadeo-window relative to the root window (desktop)
	XGetGeometry(xj_dpy, xj_win, &dummy_w, x,y, &dummy_W, &dummy_H,&dummy_u0,&dummy_u1);
}
#endif

void xj_get_window_size (unsigned int *my_Width, unsigned int *my_Height) {
	int dummyX,dummyY;
	unsigned int dummy_u0, dummy_u1;
	Window dummy_w;
	XGetGeometry(xj_dpy, xj_win, &dummy_w, &dummyX,&dummyY,my_Width,my_Height,&dummy_u0,&dummy_u1);
}

void xj_resize (unsigned int x, unsigned int y) { 
	XResizeWindow(xj_dpy, xj_win, x, y);
}

void xj_position (int x, int y) { 
	XMoveWindow(xj_dpy, xj_win,x,y);
}

void xj_letterbox() {
	if (!want_letterbox) { /* scale */
		xj_box[0]=xj_box[1]=0;
		xj_box[2]=xj_dwidth;
		xj_box[3]=xj_dheight;
	} else { /* letterbox; */
		float asp_src= (float)movie_width/movie_height;
		float asp_dst= (float)xj_dwidth/xj_dheight;
		if (asp_dst > asp_src ) {
			xj_box[3]=xj_dheight;
			xj_box[2]=xj_box[3]*asp_src;
		} else {
			xj_box[2]=xj_dwidth;
			xj_box[3]=xj_box[2]/asp_src;
		}
		xj_box[0]=(xj_dwidth-xj_box[2])/2;
		xj_box[1]=(xj_dheight-xj_box[3])/2;
	}
}

/*
void set_x11_icon_name(unsigned char *icon) {
	XTextProperty text_prop;
	text_prop.value = icon;
	text_prop.nitems = strlen((char*)icon);
	text_prop.encoding = XA_STRING;
	text_prop.format = 8;
	XSetWMIconName(xj_dpy, xj_win, &text_prop);
	XFlush(xj_dpy);
}
*/

/*******************************************************************************
 * Drag and Drop - common X11 code
 */
# ifdef DND 
  Atom 			xj_a_XdndDrop;   
  Atom 			xj_a_XdndFinished;   
  Atom 			xj_a_XdndActionCopy;   
  Atom 			xj_a_XdndLeave;   
  Atom 			xj_a_XdndPosition;   
  Atom 			xj_a_XdndStatus;   
  Atom 			xj_a_XdndEnter;   
  Atom 			xj_a_XdndAware;   
  Atom 			xj_a_XdndTypeList;   
  Atom 			xj_a_XdndSelection;   
  Atom			xj_atom;
  int 			dnd_source;
  const int 		xdnd_version = 5;

void HandleEnter(XEvent * xe) {
	long *l = xe->xclient.data.l;
    	xj_atom= None;
	Atom ok = XInternAtom(xj_dpy, "text/uri-list", False);

	int version = (int)(((unsigned long)(l[1])) >> 24);
	if (version > xdnd_version) return;
	dnd_source = l[0];

        if (l[1] & 1) {
		Atom type = 0;
		int f,ll;
		unsigned long n, a;
		unsigned char *data;
		int offset = 0;
		a=1;
      		while(a && xj_atom== None){
			XGetWindowProperty(xj_dpy, dnd_source, xj_a_XdndTypeList, offset,
			256, False, XA_ATOM, &type, &f,&n,&a,&data);
				if(data == NULL || type != XA_ATOM || f != 8*sizeof(Atom)){
				XFree(data);
				return;
			}
			for (ll=0; ll<n; ll++) {
				//if (data[ll]!=None)
				//	printf("DEBUG atom:%s\n", XGetAtomName(xj_dpy,data[ll]));
				if (data[ll] == ok) {
					xj_atom= ok;
					break;
				}
			}
			if (data) XFree(data);
		}
	} else {
		int i;
		for(i=2; i < 5; i++) {
			//if(l[i]!=None)
			//	printf("DEBUG atom:%s\n", XGetAtomName(xj_dpy,l[i]));
			if (l[i] == ok) xj_atom= ok;
		}
	}
	if (want_debug)
		printf("DEBUG: DND ok: %i\n",xj_atom==ok);
}

void SendStatus (XEvent * xe) {
	unsigned int w,h;
	XClientMessageEvent response;
	response.type = ClientMessage;
	response.window = xj_win;
	response.format = 32;
	response.message_type = xj_a_XdndStatus;
	response.data.l[0] = xj_win;

	response.data.l[1] = 0x3; // bit0: accept ; bit1: want_position

//	response.data.l[2] = 0; // x, y
	response.data.l[2] = xe->xclient.data.l[2]; // x, y

//	response.data.l[3] = (1024<<16) | (768&0xFFFFUL); // w, h 
//	response.data.l[3] = (1<<16) | (1&0xFFFFUL); // w, h 
	xj_get_window_size(&w,&h);
	response.data.l[3] = (w<<16) | (h&0xFFFFUL); // w, h 

	response.data.l[4] = xj_a_XdndActionCopy; // action
//	response.data.l[4] = xe->xclient.data.l[4];

	XSendEvent(xj_dpy, dnd_source, False, NoEventMask, (XEvent*)&response);
#if 0
	int x,y;
	x= xe->xclient.data.l[2] >>16;
	y= xe->xclient.data.l[2] & 0xFFFFUL;
	w= response.data.l[3] >>16;
	h= response.data.l[3] & 0xFFFFUL;
	printf("x:%iy:%iw:%i:h:%i\n",x,y,w,h);
#endif
}

void SendFinished (void) {
	XClientMessageEvent finished;
	finished.type = ClientMessage;
	finished.display = xj_dpy;
	finished.window = dnd_source; 
	finished.format = 32;
	finished.message_type = xj_a_XdndFinished;
	finished.data.l[0] = xj_win;
	finished.data.l[1] = (1)?1:0; // flags - isAccepted ? sure.
	finished.data.l[2] = 0; // action atom
	finished.data.l[3] = 0; 
	finished.data.l[4] = 0;
	XSendEvent(xj_dpy, dnd_source, False, NoEventMask, (XEvent*)&finished);
}

#define MAX_DND_FILES 64
void xapi_open(void *d); 

void getDragData (XEvent *xe) {
	Atom type;
	int f;
	unsigned long n, a;
	unsigned char *data;

	XGetWindowProperty(xj_dpy, xe->xselection.requestor,
            xe->xselection.property, 0, 65536, 
	    True, xj_atom, &type, &f, &n, &a, &data);

	SendFinished();

	if (!data){
		fprintf(stderr, "WARNING: drag-n-drop - no data\n"); 
		return;
	}

	/* Handle dropped files */
	char * retain = (char*)data;
	char * files[MAX_DND_FILES];
	int num = 0;

	while(retain < ((char *) data) + n) {
		int nl = 0;
		if (!strncmp(retain,"file:",5)) { retain+=5; }
		files[num++]=retain;

		while(retain < (((char *)data) + n)){
			if(*retain == '\r' || *retain == '\n'){
				*retain=0;
				nl = 1;
			} else if (nl) break;
			retain++;
		}

		if (num >= MAX_DND_FILES)
			break;
	}

	if (want_debug)
		for (f=0;f<num;f++) {
			printf("drag-n-drop: recv: %i '%s'\n",f,files[f]);
		}
	{ //translate %20 -> to whitespaces, etc.
		char *t=files[0];
		while ((t=strchr(t,'%')) && strlen(t)>2) {
			int ti=0;
			char tc= t[3];
			t[3]=0; ti=(int)strtol(&(t[1]),NULL,16); t[3]=tc; 
			memmove(t+1,t+3,strlen(t)-2);
			tc=(char)ti; *t=tc; t[strlen(t)]='\0';
			t++;
		}
	}
	if (num>0) xapi_open(files[0]);
	free(data);
}

void init_dnd () {
	Atom atm = (Atom)xdnd_version;
	if ((xj_a_XdndDrop = XInternAtom (xj_dpy, "XdndDrop", True)) != None && 
	    (xj_a_XdndLeave = XInternAtom (xj_dpy, "XdndLeave", True)) != None && 
	    (xj_a_XdndEnter = XInternAtom (xj_dpy, "XdndEnter", True)) != None && 
	/*  (xj_uri_atom = XInternAtom (xj_dpy, "text/uri-list", True)) != None &&  */
	    (xj_a_XdndActionCopy = XInternAtom (xj_dpy, "XdndActionCopy", True)) != None && 
	    (xj_a_XdndFinished = XInternAtom (xj_dpy, "XdndFinished", True)) != None && 
	    (xj_a_XdndPosition = XInternAtom (xj_dpy, "XdndPosition", True)) != None && 
	    (xj_a_XdndStatus = XInternAtom (xj_dpy, "XdndStatus", True)) != None && 
	    (xj_a_XdndTypeList = XInternAtom (xj_dpy, "XdndTypeList", True)) != None && 
	    (xj_a_XdndSelection = XInternAtom (xj_dpy, "XdndSelection", True)) != None && 
	    (xj_a_XdndAware = XInternAtom (xj_dpy, "XdndAware", True)) != None  ) {
	    	if(!want_quiet) printf("enabled drag-DROP support.\n");
		XChangeProperty(xj_dpy, xj_win, xj_a_XdndAware, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atm, 1);
	}
}

void disable_dnd () {
	XDeleteProperty(xj_dpy, xj_win, xj_a_XdndAware);
}

# endif /* DND */


/*******************************************************************************
 * X event callback handler
 */


extern const vidout VO[];
extern int VOutput;

inline void xj_render () { 
	/* this is the only reference to the global buffer..
	 * buffer = mybuffer (so far no shared mem or sth) */
	VO[VOutput].render(buffer); 
}

void xj_handle_X_events (void) {
	XEvent event;
//	XSynchronize(xj_dpy,False );
//	XLockDisplay(xj_dpy);
	while(XPending(xj_dpy)) {
		XNextEvent(xj_dpy, &event);
		switch (event.type) {
			case Expose:
				xj_render();
				break;
			case SelectionRequest:
				break;
			case SelectionNotify:
#ifdef DND
				getDragData(&event);
#endif
				break;
			case ClientMessage:
#ifdef DND
		       	//	fprintf(stdout, "event client: %i\n",event.xclient.message_type);
				if (event.xclient.message_type == xj_a_XdndPosition) {
					if (xj_atom!= None) SendStatus(&event);
				} else if (event.xclient.message_type == xj_a_XdndLeave) {
					if (want_debug) printf("DND LEAVE!\n");
				} else if (event.xclient.message_type == xj_a_XdndEnter) {
					HandleEnter(&event);
				} else if (event.xclient.message_type == xj_a_XdndDrop) {
					if ((event.xclient.data.l[0] != XGetSelectionOwner(xj_dpy, xj_a_XdndSelection))
					    || (event.xclient.data.l[0] != dnd_source)){
					    	if (!want_quiet)
							fprintf(stderr,"[x11] DnD owner mismatch.");
					}
					if(want_debug) printf("DROP!\n");
    					if (xj_atom!= None) {
						XConvertSelection(xj_dpy, xj_a_XdndSelection, xj_atom, xj_a_XdndSelection, xj_win, CurrentTime);
					}
				//	SendFinished(); // called by getDragData(..)
				} else 
#endif
				if (event.xclient.data.l[0] == xj_del_atom) {
				//	fprintf(stdout, "Window destoyed...\n");
					loop_flag = 0;
#if 0
				} elseif (event.xclient.data.l[0] == xj_a_TakeFocus)  {
					;
#endif
				} else {
		         	//	fprintf(stdout, "unhandled X-client event: %ld\n",(long) event.xclient.message_type);
				}
				break;
			case ConfigureNotify: // from XV only 
				{
					unsigned int my_Width,my_Height;
					xj_get_window_size(&my_Width,&my_Height);
					xj_dwidth= my_Width;
					xj_dheight= my_Height;
					xj_letterbox();
				}
				if (VOutput == 1) // only XV
					xj_render();
				break;
#if 0
			case VisibilityNotify:
				if (event.xvisibility.state == VisibilityUnobscured) {
			//		fprintf(stdout, "VisibilityUnobscured!\n");
					loop_run=1;
				}
				else if (event.xvisibility.state == VisibilityPartiallyObscured) {
			//		fprintf(stdout, "Visibility Partly Unobscured!\n");
					loop_run=1;
				}
				else {
					loop_run=0;
			//		fprintf(stdout, "Visibility Hidden!\n");
				}
				break;
#endif
			case MapNotify: 
			//	fprintf(stdout, "Window (re)mapped - enable Video.\n");
				loop_run=1;
				break;
			case UnmapNotify: 
			//	fprintf(stdout, "Window unmapped/minimized - disabled Video.\n");
				loop_run=0;
				break;
			case ButtonPress:
				break;
			case ButtonRelease:
				if (event.xbutton.button == 1) {
					xj_resize(movie_width, movie_height);
				} else {
					unsigned int my_Width,my_Height;
					xj_get_window_size(&my_Width,&my_Height);


					if (event.xbutton.button == 4 && my_Height > 32 && my_Width > 32)  {
						float step=sqrt((float)my_Height);
						my_Width-=floor(step*((float)movie_width/(float)movie_height));
						my_Height-=step;
					//	xj_resize(my_Width, my_Height);
					}
					if (event.xbutton.button == 5) {
						float step=sqrt((float)my_Height);
						my_Width+=floor(step*((float)movie_width/(float)movie_height));
						my_Height+=step;
					}

					// resize to match movie aspect ratio
					if( ((float)movie_width/(float)movie_height) < ((float)my_Width/(float)my_Height) )
						my_Width=floor((float)my_Height * (float)movie_width / (float)movie_height);
					else my_Height=floor((float)my_Width * (float)movie_height / (float)movie_width);

					xj_resize(my_Width, my_Height);
				}
//				fprintf(stdout, "Button %i release event.\n", event.xbutton.button);

				if (VOutput == 1) // only XV
					xj_render();
				break;
			case KeyPress:
				{
					int key;
					KeySym keySym;
					char buf[100];
					static XComposeStatus stat;

					XLookupString(&event.xkey, buf, sizeof(buf), &keySym, &stat);
					key = ((keySym & 0xff00) != 0 ? ((keySym & 0x00ff) + 256) : (keySym));

					if     (key == 0x11b ) loop_flag=0; // 'Esc'
					else if (key == 0x71 ) loop_flag=0; // 'q'
					else if (key == 0x61 ) xj_set_ontop(xj_ontop^=1); //'a'
					else if (key == 0x66 ) xj_set_fullscreen(xj_fullscreen^=1); //'f' // fullscreen
					else if (key == 0x6d ) { 	// 'm'
					    if (xj_mouse^=1) xj_hidecursor(); else xj_showcursor();
					} else if (want_debug) {
						printf("unassigned key pressed: '%c' 0x%x\n",key,key);
					}
				}
				break;
			case ReparentNotify:
				break;
			default:
			/* TODO: I get Xevents type 94 a lot - 
			 * no what could that be ?  */
			//	printf("unhandled X event: type: %ld\n",(long) event.type);
				break;
		}
	}
//	XUnlockDisplay(xj_dpy);
}

#endif /* HAVE any of xv, imlib* */


/*******************************************************************************
 * XV !!!
 */
#if HAVE_LIBXV

// TODO: support other YUV Xv - ffmpeg combinations
// (depending on hardware and X) Xv can do more than YV12 ...
#define FOURCC_YV12 0x32315659  /* YV12   YUV420P */
#define FOURCC_I420 0x30323449  /* I420   Intel Indeo 4 */

//#define FOURCC_YUV2 0x32595559  /* YUV2   YUV422 */
//#define FOURCC_UYVY 0x59565955  /* YUV 4:2:2 */



  Screen       		*xv_scn;
  int          		xv_swidth, xv_sheight;

  XEvent       		xv_event;
  XvPortID    	 	xv_port;
  XShmSegmentInfo  	xv_shminfo;
  XvImage      		*xv_image;

  char	 		*xv_buffer;
  size_t		xv_len;
  int			xv_one_memcpy = 0; 
  int			xv_pic_format = FOURCC_I420; 

void allocate_xvimage (void) {
	// YV12 has 12 bits per pixel. 8bitY 2+2 UV
	xv_len = movie_width * movie_height * 3 / 2 ;

	/* shared memory allocation etc.  */
	xv_image = XvShmCreateImage(xj_dpy, xv_port,
		xv_pic_format, NULL, // FIXME: use xjadeo buffer directly 
		xj_dwidth, xj_dheight, //768, 486, //720, 576,
		&xv_shminfo);

	/* TODO: check that this does not break support for some VIC's 
	 * let's ship xjadeo w/ xv_one_memcpy=0; - slower(?) but safer(!) 
	 * (maybe the U/V planes are swapped, byte order or whatever...)
	 */ 
#if 0
	if (xv_len != xv_image->data_size) xv_one_memcpy =0; else xv_one_memcpy=1;
#else
	xv_one_memcpy=0;
#endif

	xv_len =  xv_image->data_size;
	xv_shminfo.shmid = shmget(IPC_PRIVATE, xv_len, IPC_CREAT | 0777);
	xv_image->data = xv_buffer = xv_shminfo.shmaddr = shmat(xv_shminfo.shmid, 0, 0);

	XShmAttach(xj_dpy, &xv_shminfo);
	XSync(xj_dpy, False);

	if (xv_shminfo.shmid > 0)
		shmctl (xv_shminfo.shmid, IPC_RMID, 0);
}

void deallocate_xvimage(void) {
	XShmDetach(xj_dpy, &xv_shminfo);
	shmdt(xv_shminfo.shmaddr);
	XFree(xv_image);
	XSync(xj_dpy, False);
	xv_buffer=NULL;
}

void render_xv (uint8_t *mybuffer) {

	if (!xv_buffer || !mybuffer) return;

	size_t Ylen  = movie_width * movie_height;
	size_t UVlen = movie_width * movie_height/4; 
	size_t mw2 = movie_width /2; 
	size_t mh2 = movie_height /2; 

	// decode ffmpeg - YUV 
	uint8_t *Yptr=mybuffer; // Y 
	uint8_t *Uptr=Yptr + Ylen; // U
	uint8_t *Vptr=Uptr + UVlen; // V

	if (xv_pic_format == FOURCC_I420 && xv_one_memcpy) {
		// copy YUV420P 
		memcpy(xv_buffer,mybuffer,Ylen+UVlen+UVlen); // Y+U+V
	} else if (xv_pic_format == FOURCC_I420) {
	
	// encode YV420P
		stride_memcpy(xv_buffer+xv_image->offsets[0],
			Yptr, xv_swidth, xv_sheight, xv_image->pitches[0], xv_swidth);

		stride_memcpy(xv_buffer+xv_image->offsets[1],
			Uptr, mw2, mh2, xv_image->pitches[1], mw2);

		stride_memcpy(xv_buffer+xv_image->offsets[2],
			Vptr, mw2, mh2, xv_image->pitches[2], mw2);
	} else {
	// encode YV12
		stride_memcpy(xv_buffer+xv_image->offsets[0],
			Yptr, xv_swidth, xv_sheight, xv_image->pitches[0], xv_swidth);

		stride_memcpy(xv_buffer+xv_image->offsets[1],
			Vptr, mw2, mh2, xv_image->pitches[1], mw2);

		stride_memcpy(xv_buffer+xv_image->offsets[2],
			Uptr, mw2, mh2, xv_image->pitches[2], mw2);
	}

	XvShmPutImage(xj_dpy, xv_port,
		xj_win, xj_gc,
		xv_image,
		0, 0,				/* sx, sy */
		xv_swidth, xv_sheight,		/* sw, sh */
#if 1 // letterbox
		xj_box[0],xj_box[1],xj_box[2],xj_box[3],
#else
		0, 0,				/* dx, dy */
		xj_dwidth, xj_dheight,		/* dw, dh */
#endif
		True);
	XFlush(xj_dpy);
// 	XSync(xj_dpy,False);
}

void newsrc_xv (void) {
	deallocate_xvimage();

  	xj_dwidth = xv_swidth = movie_width;
	xj_dheight = xv_sheight = movie_height;
	xj_letterbox();
	allocate_xvimage();
	xj_render();

	unsigned int my_Width,my_Height;
#if 1 // keep current window size when loading a new file ?? -> TODO config option
      // and also other video modes.. 
	xj_get_window_size(&my_Width,&my_Height);
#else
	my_Width=movie_width;
	my_Height=movie_height;
#endif
	// or just update xj_dwidth, xj_dheight
	xj_resize( my_Width, my_Height);
}

int open_window_xv (void) {

	unsigned int 	ad_cnt;
	int fmt_cnt, got_port, i, k;
	int xv_have_YV12, xv_have_I420;
	long ev_mask;

	XGCValues	values;

	XvAdaptorInfo	*ad_info;
	XvImageFormatValues	*fmt_info;

	if ( (xj_dpy=XOpenDisplay(NULL)) == NULL ) {
		fprintf( stderr, "Cannot connect to X server\n");
		return (1); 
	}

	xj_rwin = DefaultRootWindow(xj_dpy);
	xj_screen = DefaultScreen(xj_dpy);

	if (!XShmQueryExtension(xj_dpy)) return 1;

	/* So let's first check for an available adaptor and port */
	if(Success != XvQueryAdaptors(xj_dpy, xj_rwin, &ad_cnt, &ad_info)) {
    		/* Xv extension probably not present */
		return (1);
	}

	for(i = 0, got_port = False; i < ad_cnt; ++i) {
		if (!want_quiet) fprintf(stdout,
			"Xv: %s: ports %ld - %ld\n",
			ad_info[i].name,
			ad_info[i].base_id,
			ad_info[i].base_id +
			ad_info[i].num_ports - 1);

		if (!(ad_info[i].type & XvImageMask)) {
			if (want_verbose) fprintf(stdout,
				"Xv: %s: XvImage NOT in capabilty list (%s%s%s%s%s )\n",
				ad_info[i].name,
				(ad_info[i].type & XvInputMask) ? " XvInput"  : "",
				(ad_info[i]. type & XvOutputMask) ? " XvOutput" : "",
				(ad_info[i]. type & XvVideoMask)  ?  " XvVideo"  : "",
				(ad_info[i]. type & XvStillMask)  ?  " XvStill"  : "",
				(ad_info[i]. type & XvImageMask)  ?  " XvImage"  : "");
			continue;
		}

		fmt_info = XvListImageFormats(xj_dpy, ad_info[i].base_id,&fmt_cnt);
		if (!fmt_info || fmt_cnt == 0) {
			fprintf(stderr, "Xv: %s: NO supported formats\n", ad_info[i].name);
			continue;
		} 

		for(xv_have_YV12=0, xv_have_I420=0, k=0; k < fmt_cnt; ++k) {
			if (want_debug) {
				fprintf(stderr, "INFO: Xvideo port %d: 0x%#08x (%c%c%c%c) %s",
					(int)xv_port,
					fmt_info[k].id,
					(fmt_info[k].id)      & 0xff,
					(fmt_info[k].id >  8) & 0xff,
					(fmt_info[k].id > 16) & 0xff,
					(fmt_info[k].id > 24) & 0xff,
					(fmt_info[k].format == XvPacked) ? "packed" : "planar");

				fprintf (stderr, " [%s]\n", fmt_info[k].guid);
			}
			if (FOURCC_YV12 == fmt_info[k].id) xv_have_YV12 = 1; 
			if (FOURCC_I420 == fmt_info[k].id) xv_have_I420 = 1; 
		}

		if (xv_have_I420) {
			xv_pic_format = FOURCC_I420;
			if (!want_quiet) 
				fprintf(stderr,"XV: using YUV420P + Xvideo extention (I420)\n");
		} else if (xv_have_YV12) { 
			xv_pic_format = FOURCC_YV12;
			if (!want_quiet) 
				fprintf(stderr,"XV: using YUV420P + Xvideo extention (YV12)\n");
		} else {
			fprintf(stderr,
				"Xv: %s: could not find a suitable colormodel in ( ", ad_info[i].name);
			for (k = 0; k < fmt_cnt; ++k) {
				fprintf (stderr, "%#08x[%s] ", fmt_info[k].id, fmt_info[k].guid);
			}
			fprintf(stderr, ")\n");
			continue;
		} 

		for(xv_port = ad_info[i].base_id, k = 0; k < ad_info[i].num_ports; ++k, ++(xv_port)) {
			if(!XvGrabPort(xj_dpy, xv_port, CurrentTime)) {
				if (want_verbose) fprintf(stdout, "Xv: grabbed port %ld\n", xv_port);
				got_port = True;
				break;
			} 
		} 
		if(got_port) break;
	} /* for */


	if(!ad_cnt) {
		fprintf(stderr, "Xv: (ERROR) no adaptor found!\n");
		return 1;
	}
	if(!got_port) {
		fprintf(stderr, "Xv: (ERROR) could not grab any port!\n");
		return 1;
	}
	
/*
 * default settings: source, destination and logical widht/height
 * are set to our well known dimensions.
 */
	xj_dwidth = xv_swidth = movie_width;
	xj_dheight = xv_sheight = movie_height;
	xj_letterbox();

	xj_win = XCreateSimpleWindow(xj_dpy, xj_rwin,
			0, 0,
			xj_dwidth, xj_dheight,
			0,
			XWhitePixel(xj_dpy, xj_screen),
			XBlackPixel(xj_dpy, xj_screen));

//	XmbSetWMProperties(xj_dpy, xj_win, "xjadeo", NULL, NULL, 0, NULL, NULL, NULL);
	xj_set_hints();

	ev_mask =  KeyPressMask | ButtonPressMask | ButtonReleaseMask |
			ExposureMask | StructureNotifyMask; //| PropertyChangeMask | PointerMotionMask;
	XSelectInput(xj_dpy, xj_win, ev_mask);

#ifdef DND
	init_dnd();
#endif
	XMapRaised(xj_dpy, xj_win);

	if ((xj_del_atom = XInternAtom(xj_dpy, "WM_DELETE_WINDOW", True)) != None)
	XSetWMProtocols(xj_dpy, xj_win, &xj_del_atom, 1);

	xj_gc = XCreateGC(xj_dpy, xj_win, 0, &values);

	allocate_xvimage();

	check_wm_atoms();
	if (start_ontop) xj_ontop=1;
	if (start_fullscreen) xj_fullscreen=1;
	xj_set_fullscreen(xj_fullscreen);
	xj_set_ontop(xj_ontop);

	return 0;
}

void close_window_xv(void) {
	//XvFreeAdaptorInfo(ai);
	XvStopVideo(xj_dpy, xv_port, xj_win);
	if(xv_shminfo.shmaddr) {
		XShmDetach(xj_dpy, &xv_shminfo);
		shmdt(xv_shminfo.shmaddr);
	}
	if(xv_image) XFree(xv_image);
	XSync(xj_dpy, False);
	XFreeGC(xj_dpy, xj_gc);
	if (!loop_flag) // TODO: do 'DestroyAll' during shutdown()
		XSetCloseDownMode(xj_dpy, DestroyAll);
	XCloseDisplay(xj_dpy);
}

#if 1 // LEGACY 

void handle_X_events_xv (void) {
	xj_handle_X_events();
}

void get_window_size_xv (unsigned int *my_Width, unsigned int *my_Height) {
	xj_get_window_size(my_Width,my_Height);
}

void get_window_pos_xv (int *x,  int *y) {
	xj_get_window_pos(x,y); 
}

void resize_xv (unsigned int x, unsigned int y) { 
	xj_resize(x, y);
}

void position_xv (int x, int y) { 
	xj_position(x, y);
}
#endif

#endif /* HAVE_LIBXV */



/*******************************************************************************
 * X11 / ImLib 
 */

#if HAVE_IMLIB

ImlibData *imlib = NULL;
int       depth;
//XImage    *image;
Pixmap    pxm, pxmmask;

int open_window_imlib (void) {
	XGCValues values;
	long ev_mask;

	if ( (xj_dpy=XOpenDisplay(NULL)) == NULL ) {
		fprintf( stderr, "Cannot connect to X server\n");
		return (1); 
	}

	imlib = Imlib_init(xj_dpy);
  
	xj_screen = DefaultScreen(xj_dpy); 
	xj_rwin = RootWindow(xj_dpy, xj_screen);
	depth = DefaultDepth(xj_dpy, xj_screen);
  
	xj_win = XCreateSimpleWindow(
		xj_dpy, xj_rwin,
		0,             // x
		0,             // y
		movie_width,   // width
		movie_height,  // height
		0,             // border
		BlackPixel(xj_dpy, xj_screen), 
		WhitePixel(xj_dpy, xj_screen)
	);

	xj_set_hints();
  
	ev_mask =  KeyPressMask | ButtonPressMask | ButtonReleaseMask | ExposureMask | StructureNotifyMask;
	XSelectInput(xj_dpy, xj_win, ev_mask);
 
#ifdef DND 
	init_dnd();
#endif
	XMapRaised(xj_dpy, xj_win);
     
	/* express interest in WM killing this app */
	if ((xj_del_atom = XInternAtom(xj_dpy, "WM_DELETE_WINDOW", True)) != None)
		XSetWMProtocols(xj_dpy, xj_win, &xj_del_atom, 1);

	xj_gc = XCreateGC(xj_dpy, xj_win, 0, &values);

	check_wm_atoms();
	if (start_ontop) xj_ontop=1;
	if (start_fullscreen) xj_fullscreen=1;
	xj_set_fullscreen(xj_fullscreen);
	xj_set_ontop(xj_ontop);

	return 0;
}

void close_window_imlib(void)
{
//	XSync(xj_dpy, True);

	if (loop_flag)
		XSetCloseDownMode(xj_dpy, RetainPermanent);
	else
		XSetCloseDownMode(xj_dpy, DestroyAll);
	XDestroyWindow(xj_dpy, xj_win);
	XFreeGC(xj_dpy, xj_gc);
	XSync(xj_dpy, False);
//	XCloseDisplay(xj_dpy);
	imlib=NULL;  
}
        
void render_imlib (uint8_t *mybuffer) {
	unsigned int my_Width,my_Height;
	ImlibImage *iimage;
	if (!mybuffer || !imlib) return;
	iimage = Imlib_create_image_from_data(imlib, mybuffer, NULL, movie_width, movie_height);

    /* get the current window size */
	xj_get_window_size(&my_Width,&my_Height);

    /* Render the original 24-bit Image data into a pixmap of size w * h */
	Imlib_render(imlib,iimage, my_Width,my_Height );
	//Imlib_render(imlib,iimage, my_Width-20,my_Height-20 );

    /* Extract the Image and mask pixmaps from the Image */
	pxm=Imlib_move_image(imlib,iimage);
    /* The mask will be 0 if the image has no transparency */
//	pxmmask=Imlib_move_mask(imlib,iimage);
    /* Put the Image pixmap in the background of the window */
	XSetWindowBackgroundPixmap(xj_dpy,xj_win,pxm);       
//	XPutImage(xj_dpy,xj_win,xj_gc,img, 0,0,0,0, my_Width, my_Height);
	XClearWindow(xj_dpy,xj_win);       
    /* No need to sync. XPending will take care in the event loop. */
//	XSync(display, True);     
	Imlib_free_pixmap(imlib, pxm);
	Imlib_kill_image(imlib, iimage);
}

void newsrc_imlib (void) { 
	; // nothing to do :)
}

void handle_X_events_imlib (void) {
	xj_handle_X_events();
}

#if 1 // LEGACY CODE

void get_window_pos_imlib (int *x,  int *y) {
	xj_get_window_pos(x,y); 
}

void get_window_size_imlib (unsigned int *my_Width, unsigned int *my_Height) {
	xj_get_window_size(my_Width,my_Height);
}

void resize_imlib (unsigned int x, unsigned int y) { 
	xj_resize(x, y);
}

void position_imlib (int x, int y) { 
	xj_position(x, y);
}

#endif

#endif /* HAVE_IMLIB */


/*******************************************************************************
 *
 * X11 / ImLib2 
 */

#if HAVE_IMLIB2

int       im_depth;
Visual    *im_vis;
Colormap  im_cm;

int open_window_imlib2 (void) {
	XGCValues values;
	long ev_mask;

	if ( (xj_dpy=XOpenDisplay(NULL)) == NULL ) {
		fprintf( stderr, "Cannot connect to X server\n");
		return (1); 
	}
 
	xj_screen = DefaultScreen(xj_dpy); 
	xj_rwin = RootWindow(xj_dpy, xj_screen); 
	im_depth = DefaultDepth(xj_dpy, xj_screen);
	im_vis = DefaultVisual(xj_dpy, xj_screen);
	im_cm = DefaultColormap(xj_dpy, xj_screen);

	xj_win = XCreateSimpleWindow(
		xj_dpy, xj_rwin,
		0,             // x
		0,             // y
		movie_width,   // width
		movie_height,  // height
		0,             // border
		BlackPixel(xj_dpy, xj_screen), 
		WhitePixel(xj_dpy, xj_screen)
	);

	xj_set_hints();
         
	ev_mask =  KeyPressMask | ButtonPressMask | ButtonReleaseMask | ExposureMask | StructureNotifyMask;
	XSelectInput(xj_dpy, xj_win, ev_mask);
 
#ifdef DND 
	init_dnd();
#endif
	XMapRaised(xj_dpy, xj_win);

	/* express interest in WM killing this app */
	if ((xj_del_atom = XInternAtom(xj_dpy, "WM_DELETE_WINDOW", True)) != None)
		XSetWMProtocols(xj_dpy, xj_win, &xj_del_atom, 1);

	xj_gc = XCreateGC(xj_dpy, xj_win, 0, &values);

	imlib_context_set_display(xj_dpy);
	imlib_context_set_visual(im_vis);
	imlib_context_set_colormap(im_cm);
	imlib_context_set_drawable(xj_win);

	check_wm_atoms();

	if (start_ontop) xj_ontop=1;
	if (start_fullscreen) xj_fullscreen=1;
	xj_set_fullscreen(xj_fullscreen);
	xj_set_ontop(xj_ontop);

	return 0;
}

static Imlib_Image im_image = NULL;

void close_window_imlib2(void)
{
	if (im_image) {
		imlib_context_set_image(im_image);
		imlib_free_image();
		im_image = NULL;
	}
//	XSync(xj_dpy, True);
	if (loop_flag)
		XSetCloseDownMode(xj_dpy, RetainPermanent);
	else
		XSetCloseDownMode(xj_dpy, DestroyAll);
	XDestroyWindow(xj_dpy, xj_win);
	XFreeGC(xj_dpy, xj_gc);
	XSync(xj_dpy, False);
//	XCloseDisplay(xj_dpy);
}


void render_imlib2 (uint8_t *mybuffer) {
	unsigned int my_Width,my_Height;
	Imlib_Image im_scaled = NULL;
	if (!mybuffer) return;

#ifdef IMC
	DATA32 *data;
	if (!im_image) {
		im_image = imlib_create_image(movie_width, movie_height);
		imlib_context_set_image(im_image);
		//imlib_image_set_has_alpha(1); // slows things down dramatically !
	}
	imlib_context_set_image(im_image);
	data=imlib_image_get_data();
#endif /*IMC*/

#ifdef IMLIB2RGBA
# ifndef IMC
	uint8_t * rgbabuf = mybuffer;
# else
	memcpy(data,mybuffer,4*sizeof(uint8_t)*movie_width*movie_height);
# endif 
#else /* rgb24 -> rgba32 */
# ifndef IMC
	uint8_t * rgbabuf = malloc(4*sizeof(uint8_t)*movie_width*movie_height);
# endif

# if defined(__BIG_ENDIAN__)
#  ifndef IMC
	rgb2argb( rgbabuf, mybuffer, movie_width, movie_height);
#  else
	rgb2argb( (uint8_t*) data, mybuffer, movie_width, movie_height);
#  endif
# else /* LITTLE ENDIAN */
#  ifndef IMC
	rgb2abgr( rgbabuf, mybuffer, movie_width, movie_height);
#  else
	rgb2abgr( (uint8_t*) data, mybuffer, movie_width, movie_height);
#  endif
# endif
#endif

#ifdef IMC
	imlib_image_put_back_data(data);
#else
	//im_image = imlib_create_image_using_data(movie_width, movie_height, (DATA32*)rgbabuf);
	im_image = imlib_create_image_using_copied_data(movie_width, movie_height, (DATA32*)rgbabuf);
	//imlib_image_set_has_alpha(1); // beware. SLOW.
#endif

	xj_get_window_size(&my_Width,&my_Height);

	if (im_image) {
		imlib_context_set_image(im_image);
		if (movie_width == my_Width && movie_height== my_Height)  {
			imlib_render_image_on_drawable(0, 0);
		} else {
			im_scaled=imlib_create_cropped_scaled_image(0,0,movie_width, movie_height,my_Width,my_Height);
			imlib_context_set_image(im_scaled);
			//imlib_image_set_has_alpha(0); // beware.
			imlib_render_image_on_drawable(0, 0);
			imlib_free_image();
		}
#ifndef IMC
		imlib_context_set_image(im_image);
		imlib_free_image();
#endif
	}
#ifndef IMLIB2RGBA
# ifndef IMC
	free(rgbabuf);
# endif
#endif
}

void newsrc_imlib2 (void) { 
#ifdef IMC
	if (im_image) {
		imlib_context_set_image(im_image);
		imlib_free_image();
		im_image = NULL;
	}
#endif
}

#if 1 // LEGACY 

void handle_X_events_imlib2 (void) {
	xj_handle_X_events();
}

void get_window_size_imlib2 (unsigned int *my_Width, unsigned int *my_Height) {
	xj_get_window_size(my_Width,my_Height);
}

void get_window_pos_imlib2 (int *x,  int *y) {
	xj_get_window_pos(x,y); 
}

void resize_imlib2 (unsigned int x, unsigned int y) { 
	xj_resize(x, y);
}

void position_imlib2 (int x, int y) { 
	xj_position(x, y);
}
#endif

#endif

