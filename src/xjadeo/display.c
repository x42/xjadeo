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
 */
#include "xjadeo.h"
#include "display.h"

extern int want_verbose;

/*******************************************************************************
 *
 * NULL Video Output 
 */ 

int open_window_null (void) { return (1); }
void close_window_null (void) { ; }
void render_null (uint8_t *mybuffer) { ; }
void handle_X_events_null (void) { ; }
void newsrc_null (void) { ; }
void resize_null (unsigned int x, unsigned int y) { ; }
void getsize_null (unsigned int *x, unsigned int *y) { if(x)*x=1; if(y)*y=1; }
void position_null (int x, int y) { ; }
void getpos_null (int *x, int *y) { if(x)*x=1; if(y)*y=1; }
void fullscreen_null (int a) { ; }
void ontop_null (int a) { ; }

/*******************************************************************************
 * strided memcopy - convert pitches of video buffer
 */

inline void stride_memcpy(void * dst, const void * src, int width, int height, int dstStride, int srcStride)
{
        int i;
        if(dstStride == srcStride)
                memcpy(dst, src, srcStride*height);
        else for(i=0; i<height; i++) {
		memcpy(dst, src, width);
		src+= srcStride;
		dst+= dstStride;
        }
}


/*******************************************************************************
 *
 * SubTitle Render - On Screen Display
 *
 * overwrites the given buffer by replacing 
 * the pixel values!
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
		for (y=0; y<ST_HEIGHT && (y+yalign) < movie_height;y++) {
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

void OSD_renderRGB (int bpp, uint8_t *mybuffer, char *text, int xpos, int yperc) {
	int x,y, xalign, yalign;

	if ( render_font(OSD_fontfile, text) ) return;

	if (xpos == OSD_LEFT) xalign=ST_PADDING; // left
	else if (xpos == OSD_RIGHT) xalign=movie_width-ST_PADDING-ST_rightend; // right
	else xalign=(movie_width-ST_rightend)/2; // center

	yalign= (movie_height - ST_HEIGHT) * yperc /100.0;

	for (x=0; x<ST_rightend && (x+xalign) < movie_width ;x++)
		for (y=0; y<ST_HEIGHT && (y+yalign) < movie_height;y++) {
			int dx=(x+xalign);
			int dy=(y+yalign);
			int pos=bpp*(dx+movie_width*dy);

			if (ST_image[y][x]>= ST_BG){
				mybuffer[pos]=ST_image[y][x];
				mybuffer[pos+1]=ST_image[y][x];
				mybuffer[pos+2]=ST_image[y][x];
			}
	}
}

void rgb2argb (uint8_t *rgbabuffer, uint8_t *rgbbuffer, int width, int height) {
	int x,y,p3,p4;
	for (x=0; x< width ;x++)
		for (y=0; y< height;y++) {
			p3=3*(x+movie_width*y);
			p4=4*(x+movie_width*y);
			rgbabuffer[p4+0] = 255;
			rgbabuffer[p4+1] = rgbbuffer[p3];
			rgbabuffer[p4+2] = rgbbuffer[p3+1];
			rgbabuffer[p4+3] = rgbbuffer[p3+2];
	}
}

void rgb2abgr (uint8_t *rgbabuffer, uint8_t *rgbbuffer, int width, int height) {
	int x,y,p3,p4;
	for (x=0; x< width ;x++)
		for (y=0; y< height;y++) {
			p3=3*(x+movie_width*y);
			p4=4*(x+movie_width*y);
			rgbabuffer[p4+3] = 255;
			rgbabuffer[p4+2] = rgbbuffer[p3];
			rgbabuffer[p4+1] = rgbbuffer[p3+1];
			rgbabuffer[p4+0] = rgbbuffer[p3+2];
	}
}

/*******************************************************************************
 *
 * xjadeo fn
 */


#include <ffmpeg/avcodec.h> // needed for PIX_FMT 
#include <ffmpeg/avformat.h>

#define NULLOUTPUT &render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null, &getpos_null, &fullscreen_null, &ontop_null

const vidout VO[] = {
	{ PIX_FMT_RGB24,   1, 		"NULL", NULLOUTPUT}, // NULL is --vo 0 -> autodetect 
	{ PIX_FMT_YUV420P, SUP_LIBXV,	"XV - X11 video extension",
#if HAVE_LIBXV
		&render_xv, &open_window_xv, &close_window_xv,
		&handle_X_events_xv, &newsrc_xv, &resize_xv,
		&get_window_size_xv, &position_xv, get_window_pos_xv,
		&xj_set_fullscreen, &xj_set_ontop},
#else
		NULLOUTPUT},
#endif
	{ PIX_FMT_YUV420P, SUP_SDL,	"SDL", 
#if HAVE_SDL
		&render_sdl, &open_window_sdl, &close_window_sdl,
		&handle_X_events_sdl, &newsrc_sdl, &resize_sdl,
		&getsize_sdl, &position_sdl, &getpos_null,
		&fullscreen_null, &ontop_null},
#else
		NULLOUTPUT},
#endif
	{ PIX_FMT_RGB24,   SUP_IMLIB,   "x11 - ImLib",
#if HAVE_IMLIB
		&render_imlib, &open_window_imlib, &close_window_imlib,
		&handle_X_events_imlib, &newsrc_imlib, &resize_imlib,
		&get_window_size_imlib, &position_imlib, &get_window_pos_imlib,
		&xj_set_fullscreen, &xj_set_ontop},
#else
		NULLOUTPUT},
#endif
#ifdef IMLIB2RGBA
	{ PIX_FMT_RGBA32,   SUP_IMLIB2,   "ImLib2/x11 (RGBA32)",
#else
	{ PIX_FMT_RGB24,   SUP_IMLIB2,   "ImLib2/x11 (RGB24)",
#endif
#if HAVE_IMLIB2
		&render_imlib2, &open_window_imlib2, &close_window_imlib2,
		&handle_X_events_imlib2, &newsrc_imlib2, &resize_imlib2,
		&get_window_size_imlib2, &position_imlib2, &get_window_pos_imlib2,
		&xj_set_fullscreen, &xj_set_ontop},
#else
		NULLOUTPUT},
#endif
	{-1,-1,NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL} // the end.
};

int VOutput = 0;


int parsevidoutname (char *arg) {
	int i=0;
	int s0=strlen(arg);
	if (s0==0) return (0);

	if (!strncasecmp("list",arg,s0>4?4:s0)) return(-1);

	while (VO[++i].supported>=0) {
		int s1=strlen(VO[i].name);
		if (!strncasecmp(VO[i].name,arg,s0>s1?s1:s0)) return(i);
	}
	return(0);
}

const char * vidoutname (int i) {
	return (VO[i].name);
}

int vidoutsupported (int i) {
	return(VO[i].supported);
}

void dump_vopts (void) {
	int i=0;
	fprintf (stdout, "Video Output Modes: \n");
	fprintf (stdout, " --vo 0 # autodetect (default)\n");

	while (VO[++i].supported>=0) {
		fprintf (stdout, " --vo %i # %s %s\n",i,VO[i].name, 
				VO[i].supported?"(supported by this xjadeo)":"(NOT compiled in this xjadeo)");
	}
}

int try_next_vidoutmode(int user_req) {
	int i=0;
	// check available modes..
	while (VO[++i].supported>=0);

	if (user_req >= i || user_req < 0 ) return (-1);
	if (user_req < i && user_req > 0 && VO[user_req].supported) return(1);
	return (0);
}

int vidoutmode(int user_req) {
	int i=0;
	if (user_req < 0) {
		dump_vopts();
		exit (0);
	}

	VOutput=0;

	// check available modes..
	while (VO[++i].supported>=0) {
		if (VO[i].supported && VOutput==0) {
			VOutput=i;
		}
	}

	if (user_req < i && user_req>0 )
		if (VO[user_req].supported) VOutput=user_req;

	return VO[VOutput].render_fmt;
}

void OSD_render (uint8_t *mybuffer, char *text, int xpos, int yperc) {
	if (VO[VOutput].render_fmt == PIX_FMT_YUV420P) OSD_renderYUV (mybuffer, text, xpos, yperc);
	else if (VO[VOutput].render_fmt == PIX_FMT_RGB24) OSD_renderRGB (3,mybuffer, text, xpos, yperc);
	else if (VO[VOutput].render_fmt == PIX_FMT_RGBA32) OSD_renderRGB (4,mybuffer, text, xpos, yperc);
}

void render_buffer (uint8_t *mybuffer) {
	if (!mybuffer) return;

	// render OSD on buffer 
	if (OSD_mode&OSD_FRAME) OSD_render (mybuffer, OSD_frame, OSD_fx, OSD_fy);
	if (OSD_mode&OSD_SMPTE) OSD_render (mybuffer, OSD_smpte, OSD_sx, OSD_sy);
	if (OSD_mode&OSD_TEXT ) OSD_render (mybuffer, OSD_text, OSD_tx, OSD_ty);
	VO[VOutput].render(buffer); // buffer = mybuffer (so far no share mem or sth)
}


void open_window(void) {
	if (!want_quiet)
		printf("Video output: %s\n",VO[VOutput].name);
	if (VO[VOutput].open() ) { 
		fprintf(stderr,"Could not open video output.\n");
		VOutput=0;
		loop_run=0;
	} else loop_run=1;
}

void close_window(void) {
	int vmode=VOutput;
	VOutput=0;
	loop_run=0;
	VO[vmode].close();
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

void Xgetpos (int *x, int *y) {
	VO[VOutput].getpos(x,y);
}

void Xgetsize (unsigned int *x, unsigned int *y) {
	VO[VOutput].getsize(x,y);
}

void Xresize (unsigned int x, unsigned int y) {
	VO[VOutput].resize(x,y);
}

void Xontop (int a) {
	VO[VOutput].ontop(a);
}

void Xfullscreen (int a) {
	VO[VOutput].fullscreen(a);
}

void Xposition (int x, int y) {
	printf("set window position to %i %i\n",x,y);
	VO[VOutput].position(x,y);
}
