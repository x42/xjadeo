#include "xjadeo.h"
#include "display.h"

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

// make sure the video modes are numbered the same on every system,
// until we switch to named --vo :)
const vidout VO[] = {
	{ PIX_FMT_RGB24,   1, 		"NULL", &render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null}, // NULL is --vo 0 -> autodetect 

	{ PIX_FMT_RGB24,   SUP_GTK,	"GTK",
#if HAVE_MYGTK
		&render_gtk, &open_window_gtk, &close_window_gtk, &handle_X_events_gtk, &newsrc_null, &resize_gtk, &getsize_gtk, &position_null},
#else
		&render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null},
#endif
	{ PIX_FMT_YUV420P, SUP_SDL,	"SDL", 
#if HAVE_SDL
		&render_sdl, &open_window_sdl, &close_window_sdl, &handle_X_events_sdl, &newsrc_sdl, &resize_sdl, &getsize_sdl, &position_sdl},
#else
		&render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null},
#endif
	{ PIX_FMT_RGB24,   SUP_IMLIB,   "x11 - ImLib",
#if HAVE_IMLIB
		&render_imlib, &open_window_imlib, &close_window_imlib, &handle_X_events_imlib, &newsrc_imlib, &resize_imlib, &get_window_size_imlib, &position_imlib},
#else
		&render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null},
#endif
	{ PIX_FMT_YUV420P, SUP_LIBXV,	"x11 - XV",
#if HAVE_XV
		&render_xv, &open_window_xv, &close_window_xv, &handle_X_events_xv, &newsrc_xv, &resize_xv, &get_window_size_xv, &position_xv},
#else
		&render_null, &open_window_null, &close_window_null, &handle_X_events_null, &newsrc_null, &resize_null, &getsize_null, &position_null},
#endif
	{-1,-1,NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL} // the end.
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
