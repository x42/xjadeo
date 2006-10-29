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

/*******************************************************************************
 * XV !!!
 */

#if HAVE_LIBXV

  Display      		*xv_dpy;
  Screen       		*xv_scn;
  Window		xv_rwin, xv_win;
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
//	int        old_pic_format;
	XEvent event;
	while(XPending(xv_dpy)) {
		XNextEvent(xv_dpy, &event);
		switch (event.type) {
			case Expose:
				render_xv(buffer);
//				fprintf(stdout, "event expose\n");
				break;
			case ClientMessage:
//		         fprintf(stdout, "event client\n");
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
  size_t 	ad_cnt;
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
      case ClientMessage:
//         fprintf(stdout, "event client\n");
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
