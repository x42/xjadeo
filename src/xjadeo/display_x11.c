/* xjadeo - X11/XVideo display
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

#define IMC // cache imlib2 image
#undef  IM_CUSTOM_COLORTABLE

#ifdef CROPIMG
int xoffset = 0;
#endif

#if (HAVE_LIBXV || HAVE_IMLIB2)

#include <X11/xpm.h>
#include "icons/xjadeo8.xpm"

void xapi_open (void *d);
void xapi_close (void *d);

extern const  vidout VO[];
extern int    OSD_mode; // change via keystroke

static Display *xj_dpy = NULL;
static Window   xj_rwin, xj_win;
static int      xj_screen;
static GC       xj_gc;
static Atom     xj_del_atom;
static int      xj_ontop = 0;
static int      xj_fullscreen = 0;

static int      xj_dwidth, xj_dheight; // cache window size for rendering currently only Xv
static int      xj_box[4]; // letterbox site - currently only Xv & imlib2

/*******************************************************************************
 * common X11 code
 */

static void net_wm_set_property(char *atom, int state) {
	XEvent xev;
	int set = _NET_WM_STATE_ADD;
	Atom type, property;

	if (state == _NET_WM_STATE_TOGGLE) set = _NET_WM_STATE_TOGGLE;
	else if (!state) set = _NET_WM_STATE_REMOVE;

	type = XInternAtom(xj_dpy,"_NET_WM_STATE", True); // was ,0);
	if (type == None) return;
	property = XInternAtom(xj_dpy,atom, 0);
	if (property == None) return;

	xev.type = ClientMessage;
	xev.xclient.type = ClientMessage;
	xev.xclient.window = xj_win;
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

static void xj_set_hints (void) {
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
	XpmCreatePixmapFromData(xj_dpy, xj_rwin, xjadeo8_xpm, &wmhints.icon_pixmap, &wmhints.icon_mask, NULL);
	wmhints.flags = InputHint | IconPixmapHint | IconMaskHint ;// | StateHint

	if (XStringListToTextProperty (&w_name, 1, &x_wname) &&
			XStringListToTextProperty (&i_name, 1 ,&x_iname)) {
		XSetWMProperties (xj_dpy, xj_win, &x_wname, &x_iname, NULL, 0, &hints, &wmhints, NULL);
		XFree (x_wname.value);
		XFree (x_iname.value);
	}
}

void xj_set_ontop (int action) {
	if (action==2) xj_ontop^=1;
	else xj_ontop=action;
	net_wm_set_property("_NET_WM_STATE_ABOVE", action);
}

int xj_get_ontop () {
	return xj_ontop;
}

void xj_set_fullscreen (int action) {
	if (action==2) xj_fullscreen^=1;
	else xj_fullscreen=action;
	net_wm_set_property("_NET_WM_STATE_FULLSCREEN", action);
}

int xj_get_fullscreen () {
	return (xj_fullscreen);
}

static void xj_hidecursor (void) {
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black, dummy;
	const char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	Colormap colormap = DefaultColormap (xj_dpy, xj_screen);

	if (!XAllocNamedColor (xj_dpy, colormap, "black", &black, &dummy)) return;
	bm_no = XCreateBitmapFromData (xj_dpy, xj_win, bm_no_data, 8, 8);
	no_ptr = XCreatePixmapCursor (xj_dpy, bm_no, bm_no, &black, &black, 0, 0);
	XDefineCursor (xj_dpy, xj_win, no_ptr);
	XFreeCursor (xj_dpy, no_ptr);
	if (bm_no != None) XFreePixmap(xj_dpy, bm_no);
	XFreeColors (xj_dpy, colormap, &black.pixel, 1, 0);
}

static void xj_showcursor (void) {
	XDefineCursor(xj_dpy,xj_win, 0);
}

void xj_mousepointer (int action) {
	if (action==2) hide_mouse ^= 1;
	else hide_mouse = action ? 1 : 0;
	if (hide_mouse) xj_hidecursor();
	else xj_showcursor();
}

void xj_get_window_pos (int *rx,  int *ry) {
	Window	dummy;
	XTranslateCoordinates( xj_dpy, xj_win, xj_rwin, 0, 0, rx, ry, &dummy);
	while (dummy !=None) {
		int x = 0;
		int y = 0;
		XTranslateCoordinates( xj_dpy, xj_win, dummy, 0, 0, &x, &y, &dummy);
		if (dummy!=None) {
			(*rx)-=x; (*ry)-=y;
		} else {
			(*rx)+=x; (*ry)+=y;
		}
	}
}

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

void xj_letterbox () {
	if (!want_letterbox) { /* scale */
		xj_box[0]=xj_box[1]=0;
		xj_box[2]=xj_dwidth;
		xj_box[3]=xj_dheight;
	} else { /* letterbox; */
		const float asp_src = movie_aspect?movie_aspect:(float)movie_width/(float)movie_height;
		const float asp_dst = (float)xj_dwidth / (float)xj_dheight;
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
	force_redraw=1;
}

#define EQCLAMP(var) if((var)<-999) { var=-1000;} if((var)>999) { var=1000;}
#define EQMOD(PROP,STEP) if(!xj_get_eq(PROP,&value)){value+=(STEP); EQCLAMP(value); xj_set_eq(PROP,value); force_redraw=1;}

// EQ prototypes
#ifdef HAVE_LIBXV
static int xv_set_eq(char *name, int value);
static int xv_get_eq(char *name, int *value);
#endif
#ifdef HAVE_IMLIB2
static int im2_get_eq(char *name, int *value);
static int im2_set_eq(char *name, int value);
#endif

static inline void xj_render () {
	VO[getvidmode()].render(buffer);
}

int xj_get_eq(char *prop, int *value) {
#ifdef COLOREQ
# ifdef HAVE_LIBXV
	if (getvidmode() == VO_XV) return (xv_get_eq(prop,value));
# endif
# ifdef HAVE_IMLIB2
	if (getvidmode() == VO_X11) return (im2_get_eq(prop,value));
# endif
#endif
	if (value) *value=0;
	return (1); // error
}

static float calc_slider(int x, int y) {
	if (interaction_override&OVR_MENUSYNC) return -1;
	const int bar_x0 = xj_box[0] + (PB_X * xj_box[2] / (float)movie_width);
	const int bar_xw = xj_box[2] - 2 * PB_X;

	const int bar_y1 = xj_box[1] + xj_box[3] * BAR_Y;
	const int bar_y0 = bar_y1 - (PB_H - 4) * xj_box[3] / (float)movie_height;

	if (y > bar_y0 && y < bar_y1 && x >= bar_x0 && x <= bar_x0 + bar_xw) {
		return (100.f * (x - bar_x0) / (float)(bar_xw));
	}
	return -1;
}


static void xj_set_eq (char *prop, int value) {
#ifdef COLOREQ
# ifdef HAVE_LIBXV
	if (getvidmode() == VO_XV) xv_set_eq(prop,value);
# endif
# ifdef HAVE_IMLIB2
	if (getvidmode() == VO_X11) im2_set_eq(prop,value);
# endif
#endif
}

static void xj_handle_X_events (void) {
	XEvent event;
	int value = 0; // used for color-eq
	while(XPending(xj_dpy)) {
		XNextEvent(xj_dpy, &event);
#ifdef XDLG
		if (handle_xdlg_event(xj_dpy, &event)) continue;
#endif
#ifdef XFIB
		if (x_fib_handle_events (xj_dpy, &event)) {
			if (x_fib_status () > 0) {
				char *fn = x_fib_filename ();
				xapi_open (fn);
				free (fn);
			}
		}
#endif
		if (event.xany.window != xj_win) {
			continue;
		}
#ifdef DND
		if (handle_dnd_event(xj_dpy, xj_win, &event)) continue;
#endif
		switch (event.type) {
			case Expose:
				xj_render();
				break;
			case ClientMessage:
				if (event.xclient.data.l[0] == xj_del_atom) {
					if ((interaction_override&OVR_QUIT_WMG) == 0) loop_flag=0;
				}
				break;
			case ConfigureNotify:
				if (event.xconfigure.width > 1 && event.xconfigure.height > 1)
				{
					unsigned int my_Width,my_Height;
					xj_get_window_size(&my_Width,&my_Height);
					xj_dwidth= my_Width;
					xj_dheight= my_Height;
					xj_letterbox();
				}
				if (getvidmode() == VO_XV)
					xj_render();
				break;
			case MapNotify:
				loop_run=1;
				break;
			case UnmapNotify:
				loop_run=0;
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
					if (event.xbutton.x >= 0 && event.xbutton.x < xj_dwidth
							&& event.xbutton.y >= 0 && event.xbutton.y < xj_dheight)
						show_x_dialog(xj_dpy, xj_win,
								event.xbutton.x_root, event.xbutton.y_root
								);
				} else
#endif
				{
					switch (event.xbutton.button) {
						case 2:
							XCresize_aspect(0);
							break;
						case 5:
							XCresize_aspect(-1);
							break;
						case 4:
							XCresize_aspect(1);
							break;
						default:
							break;
					}
				}
				break;
			case KeyPress:
				{
					KeySym key;
					char buf[100];
					static XComposeStatus stat;

					int n = XLookupString(&event.xkey, buf, sizeof(buf), &key, &stat);
					if (event.xkey.state & ControlMask && n == 1 && key == XK_o) {
#ifdef XFIB
						if (!(interaction_override & OVR_LOADFILE)) {
							x_fib_cfg_filter_callback(fib_filter_movie_filename);
							x_fib_show (xj_dpy, xj_win, 0, 0);
						}
#endif
					}
					else if (event.xkey.state & ControlMask && n == 1 && key == XK_w) {
						if (!(interaction_override & OVR_LOADFILE))
							xapi_close (NULL);
					}
					else if (event.xkey.state & ControlMask && n == 1 && key == XK_q) {
						if (!(interaction_override&OVR_QUIT_WMG))
							loop_flag = 0;
					}
					else if (key == XK_Escape) { // 'Esc'
						if ((interaction_override&OVR_QUIT_KEY) == 0) {
							loop_flag=0;
						} else {
							remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
						}
					}
					else if   (key == XK_a ) //'a' // always-on-top
						xj_set_ontop(xj_ontop^=1);
					else if   (key == XK_f ) //'f' // fullscreen
						xj_set_fullscreen(xj_fullscreen^=1);
					else if   (key == XK_e ) {//'e' // toggle OSD EQ config
						OSD_mode^=OSD_EQ;
						if (xj_get_eq("brightness", &value) && xj_get_eq("contrast", &value)) {
							OSD_mode&=~OSD_EQ; // disable if not supported
						}
						force_redraw=1;
					} else if   (key == XK_o ) { //'o' // OSD - offset in frames
						ui_osd_offset_cycle();
					} else if   (key == XK_p ) { //'p' // OSD - position toggle
						ui_osd_permute();
					} else if (key == XK_s ) { //'s' // OSD - current smpte
						ui_osd_tc();
					} else if (key == XK_l ) { //'l' // OSD - letterbox
						want_letterbox=!want_letterbox;
						xj_letterbox();
					} else if (key == XK_v ) { //'v' // OSD - current video frame
						ui_osd_fn();
					} else if (key == XK_b ) { //'b' // OSD - black box
						ui_osd_box();
					} else if (key == XK_i ) { //'i' // Time Into
						ui_osd_fileinfo();
					} else if (key == XK_g ) { //'g' // Geometry Into
						ui_osd_geo();
					} else if (key == XK_r ) { //'r' // OSD - out of range
						ui_osd_outofrange();
					} else if (key == XK_x ) { //'x' // OSD - seek position
						ui_osd_pos();
					} else if (key == XK_C ) { //'C' // OSD - clear all
						ui_osd_clear();
					} else if (key == XK_exclam ) { //'Shift-1' //
						EQMOD("brightness",-8)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_quotedbl || key == XK_at) { //'Shift-2' //
						EQMOD("brightness",+8)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_numbersign || key == XK_section) { //'Shift-3' //
						EQMOD("contrast",-8)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_dollar ) { //'Shift-4' //
						EQMOD("contrast",+8)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_1 ) { //'1' //
						EQMOD("brightness",-50)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_2 ) { //'2' //
						EQMOD("brightness",+50)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_3 ) { //'3' //
						EQMOD("contrast",-50)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_4 ) { //'4' //
						EQMOD("contrast",+50)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_5 ) { //'5' //
						EQMOD("gamma",-8)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_6 ) { //'6' //
						EQMOD("gamma",+8)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_7 ) { //'7' //
						EQMOD("saturation",-50)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_8 ) { //'8' //
						EQMOD("saturation",+100)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_9 ) { //'9' //
						EQMOD("hue",-5)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_0 ) { //'0' //
						EQMOD("hue",+5)
							else { remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key); }
					} else if (key == XK_E ) { //'E' //
						if (getvidmode() == VO_XV)
							xj_set_eq("contrast",-500); // xvinfo default:64 (0..255)
						else
							xj_set_eq("contrast",0);
						xj_set_eq("brightness",0);
						xj_set_eq("saturation",0);
						xj_set_eq("hue",0);
						xj_set_eq("gamma",0);
						force_redraw=1;
					} else if (key == XK_period ) { //'.' // resize 100%
						XCresize_percent(100);
					} else if (key == XK_comma ) { //',' // resize to aspect ratio
						XCresize_aspect(0);
					} else if (key == XK_less ) { //'<' // resize *.83
						XCresize_scale(-1);
					} else if (key == XK_greater ) { //'>' // resize *1.2
						XCresize_scale(1);
					} else if (key == XK_backslash ) { // '\' // A/V offset
						XCtimeoffset(0, (unsigned int) key);
					} else if (key == XK_plus ) { //'+' // A/V offset
						XCtimeoffset(1, (unsigned int) key);
					} else if (key == XK_minus ) { //'-'  A/V offset
						XCtimeoffset(-1, (unsigned int) key);
					} else if (key == XK_braceright ) { //'}' // A/V offset
						XCtimeoffset(2, (unsigned int) key);
					} else if (key == XK_braceleft) { //'{'  A/V offset
						XCtimeoffset(-2, (unsigned int) key);
					} else if (key == XK_m ) { // 'm' // toggle mouse pointer
						xj_mousepointer(2);
#ifdef CROPIMG
					} else if (key == XK_bracketleft ) { // '[' //
						xoffset-=2;
						if (xoffset<0) xoffset=0;
						force_redraw=1;
					} else if (key == XK_bracketright ) { // ']' //
						xoffset+=2;
						if (xoffset>movie_width) xoffset=movie_width;
						force_redraw=1;
#endif
					} else if (key == XK_BackSpace ) { // BACKSPACE
						if ((interaction_override&OVR_JCONTROL) == 0)
							jackt_rewind();
							remote_notify(NTY_KEYBOARD,
									310, "keypress=%d # backspace", (unsigned int) key);
					} else if (key == XK_space ) { // ' ' // SPACE
						if ((interaction_override&OVR_JCONTROL) == 0)
							jackt_toggle();
							remote_notify(NTY_KEYBOARD,
									310, "keypress=%d # space", (unsigned int) key);
#if 0 // TEST - save current config -- JACK-SESSION
					} else if (key == XK_z ) { // 'z'
						saveconfig("/tmp/xj.cfg");
#endif
					} else {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
						if (want_debug)
							printf("unassigned key pressed: 0x%x\n", (unsigned int)key);
					}
				}
				break;
			case ReparentNotify:
				break;
			default:
				//printf("unhandled X event: type: %ld = 0x%x\n", (long) event.type, (int) event.type);
				break;
		}
	}
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

static int              xv_swidth;
static int              xv_sheight;
static XvPortID         xv_port;
static XShmSegmentInfo  xv_shminfo;
static XvImage         *xv_image;

static char   *xv_buffer;
static size_t  xv_len;
static int     xv_one_memcpy = 0;
static int     xv_pic_format = FOURCC_I420;

static void allocate_xvimage (void) {
	// YV12 has 12 bits per pixel. 8bitY 2+2 UV
	xv_len = movie_width * movie_height * 3 / 2 ;

	/* shared memory allocation etc.  */
	xv_image = XvShmCreateImage(xj_dpy, xv_port,
		xv_pic_format, NULL, // FIXME: use xjadeo buffer directly
		xv_swidth, xv_sheight, //768, 486, //720, 576,
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

static void deallocate_xvimage(void) {
	XShmDetach(xj_dpy, &xv_shminfo);
	shmdt(xv_shminfo.shmaddr);
	XFree(xv_image);
	XSync(xj_dpy, False);
	xv_buffer=NULL;
}

static inline void xv_draw_colorkey(void) {
	XSetForeground( xj_dpy, xj_gc, 0 );
	if (xj_box[1] > 0 ) {
		XFillRectangle( xj_dpy, xj_win, xj_gc, 0, 0, xj_box[2], xj_box[1]);
		XFillRectangle( xj_dpy, xj_win, xj_gc, 0, xj_box[1]+xj_box[3], xj_box[2], xj_box[1]+xj_box[3]+xj_box[1]);
	} /* else */
	if (xj_box[0] > 0 ) {
		XFillRectangle( xj_dpy, xj_win, xj_gc, 0, 0, xj_box[0], xj_box[3]);
		XFillRectangle( xj_dpy, xj_win, xj_gc, xj_box[0]+xj_box[2], 0, xj_box[0]+xj_box[2]+xj_box[0], xj_box[3]);
	}
}

void render_xv (uint8_t *mybuffer) {

	if (!xv_buffer || !mybuffer) return;
	xv_draw_colorkey(); // TODO: only redraw on resize ?

	size_t Ylen  = movie_width * movie_height;
	size_t UVlen = movie_width * movie_height/4;
	size_t mw2 = movie_width /2;
	size_t mh2 = movie_height /2;

#ifdef CROPIMG
	Ylen*=2;
	UVlen*=2;
	mw2*=2;
	size_t stride = xv_swidth*2; // XXX
#else
	size_t stride = xv_swidth;
#endif
	// decode ffmpeg - YUV
	uint8_t *Yptr=mybuffer; // Y
	uint8_t *Uptr=Yptr + Ylen; // U
	uint8_t *Vptr=Uptr + UVlen; // V

#ifdef CROPIMG
	Yptr+= xoffset;
	Uptr+= xoffset/2;
	Vptr+= xoffset/2;
#endif
	if (xv_pic_format == FOURCC_I420 && xv_one_memcpy) {
		// copy YUV420P
		memcpy(xv_buffer,mybuffer,Ylen+UVlen+UVlen); // Y+U+V
	} else if (xv_pic_format == FOURCC_I420) {
	
	// encode YV420P
		stride_memcpy(xv_buffer+xv_image->offsets[0],
			Yptr, xv_swidth, xv_sheight, xv_image->pitches[0], stride);

		stride_memcpy(xv_buffer+xv_image->offsets[1],
			Uptr, mw2, mh2, xv_image->pitches[1], mw2);

		stride_memcpy(xv_buffer+xv_image->offsets[2],
			Vptr, mw2, mh2, xv_image->pitches[2], mw2);
	} else {
	// encode YV12
		stride_memcpy(xv_buffer+xv_image->offsets[0],
			Yptr, xv_swidth, xv_sheight, xv_image->pitches[0], stride);

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
	unsigned int my_Width,my_Height;

	deallocate_xvimage();

	xv_swidth = movie_width;
	xv_sheight = movie_height;

	xj_dwidth = ffctv_width;
	xj_dheight = ffctv_height;

	xj_letterbox();
	allocate_xvimage();

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

#ifdef COLOREQ
static int xv_set_eq(char *name, int value) {
	XvAttribute *attributes;
	int i, howmany, xv_atom;

	/* get available attributes */
	attributes = XvQueryPortAttributes(xj_dpy, xv_port, &howmany);
	for (i = 0; i < howmany && attributes; i++)
		if (attributes[i].flags & XvSettable) {
			xv_atom = XInternAtom(xj_dpy, attributes[i].name, True);
			if (xv_atom != None) {
				int hue = 0, port_value, port_min, port_max;

				if (!strcmp(attributes[i].name, "XV_BRIGHTNESS")
						&& (!strcasecmp(name, "brightness")))
					port_value = value;
				else if (!strcmp(attributes[i].name, "XV_CONTRAST")
						&& (!strcasecmp(name, "contrast")))
					port_value = value;
				else if (!strcmp(attributes[i].name, "XV_SATURATION")
						&& (!strcasecmp(name, "saturation")))
					port_value = value;
				else if (!strcmp(attributes[i].name, "XV_HUE")
						&& (!strcasecmp(name, "hue")))
				{
					port_value = value;
					hue = 1;
				} else
				/* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
				if (!strcmp(attributes[i].name, "XV_RED_INTENSITY")
						&& (!strcasecmp(name, "red_intensity")))
					port_value = value;
				else if (!strcmp(attributes[i].name, "XV_GREEN_INTENSITY")
						&& (!strcasecmp(name, "green_intensity")))
					port_value = value;
				else if (!strcmp(attributes[i].name, "XV_BLUE_INTENSITY")
						&& (!strcasecmp(name, "blue_intensity")))
					port_value = value;
				else
					continue;

				port_min = attributes[i].min_value;
				port_max = attributes[i].max_value;

				/* nvidia hue workaround */
				if (hue && port_min == 0 && port_max == 360)
					port_value = (port_value >= 0) ? (port_value - 1000) : (port_value + 1000);

				/* -1000 <= val <= 1000 */
				port_value = (int) rint((port_value + 1000.0) * (port_max - port_min) / 2000.0) + port_min;
//	printf("SET port value:%i\n",port_value);
				XvSetPortAttribute(xj_dpy, xv_port, xv_atom, port_value);
				return (0); // OK
			}
		}
	return (1); // not found.
}

static int xv_get_eq(char *name, int *value) {
	XvAttribute *attributes;
	int i, howmany, xv_atom;

	/* get available attributes */
	attributes = XvQueryPortAttributes(xj_dpy, xv_port, &howmany);
	for (i = 0; i < howmany && attributes; i++)
		if (attributes[i].flags & XvGettable) {
			xv_atom = XInternAtom(xj_dpy, attributes[i].name, True);
			if (xv_atom != None) {
				int val, port_value = 0, port_min, port_max;

				XvGetPortAttribute(xj_dpy, xv_port, xv_atom, &port_value);

				port_min = attributes[i].min_value;
				port_max = attributes[i].max_value;

				/* -1000 <= val <= 1000 */
				val = (port_value - port_min)*2000.0/(port_max - port_min) - 1000;
				//printf("DEBUG: got port att:%s value:%i\n", attributes[i].name, port_value);

				if (!strcmp(attributes[i].name, "XV_BRIGHTNESS")
						&& (!strcasecmp(name, "brightness")))
					*value = val;
				else if (!strcmp(attributes[i].name, "XV_CONTRAST")
						&& (!strcasecmp(name, "contrast")))
					*value = val;
				else if (!strcmp(attributes[i].name, "XV_SATURATION")
						&& (!strcasecmp(name, "saturation")))
					*value = val;
				else if (!strcmp(attributes[i].name, "XV_HUE")
						&& (!strcasecmp(name, "hue")))
				{
					/* nasty nvidia detect */
					if (port_min == 0 && port_max == 360)
						*value = (val >= 0) ? (val - 100) : (val + 100);
					else
						*value = val;
				}
				else if (!strcmp(attributes[i].name, "XV_RED_INTENSITY")
						&& (!strcasecmp(name, "red_intensity")))
					*value = val;
				else if (!strcmp(attributes[i].name, "XV_GREEN_INTENSITY")
						&& (!strcasecmp(name, "green_intensity")))
					*value = val;
				else if (!strcmp(attributes[i].name, "XV_BLUE_INTENSITY")
						&& (!strcasecmp(name, "blue_intensity")))
					*value = val;
				else
					continue;

				return (0); // all right
			}
		}
	return (1); //error.
}
#endif

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
		xv_port = ad_info[i].base_id;
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
				fprintf(stderr,"XV: using YUV420P + Xvideo extension (I420)\n");
		} else if (xv_have_YV12) {
			xv_pic_format = FOURCC_YV12;
			if (!want_quiet)
				fprintf(stderr,"XV: using YUV420P + Xvideo extension (YV12)\n");
		} else {
			fprintf(stderr,
				"Xv: %s: could not find a suitable colormodel in ( ", ad_info[i].name);
			for (k = 0; k < fmt_cnt; ++k) {
				fprintf (stderr, "%#08x[%s] ", fmt_info[k].id, fmt_info[k].guid);
			}
			fprintf(stderr, ")\n");
			XFree(fmt_info);
			continue;
		}

		XFree(fmt_info);

		for(xv_port = ad_info[i].base_id, k = 0; k < ad_info[i].num_ports; ++k, ++(xv_port)) {
			if(!XvGrabPort(xj_dpy, xv_port, CurrentTime)) {
				if (want_verbose) fprintf(stdout, "Xv: grabbed port %ld\n", xv_port);
				got_port = True;
				break;
			}
		}
		// TODO allow to ovverride (e.g. 2nd port)
		if(got_port) break;
	} /* for */

	XvFreeAdaptorInfo(ad_info);

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
	xv_swidth = movie_width;
	xv_sheight = movie_height;

	xj_dwidth = ffctv_width;
	xj_dheight = ffctv_height;

	xj_letterbox();

	xj_win = XCreateSimpleWindow(xj_dpy, xj_rwin,
			-1, -1,
			xj_dwidth, xj_dheight,
			0,
			XWhitePixel(xj_dpy, xj_screen),
			XBlackPixel(xj_dpy, xj_screen));

	xj_set_hints();

	ev_mask =  KeyPressMask | ButtonPressMask | ButtonReleaseMask | Button1MotionMask |
			ExposureMask | StructureNotifyMask; //| PropertyChangeMask | PointerMotionMask;
	XSelectInput(xj_dpy, xj_win, ev_mask);

#ifdef DND
	init_dnd(xj_dpy, xj_win);
#endif
	XMapRaised(xj_dpy, xj_win);

	if ((xj_del_atom = XInternAtom(xj_dpy, "WM_DELETE_WINDOW", True)) != None)
	XSetWMProtocols(xj_dpy, xj_win, &xj_del_atom, 1);

	xj_gc = XCreateGC(xj_dpy, xj_win, 0, &values);

	allocate_xvimage();

	if (start_ontop) xj_ontop=1;
	if (start_fullscreen) xj_fullscreen=1;
	xj_set_fullscreen(xj_fullscreen);
	xj_set_ontop(xj_ontop);

#if 0
	// XV_BRIGHTNESS  XV_CONTRAST, XV_SATURATION , XV_COLORKEY
	// broken with some gfx boards.

	/* 004:<:003e: 20: Request(16): InternAtom only-if-exists=true(0x01)  name='XV_COLORKEY'
	 * 004:>:0x003e:32: Reply to InternAtom: atom=0x59("XV_COLORKEY")
	 * 004:<:003f: 12: XVideo-Request(141,14): unknown
	 * 004:>:003f:32: unexpected reply
	 * 004:<:0040: 52: XVideo-Request(141,19): unknown
	 * 004:<:0041: 52: XVideo-Request(141,19): unknown
	 */
	int value=0;
	Atom xj_a_colorkey = XInternAtom (xj_dpy, "XV_COLORKEY", True);
	if (xj_a_colorkey!=None && Success == XvGetPortAttribute(xj_dpy, xv_port, xj_a_colorkey, &value)) {
		XvSetPortAttribute(xj_dpy, xv_port, xj_a_colorkey, 0x00000000); // AARRGGBB
	}
#endif
	return 0;
}

void close_window_xv(void) {
#ifdef XDLG
	close_x_dialog(xj_dpy);
#endif
#ifdef XFIB
	x_fib_close (xj_dpy);
#endif
	XvStopVideo(xj_dpy, xv_port, xj_win);
	if(xv_shminfo.shmaddr) {
		XShmDetach(xj_dpy, &xv_shminfo);
		shmdt(xv_shminfo.shmaddr);
	}
	if(xv_image) XFree(xv_image);
	XSync(xj_dpy, False);
	XFreeGC(xj_dpy, xj_gc);
#if 1
	if (!loop_flag)
		XSetCloseDownMode(xj_dpy, DestroyAll);
#endif
	XDestroyWindow(xj_dpy, xj_win);
	XCloseDisplay(xj_dpy);
}

#if 1 // LEGACY wrapper functions

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
 *
 * X11 / ImLib2
 */

#if HAVE_IMLIB2

static int         im_depth;
static Visual     *im_vis;
static Colormap    im_cm;
static Imlib_Image im_image = NULL;

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

	xj_dwidth = ffctv_width;
	xj_dheight = ffctv_height;

	xj_letterbox();

	xj_win = XCreateSimpleWindow(
		xj_dpy, xj_rwin,
		0,             // x
		0,             // y
		xj_dwidth,     // width
		xj_dheight,    // height
		0,             // border
		BlackPixel(xj_dpy, xj_screen),
		WhitePixel(xj_dpy, xj_screen)
	);

	xj_set_hints();

	ev_mask =  KeyPressMask | ButtonPressMask | ButtonReleaseMask | Button1MotionMask | ExposureMask | StructureNotifyMask;
	XSelectInput(xj_dpy, xj_win, ev_mask);

#ifdef DND
	init_dnd(xj_dpy, xj_win);
#endif
	XMapRaised(xj_dpy, xj_win);

	imlib_set_cache_size(4096 * 1024); // check
	imlib_context_set_dither(0);

	/* express interest in WM killing this app */
	if ((xj_del_atom = XInternAtom(xj_dpy, "WM_DELETE_WINDOW", True)) != None)
		XSetWMProtocols(xj_dpy, xj_win, &xj_del_atom, 1);

	xj_gc = XCreateGC(xj_dpy, xj_win, 0, &values);

	imlib_context_set_display(xj_dpy);
	imlib_context_set_visual(im_vis);
	imlib_context_set_colormap(im_cm);
	imlib_context_set_drawable(xj_win);

	if (start_ontop) xj_ontop=1;
	if (start_fullscreen) xj_fullscreen=1;
	xj_set_fullscreen(xj_fullscreen);
	xj_set_ontop(xj_ontop);

	return 0;
}

void close_window_imlib2(void)
{
	if (im_image) {
		imlib_context_set_image(im_image);
		imlib_free_image();
		im_image = NULL;
	}
#if 1 //
	if (loop_flag)
		XSetCloseDownMode(xj_dpy, RetainPermanent);
	else
		XSetCloseDownMode(xj_dpy, DestroyAll);
#endif
	XDestroyWindow(xj_dpy, xj_win);
	XFreeGC(xj_dpy, xj_gc);
	XSync(xj_dpy, False);
#if 0
	XCloseDisplay(xj_dpy);
#endif
}

#ifdef COLOREQ
static int im_gamma = 0;
static int im_brightness = 0;
static int im_contrast = 0;
static int im_colormod = 0;

static int im2_set_eq(char *name, int value) {
	if (!strcasecmp(name, "brightness")) im_brightness = value;
	else if (!strcasecmp(name, "contrast")) im_contrast = value;
	else if (!strcasecmp(name, "gamma")) im_gamma = value;
	else return -1;
	im_colormod = 1; // TODO: set to 0 if all values 'normal'
	return 0; // ok
}

static int im2_get_eq(char *name, int *value) {
	if  (!value) return 1;
	if (!strcasecmp(name, "brightness")) *value = im_brightness;
	else if (!strcasecmp(name, "contrast")) *value = im_contrast;
	else if (!strcasecmp(name, "gamma")) *value = im_gamma;
	else return -1;
	return 0; // ok
}

#ifdef IM_CUSTOM_COLORTABLE

#define N_CLAMP(VAR) if((VAR)<0){(VAR)=0;} if ((VAR)>1){(VAR) = 1;}

static void im_set_equalizer(void) {
	DATA8 red_table[256], green_table[256], blue_table[256], alpha_table[256];
	float gamma, brightness, contrast;
	float rf, gf, bf;
	int k;

	brightness = im_brightness / 1000.0;
	contrast = tan(0.00095 * (im_contrast + 1000) * M_PI / 4);
	gamma = pow(2, im_gamma / -500.0);

	rf = 1.0/255.0; gf = 1.0/255.0; bf = 1.0/255.0;

	for (k = 0; k < 256; k++) {
		float s;

		s = pow(rf * k, gamma);
		s = (s - 0.5) * contrast + 0.5;
		s += brightness;
		N_CLAMP(s)
		red_table[k] = (DATA8) (s * 255);

		s = pow(gf * k, gamma);
		s = (s - 0.5) * contrast + 0.5;
		s += brightness;
		N_CLAMP(s)
		green_table[k] = (DATA8) (s * 255);

		s = pow(bf * k, gamma);
		s = (s - 0.5) * contrast + 0.5;
		s += brightness;
		N_CLAMP(s)
		blue_table[k] = (DATA8) (s * 255);

		alpha_table[k] = 255;
	}

	imlib_set_color_modifier_tables(red_table, green_table, blue_table, alpha_table);
}
#endif

#endif

void render_imlib2 (uint8_t *mybuffer) {
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
#ifdef COLOREQ
	if (im_colormod) {
		Imlib_Color_Modifier imcm;
		imcm = imlib_create_color_modifier();
		imlib_context_set_color_modifier(imcm);
	#ifdef IM_CUSTOM_COLORTABLE
		im_set_equalizer();
	#else
		imlib_modify_color_modifier_brightness(im_brightness/1000.0); // -1.0 .. 1.0
		imlib_modify_color_modifier_contrast(im_contrast/1000.0+1.0); // 0.0 .. 2.0
		imlib_modify_color_modifier_gamma(im_gamma/1000.0+1.0); // 0.0 .. 2.0
	#endif
		imlib_apply_color_modifier();
		imlib_free_color_modifier();
	}
#endif
	if (im_image) {
		imlib_context_set_image(im_image);
		if (xj_box[2] == movie_width && xj_box[3]== movie_height && xj_box[0] == 0 && xj_box[1]== 0)  {
			imlib_render_image_on_drawable(xj_box[0], xj_box[1]);
		} else {
			#if 1 // draw black letter box bars
			Imlib_Image im_letterbox = NULL;
			int bw,bh,ox,oy;
			if (xj_box[0]<xj_box[1]) {bw=xj_box[2]; bh=xj_box[1]; ox=0; oy=xj_box[1]+xj_box[3];}
			else {bw=xj_box[0]; bh=xj_box[3]; oy=0; ox=xj_box[0]+xj_box[2];}
		//	printf("DEBUG %i %i %i %i [%ix%i+%i+%i]\n",xj_box[0],xj_box[1],xj_box[2],xj_box[3],bw,bh,ox,oy);
			if (bw>0 && bh > 0) {
				im_letterbox=imlib_create_image(bw, bh);
				imlib_context_set_image(im_letterbox);
				imlib_context_set_color(0, 0, 0, 255);
				imlib_image_fill_rectangle(0, 0, bw, bh);
				imlib_render_image_on_drawable(0,0);
				imlib_render_image_on_drawable(ox,oy);
				imlib_free_image();
			}
			#endif

			imlib_context_set_image(im_image);
			im_scaled=imlib_create_cropped_scaled_image(0,0,movie_width, movie_height,xj_box[2],xj_box[3]);
			imlib_context_set_image(im_scaled);
			//imlib_image_set_has_alpha(0); // beware.
			imlib_render_image_on_drawable(xj_box[0], xj_box[1]);
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
	unsigned int my_Width,my_Height;
#ifdef IMC
	if (im_image) {
		imlib_context_set_image(im_image);
		imlib_free_image();
		im_image = NULL;
	}
#endif

	xj_get_window_size(&my_Width,&my_Height);
	xj_dwidth = ffctv_width;
	xj_dheight = ffctv_height;
	xj_letterbox();
	xj_resize( my_Width, my_Height);
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
