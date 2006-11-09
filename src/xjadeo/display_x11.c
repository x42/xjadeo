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

#define DND

/*******************************************************************************
 * XV !!!
 */
#if (HAVE_LIBXV || HAVE_IMLIB || HAVE_IMLIB2)
  Display      		*xv_dpy;
  Window		xv_rwin, xv_win;
# ifdef DND // re-used by imlibs 
  Atom 			xv_a_XdndDrop;   
  Atom 			xv_a_XdndFinished;   
  Atom 			xv_a_XdndActionCopy;   
  Atom 			xv_a_XdndLeave;   
  Atom 			xv_a_XdndPosition;   
  Atom 			xv_a_XdndStatus;   
  Atom 			xv_a_XdndEnter;   
  Atom 			xv_a_XdndAware;   
  Atom 			xv_a_XdndTypeList;   
  Atom 			xv_a_XdndSelection;   
  Atom			xv_atom;
  int 			dnd_source;
  const int 		xdnd_version = 5;

void HandleEnter(XEvent * xe) {
	long *l = xe->xclient.data.l;
    	xv_atom= None;
	Atom ok = XInternAtom(xv_dpy, "text/uri-list", False);

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
      		while(a && xv_atom== None){
			XGetWindowProperty(xv_dpy, dnd_source, xv_a_XdndTypeList, offset,
			256, False, XA_ATOM, &type, &f,&n,&a,&data);
				if(data == NULL || type != XA_ATOM || f != 8*sizeof(Atom)){
				XFree(data);
				return;
			}
			for (ll=0; ll<n; ll++) {
				//if (data[ll]!=None)
				//	printf("DEBUG atom:%s\n", XGetAtomName(xv_dpy,data[ll]));
				if (data[ll] == ok) {
					xv_atom= ok;
					break;
				}
			}
			if (data) XFree(data);
		}
	} else {
		int i;
		for(i=2; i < 5; i++) {
			//if(l[i]!=None)
			//	printf("DEBUG atom:%s\n", XGetAtomName(xv_dpy,l[i]));
			if (l[i] == ok) xv_atom= ok;
		}
	}
	//printf("!!!!!!!!!!!!!!!! DND ok: %i\n",xv_atom==ok);
}

void SendStatus (void) {
	unsigned int my_Width,my_Height;
#ifdef HAVE_XV	
	get_window_size_xv(&my_Width,&my_Height);
#else 
	my_Width=0; my_Height=0; // FIXME.
#endif
	XClientMessageEvent response;
	response.type = ClientMessage;
	response.window = xv_win;
	response.format = 32;
	response.message_type = xv_a_XdndStatus;
	response.data.l[0] = xv_win;
	response.data.l[1] = (1)?1:0; // flags 3 ?? - TODO: check if we can accept this type.
	response.data.l[2] = 0; // x, y
	response.data.l[3] = my_Width<<16 || my_Height&0xFFFFL; // w, h (width<<16 || height&0xFFFFL)
	response.data.l[4] = xv_a_XdndActionCopy; // action

	XSendEvent(xv_dpy, dnd_source, False, NoEventMask, (XEvent*)&response);
}

void SendFinished (void) {
	XClientMessageEvent finished;
	finished.type = ClientMessage;
	finished.display = xv_dpy;
	finished.window = dnd_source; 
	finished.format = 32;
	finished.message_type = xv_a_XdndFinished;
	finished.data.l[0] = xv_win;
	finished.data.l[1] = (1)?1:0; // flags - isAccepted ? sure.
	finished.data.l[2] = 0; // action atom
	finished.data.l[3] = 0; 
	finished.data.l[4] = 0;
	XSendEvent(xv_dpy, dnd_source, False, NoEventMask, (XEvent*)&finished);
}

#define MAX_DND_FILES 64
void xapi_open(void *d); // command to open movie - TODO: better do movie_open, initibuffer... (remote replies)

void getDragData (XEvent *xe) {
	Atom type;
	int f;
	unsigned long n, a;
	unsigned char *data;

	XGetWindowProperty(xv_dpy, xe->xselection.requestor,
            xe->xselection.property, 0, 65536, 
	    True, xv_atom, &type, &f, &n, &a, &data);

	SendFinished();

	if (!data){
		fprintf(stderr, "WARNING: drag-n-drop - no data\n"); 
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

	for (f=0;f<num;f++) {
		printf("drag-n-drop: recv: %i '%s'\n",f,files[f]);
	}
	if (num>0) xapi_open(files[0]);
	free(data);
}

# endif /* DND */
#endif /* HAVE any of xv, imlib* */

#if HAVE_LIBXV
//Display      		*xv_dpy;
  Screen       		*xv_scn;
//Window		xv_rwin, xv_win;
  int          		xv_dwidth, xv_dheight, xv_swidth, xv_sheight, xv_pic_format;

  GC           		xv_gc;
  XEvent       		xv_event;
  XvPortID    	 	xv_port;
  XShmSegmentInfo  	xv_shminfo;
  XvImage      		*xv_image;

  Atom 			xv_del_atom;   
  char	 		*xv_buffer;
  size_t		xv_len;
  int			xv_one_memcpy = 0; 
  int			xv_ontop = 0; 
  int			xv_mouse = 0; 


// TODO: support other YUV Xv - ffmpeg combinations
// (depending on hardware and X) Xv can do more than YV12 ...
#define FOURCC_YV12 0x32315659  /* YV12   YUV420P */
#define FOURCC_I420 0x30323449  /* I420   Intel Indeo 4 */

//#define FOURCC_YUV2 0x32595559  /* YUV2   YUV422 */
//#define FOURCC_UYVY 0x59565955  /* YUV 4:2:2 */

int xv_pic_format = FOURCC_I420; // the format used for allocation.




/* blatantly ripped off mplayer's libvo/x11_common.c - THX. */

static int x11_get_property(Atom type, Atom ** args, unsigned long *nitems)
{
	int format;
	unsigned long bytesafter;
	return (Success == XGetWindowProperty(xv_dpy, xv_rwin, type, 0, 16384, False, AnyPropertyType, &type, &format, nitems, &bytesafter, (unsigned char **) args) && *nitems > 0);
}

#define NET_WM_STATE_TEST(ARGX,ARGY) { Atom type= XInternAtom(xv_dpy, ARGX,0); if (atom == type) { if (!want_quiet) fprintf(stderr,"[x11] Detected wm supports " #ARGX " state.\n" ); return ARGY; } }
static int net_wm_support_state_test(Atom atom)
{
	NET_WM_STATE_TEST("_NET_WM_STATE_FULLSCREEN",1);
	NET_WM_STATE_TEST("_NET_WM_STATE_ABOVE",2);
	NET_WM_STATE_TEST("_NET_WM_STATE_STAYS_ON_TOP",4);
	return 0;
}

void check(void) {
    Atom *args;
    int i;
    int wm=0;
    unsigned long nitems;
    if (x11_get_property(XInternAtom(xv_dpy, "_NET_SUPPORTED",0), &args, &nitems)) {
	if (!want_quiet) fprintf(stderr,"[x11] Detected wm supports NetWM.\n");
	for (i = 0; i < nitems; i++) wm |= net_wm_support_state_test(args[i]);
    }
    XFree(args);
}


static void net_wm_set_property(Window window, char *atom, int state)
{
	XEvent xev;
	int set = _NET_WM_STATE_ADD;
	Atom type, property;

	if (state == _NET_WM_STATE_TOGGLE) set = _NET_WM_STATE_TOGGLE;
	else if (!state) set = _NET_WM_STATE_REMOVE;

	type = XInternAtom(xv_dpy,"_NET_WM_STATE", 0);
	property = XInternAtom(xv_dpy,atom, 0);

	xev.type = ClientMessage;
	xev.xclient.type = ClientMessage;
	xev.xclient.window = window ; //xv_win;
	xev.xclient.message_type = type;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = set;
	xev.xclient.data.l[1] = property;
	xev.xclient.data.l[2] = 0;
	
        if (!XSendEvent(xv_dpy, DefaultRootWindow(xv_dpy), False,
		   SubstructureRedirectMask|SubstructureNotifyMask, &xev))
        {
            fprintf(stderr,"error (un)setting 'always on top' mode\n");
        }
}

void xv_fullscreen (int action) {
	net_wm_set_property(xv_win, "_NET_WM_STATE_FULLSCREEN", action);
}

/* also from mplayer's libvo/x11_common.c - thanks GPL !*/
void vo_hidecursor(Display * disp, Window win)
{
    Cursor no_ptr;
    Pixmap bm_no;
    XColor black, dummy;
    Colormap colormap;
    static char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    colormap = DefaultColormap(disp, DefaultScreen(disp));
    if ( !XAllocNamedColor(disp, colormap, "black", &black, &dummy) )
    {
      return; // color alloc failed, give up
    }
    bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8, 8);
    no_ptr = XCreatePixmapCursor(disp, bm_no, bm_no, &black, &black, 0, 0);
    XDefineCursor(disp, win, no_ptr);
    XFreeCursor(disp, no_ptr);
    if (bm_no != None)
        XFreePixmap(disp, bm_no);
    XFreeColors(disp,colormap,&black.pixel,1,0);
}

void vo_showcursor(Display * disp, Window win)
{
    XDefineCursor(disp, win, 0);
}

void xv_showcursor (void) {
	vo_showcursor(xv_dpy,xv_win);
}

void xv_hidecursor (void) {
	vo_hidecursor(xv_dpy,xv_win);
}


void allocate_xvimage (void) {
  // YV12 has 12 bits per pixel. 8bitY 2+2 UV
  xv_len = movie_width * movie_height * 3 / 2 ;

  /* shared memory allocation etc.  */
  xv_image = XvShmCreateImage(xv_dpy, xv_port,
			 xv_pic_format, NULL, // FIXME: use xjadeo buffer directly 
			 xv_dwidth, xv_dheight, //768, 486, //720, 576,
			 &xv_shminfo);

  if (xv_len != xv_image->data_size) xv_one_memcpy =0; else xv_one_memcpy=0;

  xv_len =  xv_image->data_size;

  xv_shminfo.shmid = shmget(IPC_PRIVATE, xv_len, IPC_CREAT | 0777);

  xv_image->data = xv_buffer = xv_shminfo.shmaddr = shmat(xv_shminfo.shmid, 0, 0);

  XShmAttach(xv_dpy, &xv_shminfo);
  XSync(xv_dpy, False);

  if (xv_shminfo.shmid > 0)
    shmctl (xv_shminfo.shmid, IPC_RMID, 0);
}

void deallocate_xvimage(void) {
	XShmDetach(xv_dpy, &xv_shminfo);
	shmdt(xv_shminfo.shmaddr);
	XFree(xv_image);
	XSync(xv_dpy, False);
	xv_buffer=NULL;
}
void get_window_pos_xv (int *x,  int *y) {
	XWindowAttributes attr;
	XGetWindowAttributes(xv_dpy, xv_win, &attr);
	if (x) *x=attr.x;
	if (y) *y=attr.y;
}

void get_window_pos_xv_old (int *x,  int *y) {
	unsigned int dummy_u0, dummy_u1;
	unsigned int dummy_W, dummy_H;
	Window dummy_w;
	// FIXME: this returns the position of the video in the window
	// should return the pos of the window relative to the root.
	XGetGeometry(xv_dpy, xv_win, &dummy_w, x,y, &dummy_W, &dummy_H,&dummy_u0,&dummy_u1);
}

void get_window_size_xv (unsigned int *my_Width, unsigned int *my_Height) {
	int dummyX,dummyY;
	unsigned int dummy_u0, dummy_u1;
	Window dummy_w;
	XGetGeometry(xv_dpy, xv_win, &dummy_w, &dummyX,&dummyY,my_Width,my_Height,&dummy_u0,&dummy_u1);
}

void resize_xv (unsigned int x, unsigned int y) { 
	XResizeWindow(xv_dpy, xv_win, x, y);
}

void position_xv (int x, int y) { 
	XMoveWindow(xv_dpy, xv_win,x,y);
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

	XvShmPutImage(xv_dpy, xv_port,
		xv_win, xv_gc,
		xv_image,
		0, 0,				/* sx, sy */
		xv_swidth, xv_sheight,		/* sw, sh */
		0,  0,		/* dx, dy */
		xv_dwidth, xv_dheight,		/* dw, dh */
		True);
	XFlush(xv_dpy);
}

void handle_X_events_xv (void) {
	XEvent event;
	while(XPending(xv_dpy)) {
		XNextEvent(xv_dpy, &event);
		switch (event.type) {
			case Expose:
				render_xv(buffer);
//				fprintf(stdout, "event expose\n");
				break;
			case SelectionNotify:
#ifdef DND
				getDragData(&event);
#endif
				break;
			case ClientMessage:
#ifdef DND
		         	//fprintf(stdout, "event client: %i\n",event.xclient.message_type);
				if (event.xclient.message_type == xv_a_XdndPosition) {
					if (xv_atom!= None) 
						SendStatus();
			//	} else if (event.xclient.message_type == xv_a_XdndLeave)
			//		printf("DND LEAVE!\n");
				} else if (event.xclient.message_type == xv_a_XdndEnter) {
					HandleEnter(&event);
					SendStatus();
				} else if (event.xclient.message_type == xv_a_XdndDrop) {
					//printf("DROP!\n");
    					if (xv_atom!= None) {
						XConvertSelection(xv_dpy, xv_a_XdndSelection, xv_atom, xv_a_XdndSelection, xv_win, CurrentTime);
					}
					//XFlush(xv_dpy);
					//SendFinishedOther();
				}
#endif
				//if (event.xclient.data.l[0] == xv_a_TakeFocus)  {
				//	;
				//}

				if (event.xclient.data.l[0] == xv_del_atom) 
					loop_flag = 0;
				break;
			case ConfigureNotify:
				{
					unsigned int my_Width,my_Height;
					get_window_size_xv(&my_Width,&my_Height);
					xv_dwidth= my_Width;
					xv_dheight= my_Height;
				}
				render_xv(buffer);
				break;
			case ButtonRelease:
				if (event.xbutton.button == 1) {
					XResizeWindow(xv_dpy, xv_win, movie_width, movie_height);
				} else {
					unsigned int my_Width,my_Height;
					get_window_size_xv(&my_Width,&my_Height);


					if (event.xbutton.button == 4 && my_Height > 32 && my_Width > 32)  {
						float step=sqrt((float)my_Height);
						my_Width-=floor(step*((float)movie_width/(float)movie_height));
						my_Height-=step;
					//	XResizeWindow(xv_dpy, xv_win, my_Width, my_Height);
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

					XResizeWindow(xv_dpy, xv_win, my_Width, my_Height);
					lcs_int("window_size",my_Width<<16|my_Height);
				}
//				fprintf(stdout, "Button %i release event.\n", event.xbutton.button);
				render_xv(buffer);
				break;
			case KeyPress:
				{
					int key;
					KeySym keySym;
					char buf[100];
					static XComposeStatus stat;

					XLookupString(&event.xkey, buf, sizeof(buf), &keySym, &stat);
					key = ((keySym & 0xff00) != 0 ? ((keySym & 0x00ff) + 256) : (keySym));
					if (key == (0x1b + 256) ) loop_flag=0; // 'Esc'
					if (key == (0x71) ) loop_flag=0; // 'q'
					if (key == 0x61 ) net_wm_set_property(xv_win, "_NET_WM_STATE_ABOVE", (xv_ontop^=1)); //'a'
					if (key == 0x66 ) xv_fullscreen(_NET_WM_STATE_TOGGLE); //'f' // fullscreen
					if (key == 0x6d) { 	// 'm'
					    if (xv_mouse^=1) xv_hidecursor(); else xv_showcursor();
					}    
				//	printf("X11 key press: '%c' %x\n",key,key);
				//	xjadeo_putkey(key);
				}
				break;
			default:
				break;
		}
	}
}

void newsrc_xv (void) { 
	deallocate_xvimage();

  	xv_dwidth = xv_swidth = movie_width;
	xv_dheight = xv_sheight = movie_height;
	allocate_xvimage();
	render_xv(buffer);

	unsigned int my_Width,my_Height;
#if 1
	get_window_size_xv(&my_Width,&my_Height);
#else
	my_Width=movie_width;
	my_Height=movie_height;
#endif
	XResizeWindow(xv_dpy, xv_win, my_Width, my_Height);
}

int open_window_xv (int *argc, char ***argv) {
  char *w_name ="xjadeo";
  char *i_name ="xjadeo";
  unsigned int 	ad_cnt;
  int		scn_id,
                fmt_cnt,
                got_port, 
                i, k;
  int xv_have_YV12, xv_have_I420;

  XGCValues	values;
  XSizeHints	hints;
  XWMHints	wmhints;
  XTextProperty	x_wname, x_iname;

  XvAdaptorInfo	*ad_info;
  XvImageFormatValues	*fmt_info;

  if(!(xv_dpy = XOpenDisplay(NULL))) return 1;

  xv_rwin = DefaultRootWindow(xv_dpy);
  scn_id = DefaultScreen(xv_dpy);

  if (!XShmQueryExtension(xv_dpy)) return 1;

  /* So let's first check for an available adaptor and port */
  if(Success == XvQueryAdaptors(xv_dpy, xv_rwin, &ad_cnt, &ad_info)) {
  
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
      } /* if */
      fmt_info = XvListImageFormats(xv_dpy, ad_info[i].base_id,&fmt_cnt);
      if (!fmt_info || fmt_cnt == 0) {
	fprintf(stderr, "Xv: %s: NO supported formats\n", ad_info[i].name);
	continue;
      } /* if */
      for(xv_have_YV12=0, xv_have_I420=0, k=0; k < fmt_cnt; ++k) {
#if 0
        fprintf(stderr, "INFO: Xvideo port %d: 0x%#08x (%c%c%c%c) %s",
                (int)xv_port,
                fmt_info[k].id,
                (fmt_info[k].id)      & 0xff,
                (fmt_info[k].id >  8) & 0xff,
                (fmt_info[k].id > 16) & 0xff,
                (fmt_info[k].id > 24) & 0xff,
                (fmt_info[k].format == XvPacked) ? "packed" : "planar");

	fprintf (stderr, " [%s]\n", fmt_info[k].guid);
#endif
        if (FOURCC_YV12 == fmt_info[k].id) {
            xv_have_YV12 = 1; 
        }
        if (FOURCC_I420 == fmt_info[k].id) {
            xv_have_I420 = 1; 
        }
      } /* for */

      if (xv_have_I420) {
	    xv_pic_format = FOURCC_I420;
	    if (!want_quiet) 
            fprintf(stderr,"XV: using YUV420P + Xvideo extention (I420)\n");
      }
      else if (xv_have_YV12) { 
	    if (!want_quiet) 
            fprintf(stderr,"XV: using YUV420P + Xvideo extention (YV12)\n");
	    xv_pic_format = FOURCC_YV12;
      }
      else {
	fprintf(stderr,
		"Xv: %s: could not find a suitable colormodel in ( ",
		ad_info[i].name);
	for (k = 0; k < fmt_cnt; ++k) {
	  fprintf (stderr, "%#08x[%s] ", fmt_info[k].id, fmt_info[k].guid);
	}
	fprintf(stderr, ")\n");
	continue;
      } /* if */

      for(xv_port = ad_info[i].base_id, k = 0;
	  k < ad_info[i].num_ports;
	  ++k, ++(xv_port)) {
	if(!XvGrabPort(xv_dpy, xv_port, CurrentTime)) {
	  if (want_verbose) fprintf(stdout, "Xv: grabbed port %ld\n", xv_port);
	  got_port = True;
	  break;
	} /* if */
      } /* for */
      if(got_port)
	break;
    } /* for */

  } else {
    /* Xv extension probably not present */
    return 1;
  } /* else */

  if(!ad_cnt) {
    fprintf(stderr, "Xv: (ERROR) no adaptor found!\n");
    return 1;
  }
  if(!got_port) {
    fprintf(stderr, "Xv: (ERROR) could not grab any port!\n");
    return 1;
  }

  /* --------------------------------------------------------------------------
   * default settings which allow arbitraray resizing of the window
   */
  hints.flags = PSize | PMaxSize | PMinSize;
  hints.min_width = movie_width / 16;
  hints.min_height = movie_height / 16;

  /* --------------------------------------------------------------------------
   * maximum dimensions for Xv support are about 2048x2048
   */
  hints.max_width = 2048;
  hints.max_height = 2048;

  wmhints.input = True;
  wmhints.flags = InputHint;

  XStringListToTextProperty(&w_name, 1 ,&x_wname);
  XStringListToTextProperty(&i_name, 1 ,&x_iname);

  /*
   * default settings: source, destination and logical widht/height
   * are set to our well known dimensions.
   */
  xv_dwidth = xv_swidth = movie_width;
  xv_dheight = xv_sheight = movie_height;

    xv_win = XCreateSimpleWindow(xv_dpy, xv_rwin,
				       0, 0,
				       xv_dwidth, xv_dheight,
				       0,
				       XWhitePixel(xv_dpy, scn_id),
				       XBlackPixel(xv_dpy, scn_id));

//  XmbSetWMProperties(xv_dpy, xv_win, "xjadeo", NULL, NULL, 0, NULL, NULL, NULL);
  XSetWMProperties(xv_dpy, xv_win, 
		    &x_wname, &x_iname,
		  NULL, 0, &hints, &wmhints, NULL);

  XSelectInput(xv_dpy, xv_win, KeyPressMask | ButtonPressMask | ButtonReleaseMask | ExposureMask | StructureNotifyMask);

#ifdef DND
  Atom atm = (Atom)xdnd_version;
  if ((xv_a_XdndDrop = XInternAtom (xv_dpy, "XdndDrop", True)) != None && 
      (xv_a_XdndLeave = XInternAtom (xv_dpy, "XdndLeave", True)) != None && 
      (xv_a_XdndEnter = XInternAtom (xv_dpy, "XdndEnter", True)) != None && 
/*    (xv_uri_atom = XInternAtom (xv_dpy, "text/uri-list", True)) != None &&  */
      (xv_a_XdndActionCopy = XInternAtom (xv_dpy, "XdndActionCopy", True)) != None && 
      (xv_a_XdndFinished = XInternAtom (xv_dpy, "XdndFinished", True)) != None && 
      (xv_a_XdndPosition = XInternAtom (xv_dpy, "XdndPosition", True)) != None && 
      (xv_a_XdndStatus = XInternAtom (xv_dpy, "XdndStatus", True)) != None && 
      (xv_a_XdndTypeList = XInternAtom (xv_dpy, "XdndTypeList", True)) != None && 
      (xv_a_XdndSelection = XInternAtom (xv_dpy, "XdndSelection", True)) != None && 
      (xv_a_XdndAware = XInternAtom (xv_dpy, "XdndAware", True)) != None  ) {
  	printf("enabled drag-DROP support.\n");
	XChangeProperty(xv_dpy, xv_win, xv_a_XdndAware, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atm, 1);
     // XDeleteProperty(xv_dpy, xv_win, xv_a_XdndAware);
  }
#endif

  XMapRaised(xv_dpy, xv_win);

  if ((xv_del_atom = XInternAtom(xv_dpy, "WM_DELETE_WINDOW", True)) != None)
    XSetWMProtocols(xv_dpy, xv_win, &xv_del_atom, 1);

  xv_gc = XCreateGC(xv_dpy, xv_win, 0, &values);

  allocate_xvimage ();

  check();
  if (start_ontop)
//net_wm_set_property(xv_win, "_NET_WM_STATE_STAYS_ON_TOP", (xv_ontop^=1));
  net_wm_set_property(xv_win, "_NET_WM_STATE_ABOVE", (xv_ontop^=1));

  return 0;
}

void close_window_xv(void) {
		 
	//XvFreeAdaptorInfo(ai);
	XvStopVideo(xv_dpy, xv_port, xv_win);
	if(xv_shminfo.shmaddr) {
		XShmDetach(xv_dpy, &xv_shminfo);
		shmdt(xv_shminfo.shmaddr);
	}
	if(xv_image) XFree(xv_image);
	XSync(xv_dpy, False);
	XCloseDisplay(xv_dpy);
}

#endif /* HAVE_LIBXV */


/*******************************************************************************
 *
 * X11 / ImLib 
 */

#if HAVE_IMLIB

	Display   *display;
	ImlibData *imlib = NULL;
	Window    window;
	GC        gc;
	int       screen_number;
	int       depth;
	XImage    *image;
	Pixmap    pxm, pxmmask;
	Atom      del_atom;           /* WM_DELETE_WINDOW atom   */


void get_window_size_imlib (unsigned int *my_Width, unsigned int *my_Height) {
	int dummy0,dummy1;
	unsigned int dummy_u0, dummy_u1;
	Window dummy_w;
	XGetGeometry(display, window, &dummy_w, &dummy0,&dummy1,my_Width,my_Height,&dummy_u0,&dummy_u1);
}

void get_window_pos_imlib (int *x,  int *y) {
	unsigned int dummy_u0, dummy_u1;
	unsigned int dummy_W, dummy_H;
	Window dummy_w;
	// FIXME: this returns the position of the video in the window
	// should return the pos of the window relative to the root.
	XGetGeometry(display, window, &dummy_w, x,y, &dummy_W, &dummy_H,&dummy_u0,&dummy_u1);
}

int open_window_imlib (int *argc, char ***argv) {
  if ( (display=XOpenDisplay(NULL)) == NULL )
  {
      (void) fprintf( stderr, "Cannot connect to X server\n");
      exit( -1 );
  }
 
  // init only once!
  imlib = Imlib_init(display);
  
  screen_number = DefaultScreen(display);
  depth = DefaultDepth(display, screen_number);
  
  window = XCreateSimpleWindow(
    display, 
    RootWindow(display,screen_number),
    0,             // x
    0,             // y
    movie_width,   // width
    movie_height,  // height
    0,             // border
    BlackPixel(display, screen_number), 
    WhitePixel(display,screen_number)
  );

  XmbSetWMProperties(display, window, "xjadeo", NULL,
         NULL, 0, NULL, NULL,
         NULL);
     
  XGCValues values;
  unsigned long valuemask = 0;
  gc = XCreateGC(display, window, valuemask, &values);
  
  // defined in: /usr/include/X11/X.h  //  | VisibilityChangeMask);
  XSelectInput(display, window, KeyPressMask | ExposureMask | ButtonPressMask | ButtonReleaseMask |StructureNotifyMask ); 
  
  XMapWindow(display, window);
 
#ifdef DND 
  /* hack to re-use xv-dnd  for x11/imlib
   * gonna be resolved with upcoming x11/xv merge/cleanup
   */
  xv_win= window;
  xv_dpy= display;
  Atom atm = (Atom)xdnd_version;
  if ((xv_a_XdndDrop = XInternAtom (xv_dpy, "XdndDrop", True)) != None && 
      (xv_a_XdndLeave = XInternAtom (xv_dpy, "XdndLeave", True)) != None && 
      (xv_a_XdndEnter = XInternAtom (xv_dpy, "XdndEnter", True)) != None && 
/*    (xv_uri_atom = XInternAtom (xv_dpy, "text/uri-list", True)) != None &&  */
      (xv_a_XdndActionCopy = XInternAtom (xv_dpy, "XdndActionCopy", True)) != None && 
      (xv_a_XdndFinished = XInternAtom (xv_dpy, "XdndFinished", True)) != None && 
      (xv_a_XdndPosition = XInternAtom (xv_dpy, "XdndPosition", True)) != None && 
      (xv_a_XdndStatus = XInternAtom (xv_dpy, "XdndStatus", True)) != None && 
      (xv_a_XdndTypeList = XInternAtom (xv_dpy, "XdndTypeList", True)) != None && 
      (xv_a_XdndSelection = XInternAtom (xv_dpy, "XdndSelection", True)) != None && 
      (xv_a_XdndAware = XInternAtom (xv_dpy, "XdndAware", True)) != None  ) {
  	printf("enabled drag-DROP support.\n");
	XChangeProperty(xv_dpy, xv_win, xv_a_XdndAware, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atm, 1);
     // XDeleteProperty(xv_dpy, xv_win, xv_a_XdndAware);
  }
#endif
     
  /* express interest in WM killing this app */
  if ((del_atom = XInternAtom(display, "WM_DELETE_WINDOW", True)) != None)
    XSetWMProtocols(display, window, &del_atom, 1);
	return 0;
}

void close_window_imlib(void)
{
	XFreeGC(display, gc);
	XCloseDisplay(display);
	//imlib=NULL;
}

        
void render_imlib (uint8_t *mybuffer) {
	unsigned int my_Width,my_Height;
	ImlibImage *iimage;
	if (!mybuffer) return;
	iimage = Imlib_create_image_from_data( imlib, mybuffer, NULL, movie_width, movie_height);

    /* get the current window size */
      get_window_size_imlib(&my_Width,&my_Height);

    /* Render the original 24-bit Image data into a pixmap of size w * h */
      Imlib_render(imlib,iimage, my_Width,my_Height );
      //Imlib_render(imlib,iimage,movie_width,movie_height);


    /* Extract the Image and mask pixmaps from the Image */
      pxm=Imlib_move_image(imlib,iimage);
    /* The mask will be 0 if the image has no transparency */
      pxmmask=Imlib_move_mask(imlib,iimage);
    /* Put the Image pixmap in the background of the window */
      XSetWindowBackgroundPixmap(display,window,pxm);       
  //    XPutImage(display,window,gc,pxm, 0,0,0,0, my_Width, my_Height);
      XClearWindow(display,window);       
//       XSync(display, True);     
// No need to sync. XPending will take care in the event loop.
      Imlib_free_pixmap(imlib, pxm);
      Imlib_kill_image(imlib, iimage);
}

void newsrc_imlib (void) { 
	; // nothing to do :)
}


void handle_X_events_imlib (void) {
    XEvent event;
    while(XPending(display))
    {
      XNextEvent(display, &event);
      switch  (event.type) {
      case Expose:
	   render_imlib(buffer);
//         fprintf(stdout, "event expose\n");
        break;
      case SelectionNotify:
#ifdef DND
	   getDragData(&event);
#endif
	break;
      case ClientMessage:
#ifdef DND
	//fprintf(stdout, "event client: %i\n",event.xclient.message_type);
	if (event.xclient.message_type == xv_a_XdndPosition) {
		if (xv_atom!= None) 
			SendStatus();
	} else if (event.xclient.message_type == xv_a_XdndEnter) {
		HandleEnter(&event);
		SendStatus();
	} else if (event.xclient.message_type == xv_a_XdndDrop) {
		if (xv_atom!= None) {
			XConvertSelection(xv_dpy, xv_a_XdndSelection, xv_atom, xv_a_XdndSelection, xv_win, CurrentTime);
		}
	}
#endif
        if (event.xclient.data.l[0] == del_atom)
            loop_flag = 0;
        break;
      case ButtonPress:
//	fprintf(stdout, "Button %i press event.\n", event.xbutton.button);
	break;
      case ButtonRelease:
//	fprintf(stdout, "Button %i release event.\n", event.xbutton.button);
	if (event.xbutton.button == 1)
		// resize to movie size
		XResizeWindow(display, window, movie_width, movie_height);
	else {
		unsigned int my_Width,my_Height;
      		get_window_size_imlib(&my_Width,&my_Height);

		if (event.xbutton.button == 4 && my_Height > 32 && my_Width > 32)  {
			float step=sqrt((float)my_Height);
			my_Width-=floor(step*((float)movie_width/(float)movie_height));
			my_Height-=step;
			//XResizeWindow(display, window, my_Width, my_Height);
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

		XResizeWindow(display, window, my_Width, my_Height);
		lcs_int("window_size",my_Height);
	}
	break;
	case KeyPress:
	{
		int key;
		KeySym keySym;
		char buf[100];
		static XComposeStatus stat;

		XLookupString(&event.xkey, buf, sizeof(buf), &keySym, &stat);
		key = ((keySym & 0xff00) != 0 ? ((keySym & 0x00ff) + 256) : (keySym));
		if (key == (0x1b + 256) ) loop_flag=0;
	//	printf("X11 key press: '%c'\n",key);
	//	xjadeo_putkey(key);
	}
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
      default:
	; //fprintf(stdout, "unnoticed X event.\n");
      }
    }
}

void resize_imlib (unsigned int x, unsigned int y) { 
	XResizeWindow(display, window, x, y);
}

void position_imlib (int x, int y) { 
	XMoveWindow(display, window, x, y);
}

#endif /* HAVE_IMLIB */


/*******************************************************************************
 *
 * X11 / ImLib2 
 */

#if HAVE_IMLIB2

	Display   *im_display;
	Window    im_window;
	GC        im_gc;
	int       im_screen_number;
	int       im_depth;
	Visual    *im_vis;
	Colormap  im_cm;

//	ImlibData *imlib2 = NULL;
//	XImage    *im_image;
	Pixmap    im_pxm, im_pxmmask;
	Atom      im_del_atom;           /* WM_DELETE_WINDOW atom   */


void get_window_size_imlib2 (unsigned int *my_Width, unsigned int *my_Height) {
	int dummy0,dummy1;
	unsigned int dummy_u0, dummy_u1;
	Window dummy_w;
	XGetGeometry(im_display, im_window, &dummy_w, &dummy0,&dummy1,my_Width,my_Height,&dummy_u0,&dummy_u1);
}

void get_window_pos_imlib2 (int *x,  int *y) {
	unsigned int dummy_u0, dummy_u1;
	unsigned int dummy_W, dummy_H;
	Window dummy_w;
	// FIXME: this returns the position of the video in the window
	// should return the pos of the window relative to the root.
	XGetGeometry(im_display, im_window, &dummy_w, x,y, &dummy_W, &dummy_H,&dummy_u0,&dummy_u1);
}

int open_window_imlib2 (int *argc, char ***argv) {
  if ( (im_display=XOpenDisplay(NULL)) == NULL )
  {
      (void) fprintf( stderr, "Cannot connect to X server\n");
      exit( -1 );
  }
 
  // init only once!
  //imlib2 = Imlib_init(im_display);
  
  im_screen_number = DefaultScreen(im_display);
  im_depth = DefaultDepth(im_display, im_screen_number);
  im_vis = DefaultVisual(im_display, im_screen_number);
  im_cm = DefaultColormap(im_display, im_screen_number);

  im_window = XCreateSimpleWindow(
    im_display, 
    RootWindow(im_display,im_screen_number),
    0,             // x
    0,             // y
    movie_width,   // width
    movie_height,  // height
    0,             // border
    BlackPixel(im_display, im_screen_number), 
    WhitePixel(im_display, im_screen_number)
  );

  XmbSetWMProperties(im_display, im_window, "xjadeo", NULL,
         NULL, 0, NULL, NULL,
         NULL);
     
  XGCValues values;
  unsigned long valuemask = 0;
  im_gc = XCreateGC(im_display, im_window, valuemask, &values);
  
  // defined in: /usr/include/X11/X.h  //  | VisibilityChangeMask);
  XSelectInput(im_display, im_window, KeyPressMask | ExposureMask | ButtonPressMask | ButtonReleaseMask |StructureNotifyMask ); 
  
  XMapWindow(im_display, im_window);
       
#ifdef DND 
  /* hack to re-use xv-dnd  for x11/imlib2
   * gonna be resolved with upcoming x11/xv merge/cleanup
   */
  xv_win= im_window;
  xv_dpy= im_display;
  Atom atm = (Atom)xdnd_version;
  if ((xv_a_XdndDrop = XInternAtom (xv_dpy, "XdndDrop", True)) != None && 
      (xv_a_XdndLeave = XInternAtom (xv_dpy, "XdndLeave", True)) != None && 
      (xv_a_XdndEnter = XInternAtom (xv_dpy, "XdndEnter", True)) != None && 
/*    (xv_uri_atom = XInternAtom (xv_dpy, "text/uri-list", True)) != None &&  */
      (xv_a_XdndActionCopy = XInternAtom (xv_dpy, "XdndActionCopy", True)) != None && 
      (xv_a_XdndFinished = XInternAtom (xv_dpy, "XdndFinished", True)) != None && 
      (xv_a_XdndPosition = XInternAtom (xv_dpy, "XdndPosition", True)) != None && 
      (xv_a_XdndStatus = XInternAtom (xv_dpy, "XdndStatus", True)) != None && 
      (xv_a_XdndTypeList = XInternAtom (xv_dpy, "XdndTypeList", True)) != None && 
      (xv_a_XdndSelection = XInternAtom (xv_dpy, "XdndSelection", True)) != None && 
      (xv_a_XdndAware = XInternAtom (xv_dpy, "XdndAware", True)) != None  ) {
  	printf("enabled drag-DROP support.\n");
	XChangeProperty(xv_dpy, xv_win, xv_a_XdndAware, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atm, 1);
     // XDeleteProperty(xv_dpy, xv_win, xv_a_XdndAware);
  }
#endif
     
  imlib_context_set_display(im_display);
  imlib_context_set_visual(im_vis);
  imlib_context_set_colormap(im_cm);
  imlib_context_set_drawable(im_window);

  /* express interest in WM killing this app */
  if ((im_del_atom = XInternAtom(im_display, "WM_DELETE_WINDOW", True)) != None)
    XSetWMProtocols(im_display, im_window, &im_del_atom, 1);
	return 0;
}

void close_window_imlib2(void)
{
	XFreeGC(im_display, im_gc);
	XCloseDisplay(im_display);
	//imlib=NULL;
}
#define IMC
int realloc_imlib2=0;

void render_imlib2 (uint8_t *mybuffer) {
	unsigned int my_Width,my_Height;
	static Imlib_Image im_image = NULL;
	Imlib_Image im_scaled = NULL;
	if (!mybuffer) return;

#ifdef IMC
	DATA32 *data;
	if (realloc_imlib2 && im_image) {
		imlib_context_set_image(im_image);
		imlib_free_image();
		im_image = NULL;
	}
	if (!im_image) im_image = imlib_create_image(movie_width, movie_height);
	imlib_context_set_image(im_image);
	data=imlib_image_get_data();
#endif /*IMC*/
#ifdef IMLIB2RGBA
# ifndef IMC
	uint8_t * rgbabuf = mybuffer;
# else
	memcpy(data,mybuffer,4*sizeof(uint8_t)*movie_width*movie_height);
# endif /*IMC*/
#else
# ifndef IMC
	uint8_t * rgbabuf = malloc(4*sizeof(uint8_t)*movie_width*movie_height);
# endif
# if defined(__BIG_ENDIAN__)
#  ifndef IMC
	rgb2argb( rgbabuf, mybuffer, movie_width, movie_height);
#  else
	rgb2argb( (uint8_t*) data, mybuffer, movie_width, movie_height);
#  endif /*IMC*/
# else
#  ifndef IMC
	rgb2abgr( rgbabuf, mybuffer, movie_width, movie_height);
#  else
	rgb2abgr( (uint8_t*) data, mybuffer, movie_width, movie_height);
#  endif /*IMC*/
# endif
#endif

#ifdef IMC
	imlib_image_put_back_data(data);
#else
	//im_image = imlib_create_image_using_data(movie_width, movie_height, (DATA32*)rgbabuf);
	im_image = imlib_create_image_using_copied_data(movie_width, movie_height, (DATA32*)rgbabuf);
#endif

//	imlib_context_set_display(im_display);
//	imlib_context_set_visual(im_vis);
//	imlib_context_set_colormap(im_cm);
//	imlib_context_set_drawable(im_window);

    /* get the current window size */
	get_window_size_imlib2(&my_Width,&my_Height);
	if (im_image) {
		imlib_context_set_image(im_image);
		if (movie_width == my_Width && movie_height== my_Height)  {
			imlib_render_image_on_drawable(0, 0);
		} else {
			im_scaled=imlib_create_cropped_scaled_image(0,0,movie_width, movie_height,my_Width,my_Height);
			imlib_context_set_image(im_scaled);
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
	
#endif
}


void handle_X_events_imlib2 (void) {
    XEvent event;
    while(XPending(im_display))
    {
      XNextEvent(im_display, &event);
      switch  (event.type) {
      case Expose:
	   render_imlib2(buffer);
//         fprintf(stdout, "event expose\n");
        break;
      case SelectionNotify:
#ifdef DND
	   getDragData(&event);
#endif
	break;
      case ClientMessage:
#ifdef DND
	//fprintf(stdout, "event client: %i\n",event.xclient.message_type);
	if (event.xclient.message_type == xv_a_XdndPosition) {
		if (xv_atom!= None) 
			SendStatus();
	} else if (event.xclient.message_type == xv_a_XdndEnter) {
		HandleEnter(&event);
		SendStatus();
	} else if (event.xclient.message_type == xv_a_XdndDrop) {
		if (xv_atom!= None) {
			XConvertSelection(xv_dpy, xv_a_XdndSelection, xv_atom, xv_a_XdndSelection, xv_win, CurrentTime);
		}
	}
#endif
	//fprintf(stdout, "event client: %i\n",event.xclient.message_type);
        if (event.xclient.data.l[0] == im_del_atom)
            loop_flag = 0;
        break;
      case ButtonPress:
//	fprintf(stdout, "Button %i press event.\n", event.xbutton.button);
	break;
      case ButtonRelease:
//	fprintf(stdout, "Button %i release event.\n", event.xbutton.button);
	if (event.xbutton.button == 1)
		// resize to movie size
		XResizeWindow(im_display, im_window, movie_width, movie_height);
	else {
		unsigned int my_Width,my_Height;
      		get_window_size_imlib2(&my_Width,&my_Height);

		if (event.xbutton.button == 4 && my_Height > 32 && my_Width > 32)  {
			float step=sqrt((float)my_Height);
			my_Width-=floor(step*((float)movie_width/(float)movie_height));
			my_Height-=step;
			//XResizeWindow(im_display, im_window, my_Width, my_Height);
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

		XResizeWindow(im_display, im_window, my_Width, my_Height);
	}
	break;
	case KeyPress:
	{
		int key;
		KeySym keySym;
		char buf[100];
		static XComposeStatus stat;

		XLookupString(&event.xkey, buf, sizeof(buf), &keySym, &stat);
		key = ((keySym & 0xff00) != 0 ? ((keySym & 0x00ff) + 256) : (keySym));
		if (key == (0x1b + 256) ) loop_flag=0;
	//	printf("X11 key press: '%c'\n",key);
	//	xjadeo_putkey(key);
	}
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
      default:
	; //fprintf(stdout, "unnoticed X event.\n");
      }
    }
}

void resize_imlib2 (unsigned int x, unsigned int y) { 
	XResizeWindow(im_display, im_window, x, y);
}

void position_imlib2 (int x, int y) { 
	XMoveWindow(im_display, im_window, x, y);
}

#endif /* HAVE_IMLIB2 */
