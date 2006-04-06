#include "xjadeo.h"


  extern int movie_width, movie_height;
  extern int loop_flag, loop_run;
  extern uint8_t *buffer;

  extern int want_quiet;
  extern int want_verbose;
  extern int remote_en;

  extern char OSD_fontfile[1024];
  extern char OSD_text[128];
  extern char OSD_frame[48];
  extern int OSD_mode;
  extern int OSD_fx, OSD_fy;
  extern int OSD_tx, OSD_ty;



/*******************************************************************************
 * GTK
 */


#if HAVE_GTK
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

  GtkWidget *gwindow,*gimage;
  GdkGeometry *ggeometry;

void render_gtk (uint8_t *mybuffer);

void resize_gtk (unsigned int x, unsigned int y) { 
	gdk_window_resize(gwindow->window,x,y);
}

void getsize_gtk (unsigned int *x, unsigned int *y) {
//	gint d0,d1,d2;
	gint d3,d4;
	gdk_window_get_size(gwindow->window,&d3,&d4);
//	gdk_window_get_geometry (gwindow->window, &d0,&d1,&d3,&d4,&d2);
	if(x) *x=(unsigned int) d3;
	if(y) *y=(unsigned int) d4;
}

void position_gtk (int x, int y) { 
//  gdk_window_get_origin()
//  gdk_window_get_position(gwindow,&x,&y)
//  gdk_window_set_geometry_hints (GdkWindow *window, GdkGeometry *geometry, GdkWindowHints geom_mask);
	;
}



/* gtk callback function */
void on_mygtk_destroy (GtkObject *object, gpointer user_data) {
	if (!remote_en)
		loop_flag = 0;
	gwindow=NULL;
}

void on_mygtk_expose (GtkObject *object, gpointer user_data) {
	printf("expose\n");
//	render_gtk (buffer);
}

static gint on_mygtk_clicked( GtkWidget      *widget, GdkEventButton *event )
{
	if (event->button == 1 ) {
		gdk_window_resize(gwindow->window,movie_width,movie_height);
	}  else { 
		unsigned int my_Width,my_Height;
		getsize_gtk(&my_Width,&my_Height);

		if(event->button == 4) {
			float step=sqrt((float)my_Height);
			my_Width-=floor(step*((float)movie_width/(float)movie_height));
			my_Height-=step;
		}
		if(event->button == 5) {
			float step=sqrt((float)my_Height);
			my_Width+=floor(step*((float)movie_width/(float)movie_height));
			my_Height+=step;
		} 
		// resize to match movie aspect ratio
		if( ((float)movie_width/(float)movie_height) < ((float)my_Width/(float)my_Height) )
			my_Width=floor((float)my_Height * (float)movie_width / (float)movie_height);
		else my_Height=floor((float)my_Width * (float)movie_height / (float)movie_width);

		resize_gtk(my_Width,my_Height);
	}

#if 0 // To be continued...	
	else if (event->button == 4 ) {
		gdk_window_iconify (GTK_WINDOW(gwindow));
		gtk_window_set_default_size(GTK_WINDOW(gwindow),100,100);
	}
	else printf("other: %i\n",event->button);
#endif

	return TRUE;
}


#define SUP_GTK 1
#else
#define SUP_GTK 0
#endif



void resize_gtk (unsigned int x, unsigned int y) { 
#if HAVE_GTK
	gdk_window_resize(gwindow->window,x,y);
#endif
}

void getsize_gtk (unsigned int *x, unsigned int *y) {
#if HAVE_GTK
	gdk_window_resize(gwindow->window,x,y);
//	gint d0,d1,d2;
	gint d3,d4;
	gdk_window_get_size(gwindow->window,&d3,&d4);
//	gdk_window_get_geometry (gwindow->window, &d0,&d1,&d3,&d4,&d2);
	if(x) *x=(unsigned int) d3;
	if(y) *y=(unsigned int) d4;
#endif
}

void position_gtk (int x, int y) { 
#if HAVE_GTK
//  gdk_window_get_origin()
//  gdk_window_get_position(gwindow,&x,&y)
//  gdk_window_set_geometry_hints (GdkWindow *window, GdkGeometry *geometry, GdkWindowHints geom_mask);
	;
#endif
}




int open_window_gtk(int *argc, char ***argv) {
#if HAVE_GTK
	gtk_init(argc, argv);
	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

	gwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gimage = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(gwindow),gimage);
//	gtk_drawing_area_size(GTK_DRAWING_AREA(gimage), movie_width, movie_height);
//	gtk_widget_set_usize(GTK_WIDGET(gimage), movie_width, movie_height);
//	gtk_window_set_default_size(GTK_WINDOW(gwindow),movie_width,movie_height);
	gtk_signal_connect ((gpointer) gwindow, "destroy", on_mygtk_destroy, NULL);
//	gtk_signal_connect ((gpointer) gwindow, "destroy", gtk_widget_destroy, NULL);

	gtk_signal_connect ((gpointer) gimage, "expose", on_mygtk_expose, NULL);
//
	gtk_widget_add_events (gimage,
			GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK |
			GDK_BUTTON_PRESS_MASK| GDK_BUTTON_RELEASE_MASK);
 	gtk_signal_connect ((gpointer) gimage, "button_release_event", on_mygtk_clicked, NULL);


	gtk_widget_show(gimage);
	gtk_widget_show(gwindow);
	gdk_flush();
	gdk_window_resize(gwindow->window,movie_width,movie_height);
	while(gtk_events_pending()) gtk_main_iteration();
	gdk_flush();
		
	return 0;
#endif /* HAVE_GTK */
	return 1;
}

void close_window_gtk(void) {
#if HAVE_GTK
	if (gwindow) {
		// needed if closing the window via remote.
		gtk_widget_hide(gwindow);
		gtk_widget_destroy(gwindow);
	}
	gdk_flush();
	while(gtk_events_pending()) gtk_main_iteration();
	gdk_flush();
	gtk_main_quit();
#endif /* HAVE_GTK */
}

void render_gtk (uint8_t *mybuffer) {
#if HAVE_GTK
	int width=movie_width;
	int height=movie_height;
	unsigned int dest_width,dest_height; //get gtk window size

	getsize_gtk(&dest_width,&dest_height);

	if (dest_width<8 ) dest_width=8;
	if (dest_height<8 ) dest_height=8;

	if (dest_width==width && dest_height == height) { // no scaling 
		gdk_draw_rgb_image(gimage->window, gimage->style->fg_gc[gimage->state],
				0, 0, width, height, GDK_RGB_DITHER_MAX, (guchar*) mybuffer, 
				3*width);
	} else {
		GdkPixbuf *pixbuf, *scaled;

		pixbuf=  gdk_pixbuf_new_from_data  (mybuffer, GDK_COLORSPACE_RGB, 0, 8, width, height, width*3, NULL, NULL);
		scaled = gdk_pixbuf_scale_simple (pixbuf, dest_width, dest_height, GDK_INTERP_NEAREST);

		// the old way  gtk-1.2
		gdk_pixbuf_render_to_drawable (scaled, gimage->window, gimage->style->fg_gc[gimage->state],
				0,0,0,0,dest_width, dest_height, GDK_RGB_DITHER_NORMAL,0,0);

		// the new way gtk-2.0
//		gdk_draw_pixbuf (gimage->window, gimage->style->fg_gc[gimage->state], scaled,
//				0,0,0,0,dest_width, dest_height, GDK_RGB_DITHER_NORMAL,0,0);

		gdk_pixbuf_unref (scaled);
		gdk_pixbuf_unref (pixbuf);
	}

	gdk_flush();
	while(gtk_events_pending()) {
		gtk_main_iteration();
	} 
	gdk_flush();

#endif /* HAVE_GTK */
}

void handle_X_events_gtk (void) {
#if HAVE_GTK
	while(gtk_events_pending()) {
		gtk_main_iteration();
	} 
	gdk_flush();
#endif /* HAVE_GTK */
}



/*******************************************************************************
 * SDL
 */

#if HAVE_SDL

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

  SDL_Surface* sdl_screen;
  SDL_Overlay *sdl_overlay;
  SDL_Rect sdl_rect;

void position_sdl(int x, int y) {
	SDL_SysWMinfo info;

	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) > 0 ) {
	if ( info.subsystem == SDL_SYSWM_X11 ) {
			info.info.x11.lock_func();
	/* get root window size  - center window 
			int x, y;
			int w, h;
			w = DisplayWidth(info.info.x11.display,
			DefaultScreen(info.info.x11.display));

			h = DisplayHeight(info.info.x11.display,
			DefaultScreen(info.info.x11.display));
			x = (w - screen->w)/2;
			y = (h - screen->h)/2;
	*/
			XMoveWindow(info.info.x11.display, info.info.x11.wmwindow, x, y);
			info.info.x11.unlock_func();
		} 
	} 
} 

#define SUP_SDL 1
#else
#define SUP_SDL 0
#endif

void close_window_sdl(void) {
#if HAVE_SDL
	// TODO: free sdl stuff (sdl_overlay)
	SDL_Quit();
#endif /* HAVE_SDL */
}


#if 0
#ifdef USE_SDLTTF
// OSD - experimental 
//
#include <SDL_ttf.h>

TTF_Font *font;

#define FONT_PTSIZE     18
#define FONT_FILE       "arial.ttf"


SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };

int displaybox (SDL_Color *col, int yperc) {
	SDL_Rect dstrect;
	dstrect.x = (sdl_screen->w)*.2;
	dstrect.w = (sdl_screen->w)*.6;
	dstrect.y = (sdl_screen->h)*yperc/100; 
	dstrect.h = TTF_FontHeight(font);
	SDL_FillRect(sdl_screen, &dstrect, SDL_MapRGB(sdl_screen->format, col->r, col->g, col->b));
	SDL_UpdateRect(sdl_screen, dstrect.x, dstrect.y, dstrect.w, dstrect.h);
	return(0);
}

int InitTTF (void) {
	if(TTF_Init()==-1) {
		fprintf(stderr, "Error: unable to initialize TTF_SDL, %s\n", TTF_GetError());
		return (-1);
	}
//	atexit(TTF_Quit);
	font = TTF_OpenFont(FONT_FILE, FONT_PTSIZE);
	if ( font == NULL ) {
		fprintf(stderr, "Error: Couldn't load %d pt font from %s: %s\n", FONT_PTSIZE, FONT_FILE, SDL_GetError());
		return(-2);
	}
	TTF_SetFontStyle(font, TTF_STYLE_NORMAL);

	return (0);
}

int displaytext (char *string, SDL_Color *col, int yperc) {
        SDL_Surface *text;
        SDL_Rect dstrect;

        text = TTF_RenderText_Solid(font, string, *col);
        if ( text == NULL ) {
                fprintf(stderr, "Error: Couldn't render text: %s\n", SDL_GetError());
                return(-2);
        }

        dstrect.x = (sdl_screen->w - text->w)/2;
        dstrect.y = (sdl_screen->h)*yperc/100; 
	dstrect.w = text->w;
	dstrect.h = text->h;

        SDL_BlitSurface(text, NULL, sdl_screen, &dstrect);
	SDL_UpdateRect(sdl_screen, dstrect.x, dstrect.y, dstrect.w, dstrect.h);
        SDL_FreeSurface(text);
        return(0);

}
#endif /* USE_SDLTTF */
#endif /* 0 */

int open_window_sdl (int *argc, char ***argv) {
#if HAVE_SDL
	const SDL_VideoInfo *video_info;
	int video_bpp;

	if(SDL_Init(SDL_INIT_VIDEO) < 0) goto no_sdl;

//	InitTTF(); // FIXME: exit if rv..

	/* Get the "native" video mode */
	video_info = SDL_GetVideoInfo();
	switch (video_info->vfmt->BitsPerPixel) {
		case 16:
		case 32:
			video_bpp = video_info->vfmt->BitsPerPixel;
			break;
		default:
			video_bpp = 16;
			break;
	} 

	sdl_screen = SDL_SetVideoMode(movie_width,movie_height, video_bpp,SDL_HWSURFACE | SDL_RESIZABLE);
	SDL_WM_SetCaption("xjadeo", "xjadeo");
	sdl_overlay = SDL_CreateYUVOverlay(movie_width, movie_height, SDL_YV12_OVERLAY, sdl_screen);
	if((!sdl_overlay || (!sdl_overlay->hw_overlay) || SDL_LockYUVOverlay(sdl_overlay)<0)) {
		goto no_overlay;
	}
	sdl_rect.x = 0;
	sdl_rect.y = 0;
	sdl_rect.w = sdl_overlay->w;
	sdl_rect.h = sdl_overlay->h;

	if ( sdl_overlay->pitches[0] != movie_width ||
			sdl_overlay->pitches[1] != sdl_overlay->pitches[2] ) {
		printf("unsupported SDL YV12.\n"); 
		goto no_overlay;
	}  
#if 0  // verify YUV alignment
	if ((sdl_overlay->pixels[1] - sdl_overlay->pixels[0]) != ( movie_width * movie_height) ||
			(sdl_overlay->pixels[2] - sdl_overlay->pixels[1]) != ( movie_width * movie_height /4) ) {
		printf("unsupported SDL YV12 pixel buffer alignment.\n"); 
		goto no_overlay;
	}  
#endif
	return(0);
 
no_overlay: 
	if(sdl_overlay) SDL_FreeYUVOverlay(sdl_overlay); 
	SDL_Quit();
no_sdl:
#endif /* HAVE_SDL */
	return 1;
}

void resize_sdl (unsigned int x, unsigned int y) { 
#if HAVE_SDL
	sdl_screen = SDL_SetVideoMode(x, y, 0, SDL_RESIZABLE | SDL_SWSURFACE);
	sdl_rect.w=x;
	sdl_rect.h=y;
#endif /* HAVE_SDL */
}

void getsize_sdl (unsigned int *x, unsigned int *y) {
#if HAVE_SDL
	if(x) *x = sdl_rect.w;
	if(y) *y = sdl_rect.h;
#endif /* HAVE_SDL */
}


void render_sdl (uint8_t *mybuffer) {
#if HAVE_SDL
	/* http://www.fourcc.org/indexyuv.htm */

	size_t Ylen=sdl_overlay->pitches[0] * movie_height;
	size_t UVlen=sdl_overlay->pitches[1] * movie_height/2; 
// decode ffmpeg - YUV 
	uint8_t *Yptr=mybuffer;
	uint8_t *Uptr=Yptr + Ylen;
	uint8_t *Vptr=Uptr + UVlen;

// encode SDL YV12
	memcpy(sdl_overlay->pixels[0],Yptr,Ylen); // Y
	memcpy(sdl_overlay->pixels[1],Vptr,UVlen); //V
	memcpy(sdl_overlay->pixels[2],Uptr,UVlen); // U


	SDL_UnlockYUVOverlay(sdl_overlay);
	SDL_DisplayYUVOverlay(sdl_overlay, &sdl_rect);
	SDL_LockYUVOverlay(sdl_overlay);

#if 0  // SDL - OSD test
	//	displaybox(&white, 10);
		displaytext("xjadeo!",&white,40);
#endif


#endif /* HAVE_SDL */
}


void newsrc_sdl (void) {
#if HAVE_SDL
	if(sdl_overlay) SDL_FreeYUVOverlay(sdl_overlay); 
	sdl_overlay = SDL_CreateYUVOverlay(movie_width, movie_height, SDL_YV12_OVERLAY, sdl_screen);
#endif /* HAVE_SDL */
}

void handle_X_events_sdl (void) {
#if HAVE_SDL
	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		switch(ev.type){
			case SDL_VIDEOEXPOSE: // SDL render event
				render_sdl(buffer);
			//	printf("SDL render event.\n");
				break;
			case SDL_QUIT:
			//	printf("SDL quit event.\n");
				loop_flag=0;
				break;
			case SDL_KEYDOWN:
			//	printf("SDL key down event.");
				if(ev.key.keysym.sym==SDLK_ESCAPE) {
					loop_flag=0;
				}
				break;
			case SDL_VIDEORESIZE:
			        sdl_screen = SDL_SetVideoMode(ev.resize.w, ev.resize.h, 0, SDL_RESIZABLE | SDL_HWSURFACE);
				sdl_rect.w=ev.resize.w;
				sdl_rect.h=ev.resize.h;
				break;
			case SDL_MOUSEBUTTONUP:
				if(ev.button.button == SDL_BUTTON_LEFT) {
						resize_sdl(movie_width,movie_height);
				} else 
#if 0 // fix aspect only on right-button and scroll. 
					 if(ev.button.button == SDL_BUTTON_WHEELUP ||
						ev.button.button == SDL_BUTTON_WHEELDOWN ||
						ev.button.button == SDL_BUTTON_RIGHT)
#endif
					 { 

					unsigned int my_Width,my_Height;
					getsize_sdl(&my_Width,&my_Height);

					if(ev.button.button == SDL_BUTTON_WHEELUP) {
						float step=sqrt((float)my_Height);
						my_Width-=floor(step*((float)movie_width/(float)movie_height));
						my_Height-=step;
					}
					if(ev.button.button == SDL_BUTTON_WHEELDOWN) {
						float step=sqrt((float)my_Height);
						my_Width+=floor(step*((float)movie_width/(float)movie_height));
						my_Height+=step;
					} 
					// resize to match movie aspect ratio
					if( ((float)movie_width/(float)movie_height) < ((float)my_Width/(float)my_Height) )
						my_Width=floor((float)my_Height * (float)movie_width / (float)movie_height);
					else my_Height=floor((float)my_Width * (float)movie_height / (float)movie_width);

					resize_sdl(my_Width,my_Height);
				}
				break;
			default: /* unhandled event */
				;
		}
	}
	// TODO: SDL_Event SDL_Quit_Event -> loop_flag = 0;
#endif /* HAVE_SDL */
}

/*******************************************************************************
 * XV !!!
 */

#if HAVE_LIBXV

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

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

// TODO: support other YUV Xv - ffmpeg combinations
// (depending on hardware and X) Xv can do more than YV12 ...
#define FOURCC_YV12  0x32315659
  int xv_pic_format = FOURCC_YV12;

#include <sys/ipc.h>
#include <sys/shm.h>

#define SUP_LIBXV 1
#else
#define SUP_LIBXV 0
#endif /* HAVE_LIBXV */

#if HAVE_LIBXV


void allocate_xvimage (void) {
  // YV12 has 12 bits per pixel. 8bitY 2+2 UV
  xv_len = movie_width * movie_height * 3 / 2 ;

  /* shared memory allocation etc.  */
  xv_image = XvShmCreateImage(xv_dpy, xv_port,
			 xv_pic_format, NULL, // FIXME: use xjadeo buffer directly 
			 xv_dwidth, xv_dheight, //768, 486, //720, 576,
			 &xv_shminfo);

// if (xv_len != xv_image->data_size) BAILOUT

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
#endif /* HAVE_LIBXV */

void get_window_size_xv (unsigned int *my_Width, unsigned int *my_Height) {
#if HAVE_LIBXV
	int dummy0,dummy1;
	unsigned int dummy_u0, dummy_u1;
	Window dummy_w;
	XGetGeometry(xv_dpy, xv_win, &dummy_w, &dummy0,&dummy1,my_Width,my_Height,&dummy_u0,&dummy_u1);
#endif
}

void resize_xv (unsigned int x, unsigned int y) { 
#if HAVE_LIBXV
	XResizeWindow(xv_dpy, xv_win, x, y);
#endif
}

void position_xv (int x, int y) { 
#if HAVE_LIBXV
	XMoveWindow(xv_dpy, xv_win,x,y);
#endif
}


void render_xv (uint8_t *mybuffer) {
#if HAVE_LIBXV

	if (!xv_buffer || !mybuffer) return;

	size_t Ylen  = movie_width * movie_height;
	size_t UVlen = movie_width * movie_height/4; 

// decode ffmpeg - YUV 
	uint8_t *Yptr=mybuffer; // Y 
	uint8_t *Uptr=Yptr + Ylen; // U
	uint8_t *Vptr=Uptr + UVlen; // V

// encode YV12
	memcpy(xv_buffer,Yptr,Ylen); // Y
	memcpy(xv_buffer+Ylen,Vptr,UVlen); //V
	memcpy(xv_buffer+Ylen+UVlen,Uptr,UVlen); // U

	XvShmPutImage(xv_dpy, xv_port,
		xv_win, xv_gc,
		xv_image,
		0, 0,				/* sx, sy */
		xv_swidth, xv_sheight,		/* sw, sh */
		0,  0,		/* dx, dy */
		xv_dwidth, xv_dheight,		/* dw, dh */
		True);
	XFlush(xv_dpy);
#endif /* HAVE_LIBXV */
}



void handle_X_events_xv (void) {
#if HAVE_LIBXV
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
			default:
				break;
		}
	}
#endif
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
#if HAVE_LIBXV
  char *w_name ="xjadeo";
  char *i_name ="xjadeo";
  size_t 	ad_cnt;
  int		scn_id,
                fmt_cnt,
                got_port, got_fmt,
                i, k;
  XGCValues	values;
  XSizeHints	hints;
  XWMHints	wmhints;
  XTextProperty	x_wname, x_iname;

  XvAdaptorInfo	*ad_info;
  XvImageFormatValues	*fmt_info;

  if(!(xv_dpy = XOpenDisplay(NULL))) {
    return 1;
  } /* if */

  xv_rwin = DefaultRootWindow(xv_dpy);
  scn_id = DefaultScreen(xv_dpy);

  //if (!XShmQueryExtension(xv_dpy)) BAILOUT

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
      for(got_fmt = False, k = 0; k < fmt_cnt; ++k) {
	      // TODO: support all formats that ffmpeg can 'produce'
	if (xv_pic_format == fmt_info[k].id) {
	  got_fmt = True;
	  break;
	} /* if */
      } /* for */
      if (!got_fmt) {
	fprintf(stderr,
		"Xv: %s: format %#08x is NOT in format list ( ",
		ad_info[i].name,
                xv_pic_format);
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

  XSelectInput(xv_dpy, xv_win, ButtonPressMask | ButtonReleaseMask | ExposureMask | StructureNotifyMask);
  XMapRaised(xv_dpy, xv_win);

  if ((xv_del_atom = XInternAtom(xv_dpy, "WM_DELETE_WINDOW", True)) != None)
    XSetWMProtocols(xv_dpy, xv_win, &xv_del_atom, 1);

  xv_gc = XCreateGC(xv_dpy, xv_win, 0, &values);

  allocate_xvimage ();


  return 0;
#endif /* HAVE_LIBXV */
  return 1;
}

void close_window_xv(void) {
#if HAVE_LIBXV
		 
	//XvFreeAdaptorInfo(ai);
	XvStopVideo(xv_dpy, xv_port, xv_win);
	if(xv_shminfo.shmaddr) {
		XShmDetach(xv_dpy, &xv_shminfo);
		shmdt(xv_shminfo.shmaddr);
	}
	if(xv_image) XFree(xv_image);
	XSync(xv_dpy, False);
	XCloseDisplay(xv_dpy);
#endif /* HAVE_LIBXV */
}





/*******************************************************************************
 *
 * X11 / ImLib 
 */

#if HAVE_IMLIB

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>

#include <Imlib.h>

	Display   *display;
	ImlibData *imlib = NULL;
	Window    window;
	GC        gc;
	int       screen_number;
	int       depth;
	XImage    *image;
	Pixmap    pxm, pxmmask;
	Atom      del_atom;           /* WM_DELETE_WINDOW atom   */


#define SUP_IMLIB 1
#else
#define SUP_IMLIB 0
#endif 

void get_window_size_imlib (unsigned int *my_Width, unsigned int *my_Height) {
#if HAVE_IMLIB
	int dummy0,dummy1;
	unsigned int dummy_u0, dummy_u1;
	Window dummy_w;
	XGetGeometry(display, window, &dummy_w, &dummy0,&dummy1,my_Width,my_Height,&dummy_u0,&dummy_u1);
#endif 
}

int open_window_imlib (int *argc, char ***argv) {
#if HAVE_IMLIB
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
  XSelectInput(display, window, ExposureMask | ButtonPressMask | ButtonReleaseMask |StructureNotifyMask ); 
  
  XMapWindow(display, window);
      
  /* express interest in WM killing this app */
  if ((del_atom = XInternAtom(display, "WM_DELETE_WINDOW", True)) != None)
    XSetWMProtocols(display, window, &del_atom, 1);
	return 0;
#endif /* HAVE_IMLIB */
	return 1;
}


void close_window_imlib(void)
{
#if HAVE_IMLIB
	XFreeGC(display, gc);
	XCloseDisplay(display);
	//imlib=NULL;
#endif /* HAVE_IMLIB */
}

        
void render_imlib (uint8_t *mybuffer) {
#if HAVE_IMLIB
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
      XClearWindow(display,window);       
//       XSync(display, True);     
// No need to sync. XPending will take care in the event loop.
      Imlib_free_pixmap(imlib, pxm);
      Imlib_kill_image(imlib, iimage);
#endif /* HAVE_IMLIB */
}

void newsrc_imlib (void) { 
	; // nothing to do :)
}


void handle_X_events_imlib (void) {
#if HAVE_IMLIB
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
#endif
}

void resize_imlib (unsigned int x, unsigned int y) { 
#if HAVE_IMLIB
	XResizeWindow(display, window, x, y);
#endif
}

void position_imlib (int x, int y) { 
#if HAVE_IMLIB
	XMoveWindow(display, window, x, y);
#endif
}

/*******************************************************************************
 *
 * NULL Video Output 
 */ 

int open_window_null (int *argc, char ***argv) { return (1); }
void close_window_null (void) { ; }
void render_null (uint8_t *mybuffer) { ; }
void handle_X_events_null (void) { ; }
void newsrc_null (void) { ; }
void resize_null (unsigned int x, unsigned int y) { ; }
void getsize_null (unsigned int *x, unsigned int *y) { if(x)*x=0; if(y)*y=0; }
void position_null (int x, int y) { ; }



/*******************************************************************************
 *
 * SubTitle Render - On Screen Display
 */


extern unsigned char ST_image[][ST_WIDTH];
extern int ST_rightend;

#define ST_BG ((OSD_mode&OSD_BOX)?0:1)

void OSD_renderYUV (uint8_t *mybuffer, char *text, int xpos, int yperc) {
	int x,y, xalign, yalign;
	size_t Uoff  = movie_width * movie_height;
	size_t Voff = Uoff + movie_width * movie_height/4; 

	if ( render_font(OSD_fontfile, text) ) return;

	if (xpos == OSD_LEFT) xalign=ST_PADDING; // left
	else if (xpos == OSD_RIGHT) xalign=movie_width-ST_PADDING-ST_rightend; // right
	else xalign=(movie_width-ST_rightend)/2; // center

	yalign= (movie_height - ST_HEIGHT) * yperc /100.0;

	for (x=0; x<ST_rightend && (x+xalign) < movie_width ;x++)
		for (y=0; y<ST_HEIGHT && (y+yalign) < movie_width;y++) {
			int dx=(x+xalign);
			int dy=(y+yalign);

			int yoff=(dx+movie_width*dy);
			int uvoff=((dx/2)+movie_width/2*(dy/2));

			if (ST_image[y][x]>= ST_BG){
				mybuffer[yoff]=ST_image[y][x];
				mybuffer[Uoff+uvoff]=0x80;
				mybuffer[Voff+uvoff]=0x80;
			}
	}
}

void OSD_renderRGB (uint8_t *mybuffer, char *text, int xpos, int yperc) {
	int x,y, xalign, yalign;

	if ( render_font(OSD_fontfile, text) ) return;

	if (xpos == OSD_LEFT) xalign=ST_PADDING; // left
	else if (xpos == OSD_RIGHT) xalign=movie_width-ST_PADDING-ST_rightend; // right
	else xalign=(movie_width-ST_rightend)/2; // center

	yalign= (movie_height - ST_HEIGHT) * yperc /100.0;

	for (x=0; x<ST_rightend && (x+xalign) < movie_width ;x++)
		for (y=0; y<ST_HEIGHT && (y+yalign) < movie_width;y++) {
			int dx=(x+xalign);
			int dy=(y+yalign);
			int pos=3*(dx+movie_width*dy);

			if (ST_image[y][x]>= ST_BG){
				mybuffer[pos]=ST_image[y][x];
				mybuffer[pos+1]=ST_image[y][x];
				mybuffer[pos+2]=ST_image[y][x];
			}
	}
}



/*******************************************************************************
 *
 * xjadeo fn
 */


#include <ffmpeg/avcodec.h> // needed for PIX_FMT 
#include <ffmpeg/avformat.h>

typedef struct {
	int render_fmt; // the format ffmpeg should write to the shared buffer
	int supported; // 1: format compiled in -- 0: not supported 
	const char *name; // 
	void (*render)(uint8_t *mybuffer);
	int (*open)(int *argc, char ***argv);
	void (*close)(void);
	void (*eventhandler)(void);
	void (*newsrc)(void);
	void (*resize)(unsigned int x, unsigned int y);
	void (*getsize)(unsigned int *x, unsigned int *y);
	void (*position)(int x, int y);
}vidout;



const vidout VO[] = {
	{ PIX_FMT_RGB24,   1, 		"NULL", &render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null},
	{ PIX_FMT_RGB24,   SUP_GTK,	"GTK", &render_gtk, &open_window_gtk, &close_window_gtk, &handle_X_events_gtk, &newsrc_null, &resize_gtk, &getsize_gtk, &position_null},
	{ PIX_FMT_YUV420P, SUP_SDL,	"SDL",  &render_sdl, &open_window_sdl, &close_window_sdl, &handle_X_events_sdl, &newsrc_sdl, &resize_sdl, &getsize_sdl, &position_sdl},
	{ PIX_FMT_RGB24,   SUP_IMLIB,   "x11 - ImLib", &render_imlib, &open_window_imlib, &close_window_imlib, &handle_X_events_imlib, &newsrc_imlib, &resize_imlib, &get_window_size_imlib, &position_imlib},
	{ PIX_FMT_YUV420P, SUP_LIBXV,	"x11 - XV", &render_xv, &open_window_xv, &close_window_xv, &handle_X_events_xv, &newsrc_xv, &resize_xv, &get_window_size_xv, &position_xv},
	{-1,-1,NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};


int VOutput = 0;

void dump_vopts (void) {
	int i=0;
	fprintf (stdout, "Video Output Modes: \n");
	fprintf (stdout, " --vo 0 # autodetect (default)\n");

	while (VO[++i].supported>=0) {
		fprintf (stdout, " --vo %i # %s %s\n",i,VO[i].name, 
				VO[i].supported?"(supported by this xjadeo)":"(NOT compiled in this xjadeo)");
	}
}

int vidoutmode(int user_req) {
	if (user_req < 0) {
		dump_vopts();
		exit (0);
	}

	// auto-detect
	int i=0;
	while (VO[++i].supported>=0) {
		if (VO[i].supported) {
			VOutput=i;
		}
	}

	if (user_req < i && user_req>0 )
		if (VO[user_req].supported) VOutput=user_req;

	return VO[VOutput].render_fmt;
}

void render_buffer (uint8_t *mybuffer) {
	if (!mybuffer) return;

	// render OSD on buffer 
	if (OSD_mode&1 && VO[VOutput].render_fmt == PIX_FMT_YUV420P) OSD_renderYUV (mybuffer, OSD_frame, OSD_fx, OSD_fy);
	if (OSD_mode&1 && VO[VOutput].render_fmt == PIX_FMT_RGB24) OSD_renderRGB (mybuffer, OSD_frame, OSD_fx, OSD_fy);

	if (OSD_mode&2 && VO[VOutput].render_fmt == PIX_FMT_YUV420P) OSD_renderYUV (mybuffer, OSD_text, OSD_tx, OSD_ty);
	if (OSD_mode&2 && VO[VOutput].render_fmt == PIX_FMT_RGB24) OSD_renderRGB (mybuffer, OSD_text, OSD_tx, OSD_ty);

	VO[VOutput].render(buffer); // buffer = mybuffer (so far no share mem or sth)
}


void open_window(int *argc, char ***argv) {
	loop_run=1;
	if (!want_quiet)
		printf("Video output: %s\n",VO[VOutput].name);
	if ( VO[VOutput].open(argc,argv) ) { 
		fprintf(stderr,"Could not open video output.\n");
		VOutput=0;
		loop_run=0;
	//	exit(1);
	}
}

void close_window(void) {
	VO[VOutput].close();
	VOutput=0;
	loop_run=0;
}

void handle_X_events (void) {
	VO[VOutput].eventhandler();
}

void newsourcebuffer (void) {
	VO[VOutput].newsrc();
}

int getvidmode (void) {
	return(VOutput);
}

void Xgetsize (unsigned int *x, unsigned int *y) {
	VO[VOutput].getsize(x,y);
}

void Xresize (unsigned int x, unsigned int y) {
	VO[VOutput].resize(x,y);
}

void Xposition (int x, int y) {
	VO[VOutput].position(x,y);
}
