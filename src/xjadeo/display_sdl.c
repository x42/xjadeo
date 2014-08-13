/* xjadeo - SDL display variant
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

/*******************************************************************************
 * SDL
 */

#ifdef HAVE_SDL

#define MYSDLFLAGS (SDL_HWSURFACE | SDL_RESIZABLE | SDL_DOUBLEBUF)

static SDL_Surface* sdl_screen;
static SDL_Overlay *sdl_overlay;
static SDL_Rect sdl_rect;
static SDL_Rect sdl_dest_rect;
static int sdl_pic_format= SDL_YV12_OVERLAY; // fourcc

static int full_screen_width = 1024;
static int full_screen_height = 768;
static int	sdl_ontop = 0;
static int sdl_full_screen = 0;
static SDL_Rect sdl_oldsize;

static void calc_letterbox(int src_w, int src_h, int out_w, int out_h, int *sca_w, int *sca_h);
static void resized_sdl ();

void close_window_sdl(void) {
	if(sdl_overlay) SDL_FreeYUVOverlay(sdl_overlay); 
	SDL_Quit();
}

int open_window_sdl (void) {
	const SDL_VideoInfo *video_info;
	int video_bpp;

	if(SDL_Init(SDL_INIT_VIDEO) < 0) goto no_sdl;

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

	full_screen_width = video_info->current_w;
	full_screen_height = video_info->current_h;

	sdl_rect.x = 0;
	sdl_rect.y = 0;
	sdl_rect.h = ffctv_height;
	sdl_rect.w = ffctv_width;

	sdl_screen = SDL_SetVideoMode(sdl_rect.w, sdl_rect.h, video_bpp,MYSDLFLAGS);
	SDL_WM_SetCaption("xjadeo", "xjadeo");

	newsrc_sdl();

	if((!sdl_overlay)) 
		fprintf(stderr, "NO OVERLAY\n");
	if((!sdl_overlay || SDL_LockYUVOverlay(sdl_overlay)<0)) {
		printf("OVERLAY error.\n");
		goto no_overlay;
	}

	resized_sdl();

	if (sdl_overlay->pitches[0] != movie_width ||
			sdl_overlay->pitches[1] != sdl_overlay->pitches[2] ) {
		fprintf(stderr,"unsupported SDL YV12.\n"); 
		goto no_overlay;
	}  

	if (start_ontop) {
		sdl_set_ontop(1);
	}
	if (start_fullscreen) {
		sdl_toggle_fullscreen(1);
	}

	return(0);
 
no_overlay: 
	if(sdl_overlay) SDL_FreeYUVOverlay(sdl_overlay); 
	SDL_Quit();
no_sdl:
	return 1;
}

static void black_border_sdl(SDL_Rect b) {
	SDL_FillRect(sdl_screen, &b, SDL_MapRGB(sdl_screen->format, 0,0,0));
	SDL_UpdateRect(sdl_screen, b.x, b.y, b.w, b.h);
}

static void resized_sdl () {
	if (!want_letterbox) {
		memcpy(&sdl_dest_rect, &sdl_rect, sizeof (SDL_Rect));
		return;
	}
	/* want letterbox: */
	int dw,dh;
	calc_letterbox(movie_width, movie_height, sdl_rect.w, sdl_rect.h, &dw, &dh);
	sdl_dest_rect.w = dw;
	sdl_dest_rect.h = dh;
	sdl_dest_rect.x = (sdl_rect.w - sdl_dest_rect.w)/2;
	sdl_dest_rect.y = (sdl_rect.h - sdl_dest_rect.h)/2;

	SDL_Rect b;
	if (sdl_dest_rect.y >0 ){
		b.x=0;b.y=0; b.w=sdl_rect.w; b.h=sdl_dest_rect.y;
		black_border_sdl(b);

		b.x=0;b.y=sdl_rect.h - sdl_dest_rect.y; b.w=sdl_rect.w; b.h=sdl_dest_rect.y;
		black_border_sdl(b);
	}
	if (sdl_dest_rect.x >0 ){

		b.x=0;b.y=0; b.w=sdl_dest_rect.x; b.h=sdl_rect.h;
		black_border_sdl(b);

		b.x=sdl_rect.w - sdl_dest_rect.x;b.y=0; b.w=sdl_dest_rect.x; b.h=sdl_rect.h;
		black_border_sdl(b);
	}
}

void mousecursor_sdl(int action) {
	if (action==2) hide_mouse ^= 1;
	else hide_mouse = action ? 1 : 0;
	SDL_ShowCursor(!hide_mouse);
}

void resize_sdl (unsigned int x, unsigned int y) { 
	sdl_screen = SDL_SetVideoMode(x, y, 0, MYSDLFLAGS);
	sdl_rect.w=x;
	sdl_rect.h=y;
	resized_sdl();
	force_redraw=1;
}

void getsize_sdl (unsigned int *x, unsigned int *y) {
	if(x) *x = sdl_rect.w;
	if(y) *y = sdl_rect.h;
}

void get_window_pos_sdl (int *rx, int *ry) {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) > 0 ) {
#ifdef PLATFORM_WINDOWS

#if 0 // legacy
	WINDOWINFO w;
	GetWindowInfo(info.window, &w);
	*x= w.rcWindow.left;
	*y= w.rcWindow.top;
	printf("%ld - %ld\n", w.rcWindow.left, w.rcWindow.top);
#endif
		RECT rect;
		//GetWindowRect() <> SetWindowPos()
		if (GetClientRect(info.window, &rect)) {
			*rx = rect.left;
			*ry = rect.top;
			return;
		}
#elif (defined HAVE_LIBXV || defined HAVE_IMLIB2)
		if (info.subsystem == SDL_SYSWM_X11 ) {
			// NB. with SDL window decorations are not taken into account :(
			Window	dummy;
			info.info.x11.lock_func();
			XTranslateCoordinates(info.info.x11.display, info.info.x11.wmwindow, DefaultRootWindow(info.info.x11.display), 0, 0, rx, ry, &dummy);
			while (dummy !=None) {
				int x = 0;
				int y = 0;
				XTranslateCoordinates(info.info.x11.display, info.info.x11.wmwindow, dummy, 0, 0, &x, &y, &dummy);
				if (dummy!=None) {
					(*rx)-=x; (*ry)-=y;
				} else {
					(*rx)+=x; (*ry)+=y;
				}
			}
			info.info.x11.unlock_func();
			return;
		}
#endif
	}
	if(rx) *rx=1;
	if(ry) *ry=1;
}

void position_sdl(int x, int y) {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) > 0 ) {
#ifdef PLATFORM_WINDOWS
	SetWindowPos(info.window,
			sdl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
			x, y, sdl_rect.w, sdl_rect.h, 0);
#endif
#if (defined HAVE_LIBXV || defined HAVE_IMLIB2)
	if ( info.subsystem == SDL_SYSWM_X11 ) {
			info.info.x11.lock_func();
			XMoveWindow(info.info.x11.display, info.info.x11.wmwindow, x, y);
			info.info.x11.unlock_func();
		} 
#endif
	} 
}

#if (defined HAVE_LIBXV || defined HAVE_IMLIB2)
static void net_wm_set_property(char *atom, int state) {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) <= 0 ) {
		return;
	}

	XEvent xev;
	int set = _NET_WM_STATE_ADD;
	Atom type, property;

	if (state == _NET_WM_STATE_TOGGLE) set = _NET_WM_STATE_TOGGLE;
	else if (!state) set = _NET_WM_STATE_REMOVE;

	type = XInternAtom(info.info.x11.display, "_NET_WM_STATE", True);
	if (type == None) return;
	property = XInternAtom(info.info.x11.display, atom, 0);
	if (property == None) return;

	xev.type = ClientMessage;
	xev.xclient.type = ClientMessage;
	xev.xclient.window = info.info.x11.wmwindow;
	xev.xclient.message_type = type;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = set;
	xev.xclient.data.l[1] = property;
	xev.xclient.data.l[2] = 0;

	if (!XSendEvent(info.info.x11.display, DefaultRootWindow(info.info.x11.display), False,
				SubstructureRedirectMask|SubstructureNotifyMask, &xev))
	{
			fprintf(stderr,"error changing X11 property\n");
	}
}
#endif

void sdl_set_ontop (int action) {
	if (action==2) sdl_ontop^=1;
	else sdl_ontop=action;

#ifdef PLATFORM_WINDOWS
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) > 0 ) {
		SetWindowPos(info.window,
				sdl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
				0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
#elif (defined HAVE_LIBXV || defined HAVE_IMLIB2)
	net_wm_set_property("_NET_WM_STATE_ABOVE", action);
#endif
}

int sdl_get_ontop () {
	return sdl_ontop;
}

void render_sdl (uint8_t *mybuffer) {
	/* http://www.fourcc.org/indexyuv.htm */

	size_t Ylen= movie_width * movie_height;
	size_t UVlen= movie_width/2 * movie_height/2; 

	// decode ffmpeg - YUV 
	uint8_t *Yptr=mybuffer;
	uint8_t *Uptr=Yptr + Ylen;
	uint8_t *Vptr=Uptr + UVlen;

	if (sdl_pic_format == SDL_YV12_OVERLAY) { 
	// encode SDL YV12
		stride_memcpy(sdl_overlay->pixels[0],Yptr,movie_width,movie_height,sdl_overlay->pitches[0],movie_width);//Y
		stride_memcpy(sdl_overlay->pixels[1],Vptr,movie_width/2,movie_height/2,sdl_overlay->pitches[1],movie_width/2);//V
		stride_memcpy(sdl_overlay->pixels[2],Uptr,movie_width/2,movie_height/2,sdl_overlay->pitches[2],movie_width/2);//U
	} else {
	// encode SDL YUV
		stride_memcpy(sdl_overlay->pixels[0],Yptr,movie_width,movie_height,sdl_overlay->pitches[0],movie_width);//Y
		stride_memcpy(sdl_overlay->pixels[1],Uptr,movie_width/2,movie_height/2,sdl_overlay->pitches[1],movie_width/2);//U
		stride_memcpy(sdl_overlay->pixels[2],Vptr,movie_width/2,movie_height/2,sdl_overlay->pitches[2],movie_width/2);//V
	}

	SDL_UnlockYUVOverlay(sdl_overlay);
	SDL_DisplayYUVOverlay(sdl_overlay, &sdl_dest_rect);
	SDL_LockYUVOverlay(sdl_overlay);
}

int sdl_get_fullscreen () {
	return (sdl_full_screen);
}

void sdl_toggle_fullscreen(int action) {
  if (sdl_full_screen && action !=1) {
    sdl_rect.w=sdl_oldsize.w; sdl_rect.h=sdl_oldsize.h;
    sdl_screen = SDL_SetVideoMode(sdl_rect.w, sdl_rect.h, 0, MYSDLFLAGS);
    sdl_full_screen=0;
   // dv_center_window(sdl_screen);
	}
	else if (!sdl_full_screen && action !=0) {
    sdl_oldsize.w=sdl_rect.w; sdl_oldsize.h=sdl_rect.h;
    sdl_rect.w= full_screen_width;
    sdl_rect.h= full_screen_height;
    sdl_screen = SDL_SetVideoMode(sdl_rect.w, sdl_rect.h, 0, (MYSDLFLAGS & ~SDL_RESIZABLE) | SDL_FULLSCREEN );
    sdl_full_screen=1;
  }
	resized_sdl();
	force_redraw=1;
}

static void calc_letterbox(int src_w, int src_h, int out_w, int out_h, int *sca_w, int *sca_h) {
	const float asp_src = movie_aspect?movie_aspect:(float)src_w/src_h;
  if (asp_src * out_h > out_w) {
    (*sca_w)=out_w;
    (*sca_h)=(int)round((float)out_w/asp_src);
	} else {
    (*sca_h)=out_h;
    (*sca_w)=(int)round((float)out_h*asp_src);
	}
}

void sdl_letterbox_change(void) {
	resized_sdl();
	force_redraw=1;
}

void newsrc_sdl (void) {
	if(sdl_overlay) SDL_FreeYUVOverlay(sdl_overlay);
// FIXME: on linux the SDL_*_OVERLAY are defined as FOURCC numbers rather than beeing abstract
// so we could try other ffmpeg/lqt compatible 420P formats as I420 (0x30323449)
	sdl_overlay = SDL_CreateYUVOverlay(movie_width, movie_height, 0x30323449, sdl_screen);
	sdl_pic_format=0x30323449;
	if(!sdl_overlay || (!sdl_overlay->hw_overlay)) {
		sdl_overlay = SDL_CreateYUVOverlay(movie_width, movie_height, SDL_YV12_OVERLAY, sdl_screen);
		sdl_pic_format=SDL_YV12_OVERLAY;
	}
}

void handle_X_events_sdl (void) {
	SDL_Event ev;
	unsigned int key;
	while (SDL_PollEvent(&ev)) {
		switch(ev.type){
			case SDL_VIDEOEXPOSE: // SDL render event
				render_sdl(buffer);
				break;
			case SDL_QUIT:
				if ((interaction_override&OVR_QUIT_KEY) == 0) loop_flag=0;
				break;
			case SDL_KEYDOWN:
				key = ev.key.keysym.sym;
				if(ev.key.keysym.sym==SDLK_ESCAPE) {
					if ((interaction_override&OVR_QUIT_KEY) == 0) {
						loop_flag=0;
					} else {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
					}
				} else if(ev.key.keysym.sym==SDLK_s) {
					ui_osd_tc();
				} else if(ev.key.keysym.sym==SDLK_a) {
					sdl_set_ontop(sdl_ontop^=1);
				} else if(ev.key.keysym.sym==SDLK_f) {
					sdl_toggle_fullscreen(2);
				} else if(ev.key.keysym.sym==SDLK_l) {
						want_letterbox=!want_letterbox; 
						sdl_letterbox_change();
				} else if(ev.key.keysym.sym==SDLK_m) { 
					mousecursor_sdl(2);
				} else if(ev.key.keysym.sym==SDLK_v) {
					ui_osd_fn();
				} else if(ev.key.keysym.sym==SDLK_b) {
					ui_osd_box();
				} else if(ev.key.keysym.sym==SDLK_i) {
					ui_osd_fileinfo();
				} else if(ev.key.keysym.sym==SDLK_g) {
					ui_osd_geo();
				} else if(ev.key.keysym.sym== SDLK_c && ev.key.keysym.mod&KMOD_SHIFT) {
					ui_osd_clear();
				} else if(ev.key.keysym.sym== SDLK_LESS || (ev.key.keysym.sym== SDLK_COMMA && ev.key.keysym.mod&KMOD_SHIFT) ) { // '<'
					XCresize_scale(-1);
				} else if(ev.key.keysym.sym== SDLK_GREATER || (ev.key.keysym.sym== SDLK_PERIOD && ev.key.keysym.mod&KMOD_SHIFT) ) { // '>'
					XCresize_scale(1);
				} else if(ev.key.keysym.sym==SDLK_PERIOD) {
					XCresize_percent(100);
					resize_sdl(ffctv_width, ffctv_height);
				} else if(ev.key.keysym.sym== SDLK_COMMA) { // ','
					XCresize_aspect(0);
				} else if(ev.key.keysym.sym==SDLK_o) {
					ui_osd_offset_cycle();
				} else if(ev.key.keysym.sym==SDLK_p) {
					ui_osd_permute();
				} else if(ev.key.keysym.sym== SDLK_BACKSLASH) {
					XCtimeoffset(0, (unsigned int) key);
				} else if(ev.key.keysym.sym== SDLK_EQUALS && ev.key.keysym.mod&KMOD_SHIFT) { // '+' SDLK_PLUS does not work :/
					XCtimeoffset(1, (unsigned int) key);
				} else if(ev.key.keysym.sym==SDLK_MINUS) {
					XCtimeoffset(-1, (unsigned int) key);
				} else if(ev.key.keysym.sym== SDLK_LEFTBRACKET && ev.key.keysym.mod&KMOD_SHIFT) { // '{'
					XCtimeoffset(-2, (unsigned int) key);
				} else if(ev.key.keysym.sym== SDLK_RIGHTBRACKET&& ev.key.keysym.mod&KMOD_SHIFT) { // '}'
					XCtimeoffset(2, (unsigned int) key);
#ifdef CROPIMG
				} else if(ev.key.keysym.sym== SDLK_LEFTBRACKET) { // '['
				} else if(ev.key.keysym.sym== SDLK_RIGHTBRACKET) { // ']'
#endif
				} else if(ev.key.keysym.sym== SDLK_BACKSPACE) {
					if ((interaction_override&OVR_JCONTROL) == 0) jackt_rewind();
					remote_notify(NTY_KEYBOARD, 310, "keypress=%d # backspace", 0xff08);
				} else if(ev.key.keysym.sym== SDLK_SPACE) {
					if ((interaction_override&OVR_JCONTROL) == 0) jackt_toggle();
					remote_notify(NTY_KEYBOARD, 310, "keypress=%d # space", 0x0020);
				} else {
					remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
					//printf("SDL key event: %x\n", ev.key.keysym.sym);
				}
				break;
			case SDL_VIDEORESIZE:
				sdl_screen = SDL_SetVideoMode(ev.resize.w, ev.resize.h, 0, MYSDLFLAGS);
				sdl_rect.w=ev.resize.w;
				sdl_rect.h=ev.resize.h;
				resized_sdl();
				force_redraw=1;
				break;
			case SDL_MOUSEBUTTONUP:
				switch(ev.button.button) {
					case SDL_BUTTON_WHEELUP:
						XCresize_aspect(-1);
						break;
					case SDL_BUTTON_WHEELDOWN:
						XCresize_aspect(1);
						break;
					case SDL_BUTTON_MIDDLE:
						XCresize_aspect(0);
						break;
					default:
						break;
				}
				break;
      case SDL_ACTIVEEVENT:			/** Application loses/gains visibility */
				/* TODO disable rendering when inactive */
				break;
			case SDL_MOUSEMOTION:
				break;
			default: /* unhandled event */
				//printf("SDL EVENT: %x\n", ev.type );
				break;
		}
	}
}

#endif /* HAVE_SDL */
