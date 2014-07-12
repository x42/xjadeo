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
 */

#include "xjadeo.h"
#include "display.h"

extern long ts_offset; 
extern char    *smpte_offset;
extern int 	force_redraw; // tell the main event loop that some cfg has changed
extern int 	interaction_override; // disable some options.
extern double framerate;

void jackt_toggle();
void jackt_rewind();

void calc_letterbox(int src_w, int src_h, int out_w, int out_h, int *sca_w, int *sca_h);
void resized_sdl ();

/*******************************************************************************
 * SDL
 */

#ifdef HAVE_SDL

#define MYSDLFLAGS (SDL_HWSURFACE | SDL_RESIZABLE | SDL_DOUBLEBUF)

  SDL_Surface* sdl_screen;
  SDL_Overlay *sdl_overlay;
  SDL_Rect sdl_rect;
  SDL_Rect sdl_dest_rect;
  int sdl_pic_format= SDL_YV12_OVERLAY; // fourcc

	int full_screen_width = 1024;
	int full_screen_height = 768;

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
#if 0  // verify YUV alignment
	if ((sdl_overlay->pixels[1] - sdl_overlay->pixels[0]) != ( movie_width * movie_height) ||
			(sdl_overlay->pixels[2] - sdl_overlay->pixels[1]) != ( movie_width * movie_height /4) ) {
		printf("unsupported SDL YV12 pixel buffer alignment.\n"); 
		goto no_overlay;
	}  
#endif
	/*
	SDL_Surface* icon = SDL_LoadBMP(iconName));
	SDL_WM_SetIcon(icon, NULL);
	*/

	return(0);
 
no_overlay: 
	if(sdl_overlay) SDL_FreeYUVOverlay(sdl_overlay); 
	SDL_Quit();
no_sdl:
	return 1;
}

void black_border_sdl(SDL_Rect b) {
	//printf(" bb: +%i+%i %i %i\n", b.x, b.y, b.w, b.h);
	SDL_FillRect(sdl_screen, &b, SDL_MapRGB(sdl_screen->format, 0,0,0));
	SDL_UpdateRect(sdl_screen, b.x, b.y, b.w, b.h);
}

void resized_sdl () {
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
#if 0
	SDL_FillRect(sdl_screen, &sdl_rect, SDL_MapRGB(sdl_screen->format, 0,0,0));
	SDL_UpdateRect(sdl_screen, sdl_rect.x, sdl_rect.y, sdl_rect.w, sdl_rect.h);
#else
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
#endif
}

void mousecursor_sdl(int action) {
  static int sdl_mouse = 1;
	if (action==2) sdl_mouse^=1;
	else sdl_mouse=action?1:0;
	SDL_ShowCursor(sdl_mouse);
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

void get_window_pos_sdl (unsigned int *x, unsigned int *y) {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!( SDL_GetWMInfo(&info) > 0 )) { return; }
#ifdef HAVE_WINDOWS
	WINDOWINFO w;
	GetWindowInfo(info.window, &w);
	*x= w.rcWindow.left;
	*y= w.rcWindow.top;
	//*w= w.rcWindow.right - w.rcWindow.left;
	//*h= w.rcWindow.bottom - w.rcWindow.top;
	printf("%ld - %ld\n", w.rcWindow.left, w.rcWindow.top);
#endif
#if (defined HAVE_LIBXV || defined HAVE_IMLIB || defined HAVE_IMLIB2)
	if ( info.subsystem == SDL_SYSWM_X11 ) {
		Window	dummy;
		int xx,xy;
		info.info.x11.lock_func();
		XTranslateCoordinates(info.info.x11.display, info.info.x11.wmwindow, info.info.x11.fswindow, 0, 0, &xx, &xy, &dummy);
		// TODO recurse until dummy!=None
		info.info.x11.unlock_func();
		*x= xx;
		*y= xy;
	}
#endif
}

void position_sdl(int x, int y) {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) > 0 ) {
#ifdef HAVE_WINDOWS
	SetWindowPos(info.window, NULL, x, y, sdl_rect.w, sdl_rect.h, 0);
#endif
#if (defined HAVE_LIBXV || defined HAVE_IMLIB || defined HAVE_IMLIB2)
	if ( info.subsystem == SDL_SYSWM_X11 ) {
			info.info.x11.lock_func();
#if 0 /* get root window size  -> center window  */
			int x, y;
			int w, h;
			w = DisplayWidth(info.info.x11.display,
			DefaultScreen(info.info.x11.display));

			h = DisplayHeight(info.info.x11.display,
			DefaultScreen(info.info.x11.display));
			x = (w - screen->w)/2;
			y = (h - screen->h)/2;
#endif
			XMoveWindow(info.info.x11.display, info.info.x11.wmwindow, x, y);
			info.info.x11.unlock_func();
		} 
#endif
	} 
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

int sdl_full_screen =0;
SDL_Rect sdl_oldsize;

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

void calc_letterbox(int src_w, int src_h, int out_w, int out_h, int *sca_w, int *sca_h) {
	const float asp_src = movie_aspect?movie_aspect:(float)src_w/src_h;
  if (asp_src * out_h > out_w) {
    (*sca_w)=out_w;
    (*sca_h)=(int)round((float)out_w/asp_src);
	} else {
    (*sca_h)=out_h;
    (*sca_w)=(int)round((float)out_h*asp_src);
	}
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
			//	printf("SDL render event.\n");
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
				} else if(ev.key.keysym.sym==SDLK_q) {
					if ((interaction_override&OVR_QUIT_KEY) == 0) {
						loop_flag=0;
					} else {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
					}
				} else if(ev.key.keysym.sym==SDLK_s) {
					OSD_mode^=OSD_SMPTE;
					force_redraw=1;
				} else if(ev.key.keysym.sym==SDLK_a) {
					// TODO always on top
				} else if(ev.key.keysym.sym==SDLK_f) {
					sdl_toggle_fullscreen(2);
				} else if(ev.key.keysym.sym==SDLK_l) {
						want_letterbox=!want_letterbox; 
						resized_sdl();
						force_redraw=1;
				} else if(ev.key.keysym.sym==SDLK_m) { 
					mousecursor_sdl(2);
				} else if(ev.key.keysym.sym==SDLK_s) {
					OSD_mode^=OSD_SMPTE;
					force_redraw=1;
				} else if(ev.key.keysym.sym==SDLK_v) {
					OSD_mode^=OSD_FRAME; 
					force_redraw=1;
				} else if(ev.key.keysym.sym==SDLK_b) {
						OSD_mode^=OSD_BOX;
						force_redraw=1;
				} else if(ev.key.keysym.sym== SDLK_c && ev.key.keysym.mod&KMOD_SHIFT) {
					OSD_mode=0; 
					force_redraw=1;
				} else if(ev.key.keysym.sym== SDLK_LESS || (ev.key.keysym.sym== SDLK_COMMA && ev.key.keysym.mod&KMOD_SHIFT) ) { // '<'
					unsigned int my_Width,my_Height;
					getsize_sdl(&my_Width,&my_Height);
					float step=0.2*my_Height;
					my_Width-=floor(step*movie_aspect);
					my_Height-=step;
					resize_sdl(my_Width, my_Height);
				} else if(ev.key.keysym.sym== SDLK_GREATER || (ev.key.keysym.sym== SDLK_PERIOD && ev.key.keysym.mod&KMOD_SHIFT) ) { // '>'
					unsigned int my_Width,my_Height;
					getsize_sdl(&my_Width,&my_Height);
					float step=0.2*my_Height;
					my_Width+=floor(step*movie_aspect);
					my_Height+=step;
					resize_sdl(my_Width, my_Height);
				} else if(ev.key.keysym.sym==SDLK_PERIOD) {
					resize_sdl(ffctv_width, ffctv_height);
				} else if(ev.key.keysym.sym== SDLK_COMMA) { // ','
						unsigned int my_Width,my_Height;
						getsize_sdl(&my_Width,&my_Height);
						if( movie_aspect < ((float)my_Width/(float)my_Height) )
							my_Width=rint((float)my_Height * movie_aspect);
						else 	my_Height=rint((float)my_Width / movie_aspect);
						resize_sdl(my_Width, my_Height);
				} else if(ev.key.keysym.sym==SDLK_o) {
					if (OSD_mode&OSD_OFFF) {
						OSD_mode&=~OSD_OFFF;
						OSD_mode|=OSD_OFFS;
					} else if (OSD_mode&OSD_OFFS) {
						OSD_mode^=OSD_OFFS;
					} else {
						OSD_mode^=OSD_OFFF;
					}
					force_redraw=1;
				} else if(ev.key.keysym.sym== SDLK_EQUALS && ev.key.keysym.mod&KMOD_SHIFT) { // '+' SDLK_PLUS does not work :/
					if ((interaction_override&OVR_AVOFFSET) != 0 ) {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
						break;
					}
					ts_offset++;
					force_redraw=1;
					if (smpte_offset) free(smpte_offset);
					smpte_offset= calloc(15,sizeof(char));
					frame_to_smptestring(smpte_offset,ts_offset);
				} else if(ev.key.keysym.sym==SDLK_MINUS) {
					if ((interaction_override&OVR_AVOFFSET) != 0 ) {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
						break;
					}
					ts_offset--;
					force_redraw=1;
					if (smpte_offset) free(smpte_offset);
					smpte_offset= calloc(15,sizeof(char));
					frame_to_smptestring(smpte_offset,ts_offset);
				} else if(ev.key.keysym.sym== SDLK_LEFTBRACKET && ev.key.keysym.mod&KMOD_SHIFT) { // '{'
					if ((interaction_override&OVR_AVOFFSET) != 0 ) {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
						break;
					}
					if (framerate > 0) {
						ts_offset-= framerate *60;
					} else {
						ts_offset-= 25*60;
					}
					force_redraw=1;
					if (smpte_offset) free(smpte_offset);
					smpte_offset= calloc(15,sizeof(char));
					frame_to_smptestring(smpte_offset,ts_offset);
				} else if(ev.key.keysym.sym== SDLK_RIGHTBRACKET&& ev.key.keysym.mod&KMOD_SHIFT) { // '}'
					if ((interaction_override&OVR_AVOFFSET) != 0 ) {
						remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) key);
						break;
					}
					if (framerate > 0) {
						ts_offset+= framerate *60;
					} else {
						ts_offset+= 25*60;
					}
					force_redraw=1;
					if (smpte_offset) free(smpte_offset);
					smpte_offset= calloc(15,sizeof(char));
					frame_to_smptestring(smpte_offset,ts_offset);
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
				if(ev.button.button == SDL_BUTTON_LEFT) {
					resize_sdl(ffctv_width, ffctv_height);
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
						my_Width-=floor(step*movie_aspect);
						my_Height-=step;
					}
					if(ev.button.button == SDL_BUTTON_WHEELDOWN) {
						float step=sqrt((float)my_Height);
						my_Width+=floor(step*movie_aspect);
						my_Height+=step;
					} 
					// resize to match movie aspect ratio
					if( movie_aspect < ((float)my_Width/(float)my_Height) )
						my_Width=floor((float)my_Height * movie_aspect);
					else my_Height=floor((float)my_Width / movie_aspect);

					resize_sdl(my_Width,my_Height);
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
