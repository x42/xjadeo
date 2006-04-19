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
 */

#include "xjadeo.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include <jack/jack.h>
#include <jack/transport.h>

#include <time.h>
#include <getopt.h>

#include <unistd.h>


//------------------------------------------------
// extern Globals (main.c)
//------------------------------------------------
extern int       loop_flag;

extern int               movie_width;
extern int               movie_height;
extern AVFrame           *pFrameFMT;
extern uint8_t           *buffer;

// needs to be set before calling movie_open
extern int               render_fmt;

/* Video File Info */
extern double 	duration;
extern double 	framerate;
extern long	frames;

/* Option flags and variables */
extern char *current_file;
extern long	ts_offset;
extern long	userFrame;
extern long	dispFrame;
extern int want_quiet;
extern int want_verbose;
extern int remote_en;
extern int remote_mode;

#ifdef HAVE_MIDI
extern int midi_clkconvert;
#endif

extern double 		delay;
extern int		videomode;
extern int 		seekflags;

// On screen display
extern char OSD_fontfile[1024]; 
extern char OSD_text[128];
extern int OSD_mode;

extern int OSD_fx, OSD_tx, OSD_sx, OSD_fy, OSD_sy, OSD_ty;

extern jack_client_t *jack_client;
extern char jackid[16];


// TODO: someday we can implement 'mkfifo' or open / pipe
#define REMOTE_RX fileno(stdin) 
#define REMOTE_TX fileno(stdout)

#define REMOTEBUFSIZ 4096
int remote_read(void);
void remote_printf(int val, const char *format, ...);

//--------------------------------------------
// API commands
//--------------------------------------------

/* replace current movie file
 *
 * argument d needs to be a pointer to a char array 
 * with the data 'load <filename>\0'
 */
void xapi_open(void *d) {
	char *fn= (char*)d;
	printf("open file: '%s'\n",fn);
	if ( open_movie(fn)) 
		remote_printf(403, "failed to open file '%s'",fn);
	else	remote_printf(100, "opened file: '%s'",fn);
        init_moviebuffer();
	newsourcebuffer();
	display_frame((int64_t)(dispFrame),1); // update screen
}

void xapi_close(void *d) {
	if (!close_movie()) {
		remote_printf(100, "closed video buffer.");
		init_moviebuffer();
		newsourcebuffer();
	}
	else	remote_printf(404, "no video buffer to close.");
}

void xapi_close_window(void *d) {
	close_window();
	remote_printf(100, "window closed.");
}

void xapi_set_videomode(void *d) {
	int vmode= atoi((char*)d);
	if (getvidmode() !=0) {
		remote_printf(413, "cannot change videomode while window is open.");
		return;
	}
	if (vmode <0) {
		remote_printf(414, "video mode needs to be a positive integer or 0 for autodetect.");
		return;
	}
	remote_printf(100, "setting video mode to %i",vmode);

	render_fmt = vidoutmode(vmode);
	open_window(0,NULL); // required here; else VOutout callback fn will fail.

	if (pFrameFMT && current_file) { 
		// reinit buffer with new format
		char *fn=strdup(current_file);
		open_movie(fn);
		free(fn);
	} else {
	  if(buffer) free(buffer);
	}
	init_moviebuffer();
}

void xapi_open_window(void *d) {
	if (getvidmode() !=0) {
		remote_printf(412, "window already open.");
		return;
	}
	vidoutmode(videomode); // init VOutput
	open_window(0,NULL);
	remote_printf(100, "window opened.");
}

void xapi_pvideomode(void *d) {
	remote_printf(200,"videomode=%i", getvidmode());
}

void xapi_pwinpos(void *d) {
	int x,y;
	Xgetpos(&x,&y); 
	remote_printf(200,"windowpos=%ix%i",x,y);
}

void xapi_pwinsize(void *d) {
	unsigned int x,y;
	Xgetsize(&x,&y); 
	remote_printf(200,"windowsize=%ux%u",x,y);
}

void xapi_swinsize(void *d) {
	unsigned int x,y;
	char *size= (char*)d;
	char *tmp;
	x=movie_width;y=movie_height;
	
	if ((tmp=strchr(size,'x')) && ++tmp) {
		x=atoi(size);
		y=atoi(tmp);
	} else {
		int percent=atoi(size);
		if (percent > 0 && percent <= 500) {
			x*= percent; x/=100;
			y*= percent; y/=100;
		}
	}
			

	remote_printf(100,"resizing window to %ux%u",x,y);
	Xresize(x,y);
}

void xapi_swinpos(void *d) {
	int x,y;
	char *t0= (char*)d;
	char *t1;
	x=0;y=0;
	
	if ((t1=strchr(t0,'x')) && ++t1) {
		x=atoi(t0);
		y=atoi(t1);

		remote_printf(100,"positioning window to %ix%i",x,y);
		Xposition(x,y);
	}  else {
		remote_printf(421,"invalid position argument (example 200x100)");
	}
}

void xapi_quit(void *d) {
	remote_printf(100,"quit.");
	loop_flag=0;
}

void xapi_pfilename(void *d) {
	if (current_file) 
		remote_printf(200, "filename=%s", current_file);
	else 
		remote_printf(410, "no open video file");
}

void xapi_pduration(void *d) {
	remote_printf(200, "duration=%g", duration);
}

void xapi_pframerate(void *d) {
	remote_printf(200, "framerate=%g", framerate);
}

void xapi_pframes(void *d) {
	remote_printf(200, "frames=%ld ", frames);
}

void xapi_poffset(void *d) {
	remote_printf(200,"offset=%li",(long int) ts_offset);
}

void xapi_pseekmode (void *d) {
	remote_printf(200,"seekmode=%i", seekflags==AVSEEK_FLAG_BACKWARD?1:0);
}

void xapi_sseekmode (void *d) {
	char *mode= (char*)d;
#if LIBAVFORMAT_BUILD > 4622
	seekflags    = AVSEEK_FLAG_ANY; /* non keyframe */
#else
	seekflags    = AVSEEK_FLAG_BACKWARD; /* keyframe */
#endif
	if (!strcmp(mode,"key") || atoi(mode)==1)
		seekflags=AVSEEK_FLAG_BACKWARD;
	remote_printf(200,"seekmode=%i", seekflags==AVSEEK_FLAG_BACKWARD?1:0);
}

void xapi_pmwidth(void *d) {
	remote_printf(200,"movie_width=%i", movie_width);
}

void xapi_pmheight(void *d) {
	remote_printf(200,"movie_height=%i", movie_height);
}
void xapi_soffset(void *d) {
//	long int new = atol((char*)d);
	long int new = smptestring_to_frame((char*)d);
	ts_offset= (int64_t) new;
	remote_printf(100,"offset=%li",(long int) ts_offset);
}

void xapi_pposition(void *d) {
	remote_printf(200,"position=%li",dispFrame);
}

void xapi_psmpte(void *d) {
	char smptestr[13];
	frame_to_smptestring(smptestr,dispFrame);
	remote_printf(200,"smpte=%s",smptestr);
}

void xapi_seek(void *d) {
//	long int new = atol((char*)d);
	long int new = smptestring_to_frame((char*)d);
	userFrame= (int64_t) new;
	remote_printf(100,"defaultseek=%li",userFrame);
}

void xapi_pfps(void *d) {
	remote_printf(200,"updatefps=%i",(int) rint(1/delay));
}

void xapi_sfps(void *d) {
	char *off= (char*)d;
        delay = 1.0 / atof(off);
	remote_printf(100,"updatefps=%i",(int) rint(1/delay));
}

void xapi_jack_status(void *d) {
	if (jack_client) 
		remote_printf(200,"jackclient='%s'.",jackid);
	else 
		remote_printf(100,"not connected to jack server");

}

void xapi_open_jack(void *d) {
	open_jack();
	if (jack_client) 
		remote_printf(100,"connected to jack server.");
	else 
		remote_printf(405,"failed to connect to jack server");
}

void xapi_close_jack(void *d) {
	close_jack();
	remote_printf(100,"closed jack connection");
}


void xapi_osd_smpte(void *d) {
	int y = atoi((char*)d);
	if (y<0){
		OSD_mode&=~OSD_SMPTE;
		remote_printf(100,"hiding smpte OSD");
	} else if (y<=100) {
		OSD_mode|=OSD_SMPTE;
		OSD_sy=y;
		remote_printf(100,"rendering smpte on position y:%i%%",y);
	} else 
		remote_printf(422,"invalid argument (range -1..100)");
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_frame(void *d) {
	int y = atoi((char*)d);
	if (y<0){
		OSD_mode&=~OSD_FRAME;
		remote_printf(100,"hiding frame OSD");
	} else if (y<=100) {
		OSD_mode|=OSD_FRAME;
		OSD_fy=y;
		remote_printf(100,"rendering frame on position y:%i%%",y);
	} else 
		remote_printf(422,"invalid argument (range -1..100)");
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_off(void *d) {
	OSD_mode&=~OSD_TEXT;
	remote_printf(100,"hiding OSD");
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_on(void *d) {
	OSD_mode|=OSD_TEXT;
	remote_printf(100,"rendering OSD");
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_text(void *d) {
	snprintf(OSD_text,128,"%s",(char*)d);
	xapi_osd_on(NULL);
}

void xapi_osd_font(void *d) {
	snprintf(OSD_fontfile,1024,"%s",(char*)d);
	xapi_osd_on(NULL);
}

void xapi_osd_nobox(void *d) {
	OSD_mode&=~OSD_BOX;
	remote_printf(100,"OSD transparent background");
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_box(void *d) {
	OSD_mode|=OSD_BOX;
	remote_printf(100,"OSD black box background");
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_avail(void *d) {
#ifdef HAVE_FT
	remote_printf(100,"rendering OSD is supported");
#else
	remote_printf(490,"this feature is not compiled");
#endif
}

void xapi_osd_pos(void *d) {
	int x,y;
	char *t0= (char*)d;
	char *t1;
	x=0;y=0;
	
	if ((t1=strchr(t0,' ')) && ++t1) {
		OSD_tx=atoi(t0);
		OSD_ty=atoi(t1);
		if (OSD_tx > OSD_RIGHT) OSD_tx=OSD_RIGHT;
		if (OSD_tx < OSD_LEFT) OSD_tx=OSD_LEFT;
		if (OSD_ty > 100) OSD_ty=100;
		if (OSD_ty < 0) OSD_ty=0;

		remote_printf(100,"realigning OSD");
	}  else {
		remote_printf(421,"invalid  argument (example 1 95)");
	}
	display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_midi_status(void *d) {
#ifdef HAVE_MIDI
	// FIXME: we shall return "200,midiid=XXX" 
	// and "100 not connected" ?? as in jack connected
	// BUT: // midiid can be <int> (portmidi) or string (alsa midi)
	if (midi_connected())
		remote_printf(100,"midi connected.");
	else
		remote_printf(101,"midi not connected.");
#else
	remote_printf(499,"midi not available.");
#endif
}
void xapi_open_midi(void *d) {
#ifdef HAVE_MIDI
	midi_open(d);
	if (midi_connected())
		remote_printf(100,"MIDI connected.");
	else
		remote_printf(440,"MIDI open failed.");
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_close_midi(void *d) {
#ifdef HAVE_MIDI
	midi_close();
	remote_printf(100,"midi close.");
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_detect_midi(void *d) {
#ifdef HAVE_MIDI
	midi_open("-1");
	if (midi_connected())
		remote_printf(100,"MIDI connected.");
	else
		remote_printf(440,"MIDI open failed.");
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_pmidisync(void *d) {
#ifdef HAVE_MIDI
	remote_printf(200,"midisync=%i", midi_clkconvert);
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_smidisync(void *d) {
#ifdef HAVE_MIDI
        midi_clkconvert = atoi((char*)d);
	remote_printf(100,"midisync=%i", midi_clkconvert);
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_bidir_noframe(void *d) {
	remote_printf(100,"disabled frame notification.");
	remote_mode&=~1;
}

void xapi_bidir_frame(void *d) {
	remote_printf(100,"enabled frame notify.");
	remote_mode|=1;
}

void xapi_null(void *d) {
	remote_printf(402,"command not implemented.");
}

void api_help(void *d);

//--------------------------------------------
// cmd interpreter 
//--------------------------------------------
typedef void myfunction (void *);

typedef struct _command {
	const char *name;
	const char *help;
	struct _command *children; // unused
	myfunction *func;
	int sticky;  // unused
} Dcommand;


Dcommand cmd_root[] = {
	{"load ", "<filename>: replace current video file.", NULL , xapi_open, 0 },
	{"unload", ": close video file.", NULL , xapi_close, 0 },

	{"jack connect", ": connect and sync to jack server", NULL, xapi_open_jack , 0 },
	{"jack disconnect", ": disconnect from jack server", NULL, xapi_close_jack , 0 },
	{"jack status", ": get status of jack connection", NULL, xapi_jack_status , 0 },

	{"seek ", "<int>: seek to this frame - if jack and midi are offline", NULL, xapi_seek , 0 },
	{"get position", ": return current frame position", NULL, xapi_pposition , 0 },
	{"get smpte", ": return current frame position", NULL, xapi_psmpte , 0 },
	
	{"get fps", ": display current update frequency", NULL, xapi_pfps , 0 },
	{"set fps ", "<int>: set current update frequency", NULL, xapi_sfps , 0 },

	{"get offset", ": show current frame offset", NULL, xapi_poffset , 0 },
	{"set offset", "<int>: set current frame offset", NULL, xapi_soffset , 0 },

	{"get file", ": print filename of current video buffer", NULL, xapi_pfilename , 0 },
	{"get duration", ": query length of video buffer in seconds", NULL, xapi_pduration , 0 },
	{"get frames", ": show length of video buffer in frames", NULL, xapi_pframes , 0 },
	{"get framerate", ": show frame rate of video file", NULL, xapi_pframerate , 0 },
	{"get width", ": query width of video source buffer", NULL, xapi_pmwidth , 0 },
	{"get height", ": query width of video source buffer", NULL, xapi_pmheight , 0 },

	{"get seekmode", ": returns 1 if decoding keyframes only", NULL, xapi_pseekmode, 0 },
	{"set seekmode ", "<1|0>: set to one to seek only keyframes", NULL, xapi_sseekmode, 0 },

	{"window close", ": close window", NULL, xapi_close_window, 0 },
	{"window open", ": open window", NULL, xapi_open_window, 0 },
	{"window mode " , "<int>: changes video mode and opens window", NULL, xapi_set_videomode, 0 },
	{"window resize " , "<int>|<int>x<int>: resize window (percent of movie or absolute)", NULL, xapi_swinsize, 0 },
	{"window position " , "<int>x<int>: move window", NULL, xapi_swinpos, 0 },
	{"window pos " , "<int>x<int>: move window", NULL, xapi_swinpos, 0 },

	{"get windowsize" , ": show current window size", NULL, xapi_pwinsize, 0 },
	{"get windowpos" , ": show current window position", NULL, xapi_pwinpos, 0 },

	{"get videomode" , ": display current video mode", NULL, xapi_pvideomode, 0 },
	{"list videomodes" , ": displays a list of possible video modes", NULL, xapi_null, 0 },
	{"ask videomodes" , ": dump CSV list of possible video modes", NULL, xapi_null, 0 },

	{"osd frame " , "<ypos>: render current framenumber. y=0..100 (<0 disable)", NULL, xapi_osd_frame, 0 },
	{"osd smpte " , "<ypos>: render smpte. y=0..100 (<0 disable)", NULL, xapi_osd_smpte, 0 },
	{"osd text " , "<text>: render <text> on screen", NULL, xapi_osd_text, 0 },
	{"osd text" , ": display prev. OSD text", NULL, xapi_osd_on, 0 },
	{"osd notext" , ": clear text OSD", NULL, xapi_osd_off, 0 },
	{"osd off" , ": same as 'osd notext'", NULL, xapi_osd_off, 0 },
	{"osd on" , ": same as 'osd text'", NULL, xapi_osd_on, 0 },
	{"osd pos " , "<xalign> <ypos>: xalign=0..2 (L,C,R) ypos=0..100", NULL, xapi_osd_pos, 0 },
	{"osd available" , ": return 100 if freetype OSD is available", NULL, xapi_osd_avail, 0 },
	{"osd font " , "<filename>: use this TTF font file", NULL, xapi_osd_font, 0 },

	{"osd box" , ": forces a black box around the OSD", NULL, xapi_osd_box, 0 },
	{"osd nobox" , ": make OSD backgroung transparent", NULL, xapi_osd_nobox, 0 },

	{"notify frame" , ": enable async frame-update messages", NULL, xapi_bidir_frame, 0 },
	{"notify disable" , ": disable async messages", NULL, xapi_bidir_noframe, 0 },

	{"midi autoconnect", ": discover and connect to midi time source", NULL, xapi_detect_midi, 0 },
	{"midi connect ", "<int>: connect to midi time source", NULL, xapi_open_midi, 0 },
	{"midi disconnect", ": unconect from midi device", NULL, xapi_close_midi, 0 },
	{"midi status", ": show connected midi port", NULL, xapi_midi_status, 0 },
	{"get midisync", ": display midi smpte conversion mode", NULL, xapi_pmidisync, 0 },
	{"set midisync ", "<int>: MTC smpte conversion. 0:MTC 2:Video 3:resample", NULL, xapi_smidisync, 0 },

	{"help", ": show a quick help", NULL , api_help, 0 },
	{"quit", ": quit xjadeo", NULL , xapi_quit, 0 },
	{NULL, NULL, NULL , NULL, 0},
};

Dcommand *cur_root= cmd_root;

void api_help(void *d) {
	int i=0;
	remote_printf(100, "print help");
	remote_printf(800, "+ xjadeo remote control commands:");
	while (cur_root[i].name) {
		remote_printf(800,"+  %s%s",cur_root[i].name,cur_root[i].help);
		i++;
	}
}

void exec_remote_cmd (char *cmd) {
	int i=0;
	while (cur_root[i].name) {
		if (strncmp(cmd,cur_root[i].name,strlen(cur_root[i].name))==0) break; 
		i++;
	}
	if (!cur_root[i].name) {
		remote_printf(400,"unknown command.");
		return; // no cmd found
	}
	if ( cur_root[i].func)
		cur_root[i].func(cmd+strlen(cur_root[i].name));
	else 
		remote_printf(401,"command not implemented.");
}


//--------------------------------------------
// remote control
//--------------------------------------------


typedef struct {
	char buf[REMOTEBUFSIZ];
	int offset;
}remotebuffer;

remotebuffer *inbuf;


int remote_read(void) {
	int rx;
	char *start, *end;

	if ((rx = read(REMOTE_RX, inbuf->buf + inbuf->offset, (REMOTEBUFSIZ-1)-inbuf->offset)) > 0) {
		inbuf->offset+=rx;
		inbuf->buf[inbuf->offset] = '\0';
	}
	start=inbuf->buf;

	while ((end = strchr(start, '\n'))) {
		*(end) = '\0';
		exec_remote_cmd(start);
		inbuf->offset-=((++end)-start);
		if (inbuf->offset) memmove(inbuf->buf,end,inbuf->offset);
	}

	return(0);
}

#define LOGLEN 1023

void remote_printf(int rv, const char *format, ...) {
//	FILE *out = stdout;
	va_list arglist;
	char text[LOGLEN];
	char msg[LOGLEN];

	va_start(arglist, format);
	vsnprintf(text, LOGLEN, format, arglist);
	va_end(arglist);

	text[LOGLEN -1] =0; // just to be safe :)
//	fprintf(out, "@%i %s\n",rv,text);
	snprintf(msg, LOGLEN, "@%i %s\n",rv,text);
	msg[LOGLEN -1] =0; 
 	write(REMOTE_TX,msg,strlen(msg));
}

#if 0
int remote_select(void)
{
	fd_set fd;
	struct timeval tv = { 0, 0 };
	FD_ZERO(&fd);
	FD_SET(REMOTE_RX,&fd);

	if (select((REMOTE_RX+1), &fd, NULL, NULL, &tv)) return remote_read();
	return 0;
}

inline int poll_remote(void) {
	if (remote_en) return (remote_select());
	return(0);
}
#endif

void open_remote_ctrl (void) {
	inbuf=malloc(sizeof(remotebuffer));
	inbuf->offset=0;
	remote_printf(800, "xjadeo - remote control (type 'help<enter>' for info)");
}

void close_remote_ctrl (void) {
	free(inbuf);
}

int remote_fd_set(fd_set *fd) {
	FD_SET(REMOTE_RX,fd);
	return( REMOTE_RX+1);
}
