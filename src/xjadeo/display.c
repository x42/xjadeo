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

#ifdef OLD_FFMPEG
#include <avcodec.h> // needed for PIX_FMT 
#include <avformat.h>
#else
#include <libavcodec/avcodec.h> // needed for PIX_FMT 
#include <libavformat/avformat.h>
#endif

extern long    ts_offset; // display on screen
extern int want_verbose;
extern int want_nosplash;

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
void mousepointer_null (int a) { ; }
void ontop_null (int a) { ; }
int  getfullscreen_null () { return (0); }
int  getontop_null () { return(0); }
void letterbox_change_null () { ; }

/* 
// experimental SSE2/MMX2 fast_memcopy 
// see mplayer's libvo/aclib.c and libvo/aclib_template.c

#define MMEMCPY
#include "fast_memcpy.c"
*/

#ifndef MMEMCPY
inline void *fast_memcpy(void * to, const void * from, size_t len) {
  memcpy(to, from, len); 
  return to;
}
#endif

/*******************************************************************************
 * strided memcopy - convert pitches of video buffer
 */

inline void stride_memcpy(void * dst, const void * src, int width, int height, int dstStride, int srcStride)
{
        int i;
        if(dstStride == srcStride)
                fast_memcpy(dst, src, srcStride*height);
        else for(i=0; i<height; i++) {
		fast_memcpy(dst, src, width);
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
extern int ST_height;

#define ST_BG ((OSD_mode&OSD_BOX)?0:1)

typedef struct {
	size_t Uoff;
	size_t Voff;
	int bpp;
} rendervars;

void _overlay_YUV422 (uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val) {
       int yoff=(2*dx+movie_width*dy*2);
       mybuffer[yoff+1]=255-(mybuffer[yoff+1]+val)/2;
       mybuffer[yoff+3]=255-(mybuffer[yoff+3]+val)/2;
}

void _overlay_YUV (uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val) {
	int yoff=(dx+movie_width*dy);
	// YUV
	mybuffer[yoff]=255-(mybuffer[yoff]+val)/2;
#if 0
	int uvoff=((dx/2)+movie_width/2*(dy/2));
	int tmp=mybuffer[rv->Voff+uvoff];
	mybuffer[rv->Uoff+uvoff]=mybuffer[rv->Voff+uvoff];
	mybuffer[rv->Voff+uvoff]=tmp;
#elif 0
	mybuffer[rv->Uoff+uvoff]=0x80;
	mybuffer[rv->Voff+uvoff]=0x80;
#endif
}

void _overlay_RGB (uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val) {
	int pos=rv->bpp*(dx+movie_width*dy);
	mybuffer[pos]= 255-(mybuffer[pos]+val)/2;
	mybuffer[pos+1]= 255-(mybuffer[pos+1]+val)/2;
	mybuffer[pos+2]= 255-(mybuffer[pos+2]+val)/2;
}

void _render_YUV422 (uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val) {
       int yoff=(2*dx+movie_width*dy*2);
       mybuffer[yoff+0]=0x80;
       mybuffer[yoff+1]=val;
       mybuffer[yoff+2]=0x80;
       mybuffer[yoff+3]=val;
}

void _render_YUV (uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val) {
	int yoff=(dx+movie_width*dy);
	int uvoff=((dx/2)+movie_width/2*(dy/2));
	// YUV
	mybuffer[yoff]=val;
	mybuffer[rv->Uoff+uvoff]=0x80;
	mybuffer[rv->Voff+uvoff]=0x80;
}

void _render_RGB (uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val) {
	int pos=rv->bpp*(dx+movie_width*dy);
	mybuffer[pos]=val;
	mybuffer[pos+1]=val;
	mybuffer[pos+2]=val;
}

/*******************************************************************************
 * colorspace utils (slow)
 */

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
 * xjadeo displays
 */


#define NULLOUTPUT &render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null, &getpos_null, &fullscreen_null, &ontop_null, &mousepointer_null, &getfullscreen_null, &getontop_null

const vidout VO[] = {
	{ PIX_FMT_RGB24,   1, 		"NULL", NULLOUTPUT}, // NULL is --vo 0 -> autodetect 
	{ PIX_FMT_YUV420P, SUP_LIBXV,	"XV - X11 video extension",
#if HAVE_LIBXV
		&render_xv, &open_window_xv, &close_window_xv,
		&handle_X_events_xv, &newsrc_xv, &resize_xv,
		&get_window_size_xv, &position_xv, get_window_pos_xv,
		&xj_set_fullscreen, &xj_set_ontop, &xj_mousepointer,
		&xj_get_fullscreen, &xj_get_ontop, &xj_letterbox},
#else
		NULLOUTPUT},
#endif
	{ PIX_FMT_YUV420P, SUP_SDL,	"SDL", 
#ifdef HAVE_SDL
		&render_sdl, &open_window_sdl, &close_window_sdl,
		&handle_X_events_sdl, &newsrc_sdl, &resize_sdl,
		&getsize_sdl, &position_sdl, &get_window_pos_sdl,
		&sdl_toggle_fullscreen, &sdl_set_ontop, &mousecursor_sdl,
		&sdl_get_fullscreen, &sdl_get_ontop, &sdl_letterbox_change},
#else
		NULLOUTPUT},
#endif
	{ PIX_FMT_RGB24,   SUP_IMLIB,   "x11 - ImLib (obsolete)",
#if HAVE_IMLIB
		&render_imlib, &open_window_imlib, &close_window_imlib,
		&handle_X_events_imlib, &newsrc_imlib, &resize_imlib,
		&get_window_size_imlib, &position_imlib, &get_window_pos_imlib,
		&xj_set_fullscreen, &xj_set_ontop, &xj_mousepointer,
		&getfullscreen_null, &getontop_null &letterbox_change_null},
#else
		NULLOUTPUT},
#endif
#ifdef IMLIB2RGBA
	{ PIX_FMT_BGRA32,   SUP_IMLIB2,   "ImLib2/x11 (RGBA32)",
#else
	{ PIX_FMT_RGB24,   SUP_IMLIB2,   "ImLib2/x11 (RGB24)",
#endif
#if HAVE_IMLIB2
		&render_imlib2, &open_window_imlib2, &close_window_imlib2,
		&handle_X_events_imlib2, &newsrc_imlib2, &resize_imlib2,
		&get_window_size_imlib2, &position_imlib2, &get_window_pos_imlib2,
		&xj_set_fullscreen, &xj_set_ontop, &xj_mousepointer,
		&getfullscreen_null, &getontop_null, &xj_letterbox},
#else
		NULLOUTPUT},
#endif
       { PIX_FMT_UYVY422,   SUP_MACOSX,   "Mac OSX - quartz",
#ifdef HAVE_MACOSX
		&render_mac, &open_window_mac, &close_window_mac,
		&handle_X_events_mac, &newsrc_mac, &resize_mac,
		&getsize_mac, &position_mac, &getpos_mac,
		&fullscreen_mac, &ontop_mac, &mousepointer_null,
		&get_fullscreen_mac, &get_ontop_mac, &mac_letterbox_change},
#else
               NULLOUTPUT},
#endif
	 { PIX_FMT_BGRA32,   SUP_OPENGL,   "OpenGL",
#ifdef HAVE_GL
		 &gl_render, &gl_open_window, & gl_close_window,
		 &gl_handle_events, &gl_newsrc,
		 &gl_resize, &gl_get_window_size,
		 &gl_position, &gl_get_window_pos,
		 &gl_set_fullscreen, &gl_set_ontop,
		 &gl_mousepointer,
		 &gl_get_fullscreen, &gl_get_ontop,
		 &gl_letterbox_change},
#else
               NULLOUTPUT},
#endif
	{-1,-1,NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL} // the end.
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

/*******************************************************************************
 *
 * OSD 
 */

#define PB_H (20)
#define PB_X (10)
#define PB_W (movie_width-2*PB_X)

#define SET_RFMT(FORMAT, POINTER, VARS, FUNC) \
	if ((FORMAT) == PIX_FMT_YUV420P) \
		(POINTER) = &_##FUNC##_YUV; \
	else if ((FORMAT) == PIX_FMT_UYVY422) \
		(POINTER) = &_##FUNC##_YUV422; \
	else if ((FORMAT) == PIX_FMT_RGB24) { \
		 (POINTER) = &_##FUNC##_RGB; \
		VARS.bpp = 3; \
	} else if ((FORMAT) == PIX_FMT_RGBA32) { \
		(POINTER) = &_##FUNC##_RGB; \
		VARS.bpp = 4; \
	} else if ((FORMAT) == PIX_FMT_BGRA32) { \
		(POINTER) = &_##FUNC##_RGB; \
		VARS.bpp = 4; \
	} else return ; 



void OSD_bitmap(int rfmt, uint8_t *mybuffer, int yperc, int xoff, int w, int h, uint8_t *src, uint8_t *mask) {

	int x,y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val);

	rv.Uoff  = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height/4; 
	rv.bpp = 0;

	xalign= (movie_width - w)/2; 
	yalign= (movie_height - h) * yperc /100.0;
	if (xalign < 0 ) xalign=0; 
	if (yalign < 0 ) yalign=0;

	SET_RFMT(rfmt,_render,rv,render)

	for (x=0; x<w && (x+xalign) < movie_width ;x++) {
		for (y=0; y<h && (y+yalign) < movie_height;y++) {
			int byte = ((y*w+x)>>3); // PIXMAP width must be mult. of 8 !
			int val = src[byte] & 1<<(x%8);
			if (!mask || mask[byte] &  1<<(x%8))
				_render(mybuffer,&rv,(x+xalign),(y+yalign),val?0xee:0x11);
		}
	}
}


void OSD_bar(int rfmt, uint8_t *mybuffer, int yperc, double min,double max,double val, double tara) {

	int x,y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val);

	rv.Uoff  = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height/4; 
	rv.bpp = 0;

	xalign=PB_X ; 
	yalign= (movie_height - PB_H) * yperc /100.0;
	int pb_val = (int) (PB_W*(val-min)/(max-min));
	int pb_not = (int) (PB_W*(tara-min)/(max-min));

	SET_RFMT(rfmt,_render,rv,overlay)

	for (x=0; x<pb_val && (x+xalign) < movie_width ;x++) {
		for (y=3; y<PB_H && (y+yalign) < movie_height;y++) {
			_render(mybuffer,&rv,(x+xalign),(y+yalign),(x%6&&y!=3)?0xf0:0x10);
		}
		if ((x%6)==5) x+=6; // bars'n'stripes
	}
	/* zero notch */
	for (x=pb_not-1; x<pb_not+2 && (x+xalign) < movie_width ;x++) {
		for (y=0; x>=0 && y<4 && (y+yalign) < movie_height;y++) 
			_render(mybuffer,&rv,(x+xalign),(y+yalign),0);
		for (y=PB_H; x>=0 && y<PB_H+4 && (y+yalign) < movie_height;y++) 
			_render(mybuffer,&rv,(x+xalign),(y+yalign),0);
	}

}


#define MIN(A,B) (((A)<(B)) ? (A) : (B))
#define MAX(A,B) (((A)>(B)) ? (A) : (B))

void OSD_render (int rfmt, uint8_t *mybuffer, char *text, int xpos, int yperc) {
	int x,y, xalign, yalign;
	rendervars rv;
	void (*_render)(uint8_t *mybuffer, rendervars *rv, int dx, int dy, int val);

	rv.Uoff  = movie_width * movie_height;
	rv.Voff = rv.Uoff + movie_width * movie_height/4; 
	rv.bpp = 0;

	SET_RFMT(rfmt,_render,rv,render)

	const int fontsize = MIN(MAX(16, movie_height/15),120);

	if ( render_font(OSD_fontfile, text, fontsize) ) return;

	if (xpos == OSD_LEFT) xalign=ST_PADDING; // left
	else if (xpos == OSD_RIGHT) xalign=movie_width-ST_PADDING-ST_rightend; // right
	else xalign=(movie_width-ST_rightend)/2; // center
	const int fh = MIN(ST_HEIGHT, ST_height);
	const int fo = ST_HEIGHT - fh;
	yalign= (movie_height - fh) * yperc /100.0;

	int donext =0;
	for (y=0; y < fh && (y+yalign) < movie_height;y++) {
		donext=0;
		for (x=0; x<ST_rightend && (x+xalign) < movie_width ;x++) {
			if (ST_image[y+fo][x]>= ST_BG || donext) {
				_render(mybuffer,&rv,(x+xalign),(y+yalign),ST_image[y+fo][x]);
			}
			if (ST_image[y+fo][x]>= ST_BG && rfmt == PIX_FMT_UYVY422) donext=1;
			else donext=0;
		}

	}
}


#include "icons/splash.bitmap"
//#include "icons/splash_mask.xbm"

void splash (uint8_t *mybuffer) {
	if (want_nosplash) return;
	//if (check main.c:stat_osd_fontfile()) 
	//	OSD_render (VO[VOutput].render_fmt, mybuffer, "Xjadeo!", OSD_CENTER, 45);
	if (movie_width >= xj_splash_height && movie_height >= xj_splash_width)
		OSD_bitmap(VO[VOutput].render_fmt, mybuffer,45,0,
			xj_splash_width, xj_splash_height, xj_splash_bits, NULL);
		//	xj_splash_mask_bits);
	render_buffer(mybuffer);
}

/*******************************************************************************
 *
 * display wrapper functions
 */

#if (defined HAVE_LIBXV || defined HAVE_IMLIB || defined HAVE_IMLIB2)
#include "icons/osd_bitmaps.h"
#endif

#define OBM(NAME,YPOS)	OSD_bitmap(VO[VOutput].render_fmt, mybuffer,YPOS,0, osd_##NAME##_width, osd_##NAME##_height, osd_##NAME##_bits, osd_##NAME##_mask_bits);

void render_buffer (uint8_t *mybuffer) {
	if (!mybuffer) return;

	// render OSD on buffer 
	if (OSD_mode&OSD_FRAME) OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_frame, OSD_fx, OSD_fy);
	if (OSD_mode&OSD_SMPTE) OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_smpte, OSD_sx, OSD_sy);

#if ( HAVE_LIBXV || HAVE_IMLIB2 )
	if (OSD_mode&OSD_EQ) {
		char tempeq[48];
		int v0,v1,v2,v3,v4;
		if(xj_get_eq("brightness",&v0)) v0=0;
		if(xj_get_eq("contrast",&v1)) v1=0;
		if(xj_get_eq("gamma",&v2)) v2=0;
		if(xj_get_eq("saturation",&v3)) v3=0;
		if(xj_get_eq("hue",&v4)) v4=0;
		if (0) {
			snprintf(tempeq,48,"B:%i C:%i S:%i H:%i G:%i", v0/10, v1/10, v2/10, v3/10, v4/10);
		 	OSD_render (VO[VOutput].render_fmt, mybuffer, tempeq, OSD_CENTER, 50);
		} else {
		#if 1
			OBM(brightness, 3)
			OBM(contrast, 23)
			OBM(gamma, 43)
			OBM(saturation, 63)
			OBM(hue, 83)
		#else
			OSD_render (VO[VOutput].render_fmt, mybuffer, "Brigtness:", OSD_CENTER, 3);
			OSD_render (VO[VOutput].render_fmt, mybuffer, "Contrast:", OSD_CENTER, 23);
			OSD_render (VO[VOutput].render_fmt, mybuffer, "Gamma:", OSD_CENTER, 43);
			OSD_render (VO[VOutput].render_fmt, mybuffer, "Saturation:", OSD_CENTER, 63);
			OSD_render (VO[VOutput].render_fmt, mybuffer, "Hue:", OSD_CENTER, 83);
		#endif
			OSD_bar(VO[VOutput].render_fmt, mybuffer,10, -1000.0,1000.0,(double) v0, 0.0);
			OSD_bar(VO[VOutput].render_fmt, mybuffer,30, -1000.0,1000.0,(double) v1, (VOutput==1)?-500.0:0.0);
			OSD_bar(VO[VOutput].render_fmt, mybuffer,50, -1000.0,1000.0,(double) v2, 0.0);
			OSD_bar(VO[VOutput].render_fmt, mybuffer,70, -1000.0,1000.0,(double) v3, 0.0);
			OSD_bar(VO[VOutput].render_fmt, mybuffer,90, -1000.0,1000.0,(double) v4, 0.0);
		}

	} else 
#endif
	{ 
		if (OSD_mode&OSD_TEXT )
			OSD_render (VO[VOutput].render_fmt, mybuffer, OSD_text, OSD_tx, OSD_ty);

		if (OSD_mode&OSD_OFFF ) {
			char tempoff[30];
			snprintf(tempoff,30,"off: %li",ts_offset);
			OSD_render (VO[VOutput].render_fmt, mybuffer, tempoff, OSD_CENTER, 50);
		} else if (OSD_mode&OSD_OFFS ) { 
			char tempsmpte[30];
			sprintf(tempsmpte,"off: ");
			frame_to_smptestring(tempsmpte+4,ts_offset);
			OSD_render (VO[VOutput].render_fmt, mybuffer, tempsmpte, OSD_CENTER, 50);
		}
	}
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

int Xgetontop (void) {
	return (VO[VOutput].getontop());
}

int Xgetfullscreen (void) {
	return (VO[VOutput].getfullscreen());
}

void Xmousepointer (int a) {
	VO[VOutput].mousepointer(a);
}

#if (defined HAVE_LIBXV || defined HAVE_IMLIB || defined HAVE_IMLIB2 || defined HAVE_MACOSX || defined HAVE_SDL)
extern int force_redraw; // tell the main event loop that some cfg has changed
#endif

void Xletterbox (int action) {
	if (action==2) want_letterbox=!want_letterbox;
	else want_letterbox = action?1:0;
	VO[VOutput].letterbox_change();
}

void Xfullscreen (int a) {
	VO[VOutput].fullscreen(a);
}

void Xposition (int x, int y) {
	VO[VOutput].position(x,y);
}
