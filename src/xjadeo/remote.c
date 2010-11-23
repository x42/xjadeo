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
 *
 *
 * XAPI return values:
 *  1xx: command succeeded 
 *  2xx: query variable succeeded: 
 *  4xx: error
 *  8xx: info message (eg. help)
 *
 * more detailed: 
 *  100: <text>
 *  101: var=<int> // long int 
 *  124: vmode=<int> : <string> (list of avail video modes)
 *
 *  201: var=<int>  // long int
 *  202: var=<double>
 *  210: var=<int>x<int> 
 *  220: var=<string>
 *  228: var=<smpte-string>
 *
 *  suggestions, TODO:
 *  3xx: command succeeded, but status is negative.
 *    eg. 310 midi not connected, but available
 *       which is currenlty 199.
 *
 */
#include "xjadeo.h"

#ifdef OLD_FFMPEG
#include <avcodec.h> // needed for PIX_FMT 
#include <avformat.h>
#else
#include <libavcodec/avcodec.h> // needed for PIX_FMT 
#include <libavformat/avformat.h>
#endif

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

extern AVFormatContext   *pFormatCtx;
extern int               videoStream;

extern int 	force_redraw; // tell the main event loop that some cfg has changed

/* Option flags and variables */
extern char *current_file;
extern long	ts_offset;
extern char    *smpte_offset;
extern long	userFrame;
extern long	dispFrame;
extern int want_quiet;
extern int want_verbose;
extern int want_letterbox;
extern int remote_en;
extern int mq_en;
extern char *ipc_queue;
extern int remote_mode;

#ifdef HAVE_MIDI
extern int midi_clkconvert;
extern int midi_clkadj;
extern char midiid[32];
#endif

#ifdef TIMEMAP
extern long   timeoffset;
extern double timescale;
extern int    wraparound;
#endif

extern double 		delay;
extern double 		filefps;
extern int		videomode;
extern int 		seekflags;
extern int    interaction_override;

// On screen display
extern char OSD_fontfile[1024]; 
extern char OSD_text[128];
extern int OSD_mode;

extern int OSD_fx, OSD_tx, OSD_sx, OSD_fy, OSD_sy, OSD_ty;

extern jack_client_t *jack_client;
extern char jackid[16];


// TODO: someday we can implement 'mkfifo' or open / pipe
#define REMOTE_RX (fileno(stdin))
#define REMOTE_TX (fileno(stdout))

#define REMOTEBUFSIZ 4096
int remote_read(void);
void remote_printf(int val, const char *format, ...);

//--------------------------------------------
// API commands
//--------------------------------------------

void xapi_printversion(void *d) {
	remote_printf(220, "version=%s", VERSION);
}

void xapi_open(void *d) {
	char *fn= (char*)d;
	//printf("open file: '%s'\n",fn);
	if ( open_movie(fn)) 
		remote_printf(403, "failed to open file '%s'",fn);
	else	remote_printf(129, "opened file: '%s'",fn);
        init_moviebuffer();
	newsourcebuffer();
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update screen
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
	int vmode; 
	if (getvidmode() !=0) {
		remote_printf(413, "cannot change videomode while window is open.");
		return;
	}
	vmode=parsevidoutname(d);
	if (vmode==0 ) vmode = atoi((char*)d);
	if (vmode <0) {
		remote_printf(414, "video mode needs to be a positive integer or 0 for autodetect.");
		return;
	}
	render_fmt = vidoutmode(vmode);
	remote_printf(100, "setting video mode to %i",getvidmode());

	open_window(); // required here; else VOutout callback fn will fail.

	if (pFrameFMT && current_file) { 
		// reinit buffer with new format
		char *fn=strdup(current_file);
		open_movie(fn);
		free(fn);
	} else {
		if(buffer) free(buffer); buffer=NULL;
		// set videomode to 0 or loop_flag=0?
	}
	init_moviebuffer();
	force_redraw=1;
}

void xapi_open_window(void *d) {
	if (getvidmode() !=0) {
		remote_printf(412, "window already open.");
		return;
	}
	xapi_set_videomode("0");
}


void xapi_pvideomode(void *d) {
	remote_printf(201,"videomode=%i", getvidmode());
}

void xapi_lvideomodes(void *d) {
	int i=0;
	remote_printf(100,"list video modes.");
	while (vidoutsupported(++i)>=0) {
		if (vidoutsupported(i)) 
			remote_printf(124,"vmode=%i : %s",i,vidoutname(i));
		else  
			remote_printf(800,"n/a=%i : %s",i,vidoutname(i));
	}
}

void xapi_pletterbox(void *d) {
	if (want_letterbox)
		remote_printf(201,"letterbox=1 # fixed aspect ratio");
	else
		remote_printf(201,"letterbox=0 # free scaling");
}

void xapi_sletterbox(void *d) {
	int action=2;
	if (!strcmp(d,"on") || atoi(d)==1) action=1;
	else if (!strcmp(d,"off")) action=0;
	Xletterbox (action);
	xapi_pletterbox(NULL);
}

void xapi_pwinpos(void *d) {
	int x,y;
	Xgetpos(&x,&y); 
	remote_printf(210,"windowpos=%ix%i",x,y);
}

void xapi_pwinsize(void *d) {
	unsigned int x,y;
	Xgetsize(&x,&y); 
	remote_printf(210,"windowsize=%ux%u",x,y);
}

void xapi_saspect (void *d) {
	unsigned int my_Width,my_Height;
	Xgetsize(&my_Width,&my_Height); 
	// resize to match movie aspect ratio
	// dup code in display_x11.c (!)
	if( ((float)movie_width/(float)movie_height) < ((float)my_Width/(float)my_Height) )
		my_Width=floor((float)my_Height * (float)movie_width / (float)movie_height);
	else	my_Height=floor((float)my_Width * (float)movie_height / (float)movie_width);

	remote_printf(100,"resizing window to %ux%u",my_Width, my_Height);
	Xresize(my_Width, my_Height);
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

void xapi_ontop(void *d) {
	int action=_NET_WM_STATE_TOGGLE;
	if (!strcmp(d,"on") || atoi(d)==1) action=_NET_WM_STATE_ADD;
	else if (!strcmp(d,"toggle")) action=_NET_WM_STATE_TOGGLE;
	else if (!strcmp(d,"off") || atoi(d)==0) action=_NET_WM_STATE_REMOVE;
	remote_printf(100,"ok.");
	Xontop(action);
}

void xapi_fullscreen(void *d) {
	int action=_NET_WM_STATE_TOGGLE;
	if (!strcmp(d,"on") || atoi(d)==1) action=_NET_WM_STATE_ADD;
	else if (!strcmp(d,"toggle")) action=_NET_WM_STATE_TOGGLE;
	else if (!strcmp(d,"off") || atoi(d)==0) action=_NET_WM_STATE_REMOVE;
	remote_printf(100,"ok.");
	Xfullscreen(action);
}

void xapi_pontop(void *d) {
	if (Xgetontop())
		remote_printf(201,"windowontop=1 # always on top");
	else
		remote_printf(201,"windowontop=0 # normal window stack");
}

void xapi_pfullscreen(void *d) {
	if (Xgetfullscreen())
		remote_printf(201,"fullscreen=1 # full-screen");
	else
		remote_printf(201,"fullscreen=0 # windowed");
}

void xapi_mousepointer(void *d) {
	int action=2;
	if (!strcmp(d,"on") || atoi(d)==1) action=1;
	else if (!strcmp(d,"off")) action=0;
	Xmousepointer (action);
	remote_printf(100,"ok.");
}

void xapi_poverride(void *d) {
		remote_printf(201,"override=%i", interaction_override);
}

void xapi_soverride(void *d) {
  interaction_override=atoi(d);
	//remote_printf(100,"ok.");
	xapi_poverride(NULL);
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

void xapi_exit(void *d) {
	remote_printf(489,"exit is not a xjadeo command - use 'quit' to terminate this session.");
}

void xapi_quit(void *d) {
	remote_printf(100,"quit.");
	loop_flag=0;
}

void xapi_pfilename(void *d) {
	if (current_file) 
		remote_printf(220, "filename=%s", current_file);
	else 
		remote_printf(410, "no open video file");
}

void xapi_pduration(void *d) {
	remote_printf(202, "duration=%g", duration);
}

void xapi_pframerate(void *d) {
	remote_printf(202, "framerate=%g", framerate);
}

void xapi_pframes(void *d) {
	remote_printf(201, "frames=%ld ", frames);
}

void xapi_poffset(void *d) {
	remote_printf(201,"offset=%li",(long int) ts_offset);
}

void xapi_ptimescale(void *d) {
#ifdef TIMEMAP
	remote_printf(202,"timescale=%g", timescale);
	remote_printf(201,"timeoffset=%li",(long int) timeoffset);
#else
	remote_printf(499,"timescale is not available.");
#endif
}

void xapi_ploop(void *d) {
#ifdef TIMEMAP
	remote_printf(201,"loop=%d", wraparound);
#else
	remote_printf(499,"looping is not available.");
#endif
}

void xapi_pseekmode (void *d) {
	switch (seekflags) {
		case SEEK_ANY:
			remote_printf(201,"seekmode=2 # any"); break;
		case SEEK_KEY:
			remote_printf(201,"seekmode=3 # keyframe"); break;
		default:
			remote_printf(201,"seekmode=1 # continuous"); break;
	}
}

void xapi_sseekmode (void *d) {
	char *mode= (char*)d;
	if (!strcmp(mode,"key") || atoi(mode)==3)
		seekflags=SEEK_KEY;
	else if (!strcmp(mode,"any") || atoi(mode)==2)
		seekflags=SEEK_ANY;
	else if (!strcmp(mode,"continuous") || atoi(mode)==1)
		seekflags=SEEK_CONTINUOUS;
	else { 
		remote_printf(422,"invalid argument (1-3 or 'continuous', 'any', 'key')");
		return;
	}
	xapi_pseekmode(NULL);
}

void xapi_pmwidth(void *d) {
	remote_printf(201,"movie_width=%i", movie_width);
}

void xapi_pmheight(void *d) {
	remote_printf(201,"movie_height=%i", movie_height);
}

void xapi_soffset(void *d) {
  	//long int new = atol((char*)d);
	ts_offset = smptestring_to_frame((char*)d);
	remote_printf(101,"offset=%li",(long int) ts_offset);
}

void xapi_stimescale(void *d) {
#ifdef TIMEMAP
        char *t1;
  	timescale = atof((char*)d);
	if ((t1=strchr((char*)d,' ')) && ++t1) {
  	  timeoffset = atol(t1);
        }
	xapi_ptimescale(NULL);
	force_redraw=1;
#else
	remote_printf(499,"timescale is not available.");
#endif
}

void xapi_sloop(void *d) {
#ifdef TIMEMAP
	wraparound = atoi((char*)d)?1:0;
#else
	remote_printf(499,"timescale is not available.");
#endif
}

void xapi_sreverse(void *d) {
#ifdef TIMEMAP
  	timescale *= -1.0; 
	if (timescale<0) 
		timeoffset = (-2.0*timescale) *dispFrame; // TODO: check file-offset and ts_offset.
	else 
		timeoffset = 0; // TODO - applt diff dispFrame <> transport src
#endif
}

void xapi_pposition(void *d) {
	remote_printf(201,"position=%li",dispFrame);
}

void xapi_psmpte(void *d) {
	char smptestr[13];
	frame_to_smptestring(smptestr,dispFrame);
	remote_printf(228,"smpte=%s",smptestr);
}

void xapi_seek(void *d) {
  	//long int new = atol((char*)d);
	userFrame= smptestring_to_frame((char*)d);
	remote_printf(101,"defaultseek=%li",userFrame);
}

void xapi_pfps(void *d) {
	remote_printf(201,"updatefps=%i",(int) rint(1/delay));
}

void xapi_sfps(void *d) {
	char *off= (char*)d;
	if(atof(off)>0)
		delay = 1.0 / atof(off);
	else delay = -1; // use file-framerate
	remote_printf(101,"updatefps=%i",(int) rint(1/delay));
}

void xapi_sframerate(void *d) {
	char *off= (char*)d;
	if (!(atof(off)>0)) {
		remote_printf(423,"invalid argument (range >0)"); 
		return;
	}
        filefps= atof(off);
	override_fps(filefps);
	remote_printf(202, "framerate=%g", framerate);
}

void xapi_jack_status(void *d) {
	if (jack_client) 
		remote_printf(220,"jackclient=%s",jackid);
	else 
		remote_printf(100,"not connected to jack server");
}

void xapi_ltc_status(void *d) {
	if (ltcjack_connected())
		remote_printf(220,"jackclient=%s",ltc_jack_client_name());
	else 
		remote_printf(100,"no open LTC JACK source");
}

void xapi_open_ltc(void *d) {
#ifdef HAVE_LTCSMPTE
#ifdef HAVE_MIDI
	midi_close();
#endif
	close_jack();
	open_ltcjack((char *) d); // TODO: autoconnect 
	if (ltcjack_connected())
		remote_printf(100,"opened LTC jack port.");
	else 
		remote_printf(405,"failed to connect to jack server");
#else
	remote_printf(499,"LTC-jack is not available.");
#endif
}

void xapi_close_ltc(void *d) {
#ifdef HAVE_LTCSMPTE
	close_ltcjack();
	remote_printf(100,"closed ltc-jack connection");
#else
	remote_printf(499,"LTC-jack is not available.");
#endif
}

void xapi_open_jack(void *d) {
#ifdef HAVE_MIDI
	midi_close();
#endif
#ifdef HAVE_LTCSMPTE
	close_ltcjack();
#endif
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
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
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
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_off(void *d) {
	OSD_mode&=~OSD_TEXT;
	remote_printf(100,"hiding OSD");
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_on(void *d) {
	OSD_mode|=OSD_TEXT;
	remote_printf(100,"rendering OSD");
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_text(void *d) {
	snprintf(OSD_text,128,"%s",(char*)d);
	display_frame((int64_t)(dispFrame),1); // update OSD
	xapi_osd_on(NULL);
}

void xapi_osd_font(void *d) {
	snprintf(OSD_fontfile,1024,"%s",(char*)d);
	xapi_osd_on(NULL);
}

void xapi_osd_nobox(void *d) {
	OSD_mode&=~OSD_BOX;
	remote_printf(100,"OSD transparent background");
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_box(void *d) {
	OSD_mode|=OSD_BOX;
	remote_printf(100,"OSD black box background");
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_osd_avail(void *d) {
#ifdef HAVE_FT
	remote_printf(100,"rendering OSD is supported");
#else
	remote_printf(490,"this feature is not compiled");
#endif
}

void xapi_osd_mode(void *d) {
	int m = atoi((char*)d);
	if (m&1) OSD_mode|=OSD_FRAME; else OSD_mode&=~OSD_FRAME;
	if (m&2) OSD_mode|=OSD_SMPTE; else OSD_mode&=~OSD_SMPTE;
	if (m&4) OSD_mode|=OSD_TEXT;  else OSD_mode&=~OSD_TEXT;
	if (m&8) OSD_mode|=OSD_BOX;   else OSD_mode&=~OSD_BOX;
	remote_printf(100,"set osdmode=%i", (OSD_mode&OSD_FRAME?1:0)|(OSD_mode&OSD_SMPTE?2:0)|(OSD_mode&OSD_TEXT?4:0)|(OSD_mode&OSD_BOX?8:0));
	force_redraw=1;
}

void xapi_posd(void *d) {
#ifdef HAVE_FT
	remote_printf(201,"osdmode=%i", (OSD_mode&OSD_FRAME?1:0)|(OSD_mode&OSD_SMPTE?2:0)|(OSD_mode&OSD_TEXT?4:0)|(OSD_mode&OSD_BOX?8:0));
	remote_printf(220,"osdfont=%s", OSD_fontfile); 
	remote_printf(220,"osdtext=%s", OSD_text); 
#else
	remote_printf(490,"this feature is not compiled");
#endif
}

void xapi_psync(void *d) {
	int ss =0;
#ifdef HAVE_LTCSMPTE
	if (ltcjack_connected()) ss=3;
	else
#endif
#ifdef HAVE_MIDI
	if (midi_connected()) ss=2;
	else
#endif
	if (jack_connected()) ss=1;
	remote_printf(201,"syncsource=%i",ss);
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
	force_redraw=1;
	//display_frame((int64_t)(dispFrame),1); // update OSD
}

void xapi_midi_status(void *d) {
#ifdef HAVE_MIDI
	// FIXME: we shall return "200,midiid=XXX" 
	// and "100 not connected" ?? as in jack connected
	// BUT: // midiid can be <int> (portmidi) or string (alsa midi)
	if (midi_connected())
		remote_printf(100,"midi connected.");
	else
		remote_printf(199,"midi not connected.");
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_smididriver(void *d) {
#ifdef HAVE_MIDI
	char *mp = NULL;
	if (d && strlen(d)>0) mp=d; 
	if (midi_choose_driver(mp)>0) {
		remote_printf(100,"ok.");
	} else {
		remote_printf(440,"chosen MIDI driver is not supported.");
	}
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_open_midi(void *d) {
#ifdef HAVE_MIDI
	char *mp;
	if (d && strlen(d)>0) mp=d; 
	else mp="-1"; // midiid ?

	if (midi_connected())
		remote_printf(441,"MIDI port already connected.");
	midi_open(mp);
	if (midi_connected()) {
		remote_printf(100,"MIDI connected.");
	  	strncpy(midiid,mp,32);
		midiid[31]=0;
	} else
		remote_printf(440,"MIDI open failed.");
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_reopen_midi(void *d) {
#ifdef HAVE_MIDI
	midi_close();
	xapi_open_midi(midiid);
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

void xapi_pmidilibrary (void *d) {
#ifdef HAVE_MIDI
	remote_printf(220,"mididrv=%s", midi_driver_name());
#else
	remote_printf(499,"midi not available.");
#endif
}

	
void xapi_pmidiclk(void *d) {
#ifdef HAVE_MIDI
	remote_printf(201,"midiclk=%i", midi_clkadj);
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_smidiclk(void *d) {
#ifdef HAVE_MIDI
        midi_clkadj = atoi((char*)d)?1:0;
	remote_printf(101,"midiclk=%i", midi_clkadj);
#else
	remote_printf(499,"midi not available.");
#endif
}

	
void xapi_pmidisync(void *d) {
#ifdef HAVE_MIDI
	remote_printf(201,"midisync=%i", midi_clkconvert);
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_smidisync(void *d) {
#ifdef HAVE_MIDI
        midi_clkconvert = atoi((char*)d);
	remote_printf(101,"midisync=%i", midi_clkconvert);
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_bidir_noframe(void *d) {
	remote_printf(100,"disabled frame notification.");
	remote_mode&=~3;
}

void xapi_bidir_loop(void *d) {
	remote_printf(100,"enabled frame notify.");
	remote_mode|=1;
}

void xapi_bidir_frame(void *d) {
	remote_printf(100,"enabled frame notify.");
	remote_mode|=2;
}

void xapi_ping(void *d) {
	remote_printf(100,"pong.");
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
	struct _command *children;
	myfunction *func;
	int sticky;  // unused
} Dcommand;

Dcommand cmd_test[] = {
	{"load ", "<filename>: replace current video file.", NULL , xapi_open, 0 },
	{"seek ", "<int>: seek to this frame - if jack and midi are offline", NULL, xapi_seek , 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_midi[] = {
/*	{"autoconnect", ": discover and connect to midi time source", NULL, xapi_detect_midi, 0 }, */
	{"connect ", "<port>: connect to midi time source (-1: discover)", NULL, xapi_open_midi, 0 },
	{"disconnect", ": unconect from midi device", NULL, xapi_close_midi, 0 },
	{"reconnect", ": connect to last specified midi port", NULL, xapi_reopen_midi, 0 },
	{"status", ": display status of midi connection.", NULL, xapi_midi_status, 0 },
	{"driver ", "<name>: select midi driver.", NULL, xapi_smididriver, 0 },
	{"driver", ": display the used midi driver", NULL, xapi_pmidilibrary, 0 },
	{"library", ": alias for 'midi driver' (deprecated)", NULL, xapi_pmidilibrary, 0 },
	{"sync ", "<int>: set MTC smpte conversion. 0:MTC 2:Video 3:resample", NULL, xapi_smidisync, 0 },
	{"clk ", "[1|0]: use MTC quarter frames.", NULL, xapi_smidiclk, 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_jack[] = {
	{"connect", ": connect and sync to jack server", NULL, xapi_open_jack , 0 },
	{"disconnect", ": disconnect from jack server", NULL, xapi_close_jack , 0 },
	{"status", ": get status of jack connection", NULL, xapi_jack_status , 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_ltc[] = {
	{"connect", ": connect and sync to jack server", NULL, xapi_open_ltc , 0 },
	{"disconnect", ": disconnect from jack server", NULL, xapi_close_ltc , 0 },
	{"status", ": get status of jack connection", NULL, xapi_ltc_status , 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_osd[] = {
	{"frame " , "<ypos>: render current framenumber. y=0..100 (<0 disable)", NULL, xapi_osd_frame, 0 },
	{"smpte " , "<ypos>: render smpte. y=0..100 (<0 disable)", NULL, xapi_osd_smpte, 0 },
	{"text " , "<text>: render <text> on screen", NULL, xapi_osd_text, 0 },
	{"text" , ": display prev. OSD text", NULL, xapi_osd_on, 0 },
	{"notext" , ": clear text OSD", NULL, xapi_osd_off, 0 },
	{"off" , ": same as 'osd notext'", NULL, xapi_osd_off, 0 },
	{"on" , ": same as 'osd text'", NULL, xapi_osd_on, 0 },
	{"pos " , "<xalign> <ypos>: xalign=0..2 (L,C,R) ypos=0..100", NULL, xapi_osd_pos, 0 },
	{"available" , ": return 100 if freetype OSD is available", NULL, xapi_osd_avail, 0 },
	{"font " , "<filename>: use this TTF font file", NULL, xapi_osd_font, 0 },
	{"box" , ": forces a black box around the OSD", NULL, xapi_osd_box, 0 },
	{"nobox" , ": make OSD backgroung transparent", NULL, xapi_osd_nobox, 0 },
	{"mode" , ":<int>: restore OSD as returned by 'get osdcfg'", NULL, xapi_osd_mode, 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_get[] = {
	{"position", ": return current frame position", NULL, xapi_pposition , 0 },
	{"smpte", ": return current frame position", NULL, xapi_psmpte , 0 },
	{"fps", ": display current update frequency", NULL, xapi_pfps , 0 },
	{"offset", ": show current frame offset", NULL, xapi_poffset , 0 },
	{"timescale", ": show scale/offset", NULL, xapi_ptimescale , 0 },
	{"loop", ": show loop/wrap-around setting", NULL, xapi_ploop , 0 },

	{"file", ": print filename of current video buffer", NULL, xapi_pfilename , 0 },
	{"duration", ": query length of video buffer in seconds", NULL, xapi_pduration , 0 },
	{"frames", ": show length of video buffer in frames", NULL, xapi_pframes , 0 },
	{"framerate", ": show frame rate of video file", NULL, xapi_pframerate , 0 },
	{"width", ": query width of video source buffer", NULL, xapi_pmwidth , 0 },
	{"height", ": query width of video source buffer", NULL, xapi_pmheight , 0 },

	{"seekmode", ": display how video frames are searched ", NULL, xapi_pseekmode, 0 },
	{"windowsize" , ": show current window size", NULL, xapi_pwinsize, 0 },
	{"windowpos" , ": show current window position", NULL, xapi_pwinpos, 0 },
	{"videomode" , ": display current video mode", NULL, xapi_pvideomode, 0 },
	{"midisync", ": display midi smpte conversion mode", NULL, xapi_pmidisync, 0 },
	{"midiclk", ": MTC quarter frame precision", NULL, xapi_pmidiclk, 0 },
	{"osdcfg", ": display status on screen display", NULL, xapi_posd, 0 },
	{"syncsource", ": display currently used sync source", NULL, xapi_psync, 0 },
	{"letterbox" , ": query video scaling mode", NULL, xapi_pletterbox, 0 },
	{"fullscreen" , ": is xjadeo displayed on full screen", NULL, xapi_pfullscreen, 0 },
	{"ontop" , ": query window on top mode", NULL, xapi_pontop, 0 },
	{"override", ": query disabled window events", NULL, xapi_poverride , 0 },
	{"version", ": query xjadeo version", NULL, xapi_printversion , 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_notify[] = {
	{"frame" , ": enable async frame-update messages", NULL, xapi_bidir_frame, 0 },
	{"loop" , ": enable continuous frame position messages", NULL, xapi_bidir_loop, 0 },
	{"disable" , ": disable async messages", NULL, xapi_bidir_noframe, 0 },
	{"off" , ": disable async messages", NULL, xapi_bidir_noframe, 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_window[] = {
	{"close", ": close window", NULL, xapi_close_window, 0 },
	{"open", ": open window", NULL, xapi_open_window, 0 },
	{"mode " , "<int>: changes video mode and opens window", NULL, xapi_set_videomode, 0 },
	{"resize " , "<int>|<int>x<int>: resize window (percent of movie or abs)", NULL, xapi_swinsize, 0 },
	{"size " , "<int>|<int>x<int>: alias for resize", NULL, xapi_swinsize, 0 },
	{"position " , "<int>x<int>: move window to absolute position", NULL, xapi_swinpos, 0 },
	{"pos " , "<int>x<int>: alias for 'window position'", NULL, xapi_swinpos, 0 },
	{"xy " , "<int>x<int>: alias for 'window position'", NULL, xapi_swinpos, 0 },
	{"fullscreen " , "[on|off|toggle]: en/disable fullscreen (only XV/x11/OSX)", NULL, xapi_fullscreen, 0 },
	{"zoom " , "[on|off|toggle]: alias for 'window fullscreen'", NULL, xapi_fullscreen, 0 },
	{"letterbox " , "[on|off|toggle]: don't break aspect ratio (only XV/x11-imlib2)", NULL, xapi_sletterbox, 0 },
	{"mouse " , "[on|off|toggle]: en/disable mouse cursor display (only XV/x11)", NULL, xapi_mousepointer, 0 },
	{"ontop " , "[on|off|toggle]: en/disable 'on top' (only XV/x11)", NULL, xapi_ontop, 0 },
	{"fixaspect" , ": scale window to match aspect ration (same as mouse btn 3)", NULL, xapi_saspect, 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_set[] = {
	{"fps ", "<float>: set current update frequency", NULL, xapi_sfps , 0 },
	{"offset ", "<int>: set current frame offset", NULL, xapi_soffset , 0 },
	{"timescale ", "<float> <int>: set timescale and offset", NULL, xapi_stimescale , 0 },
	{"loop ", "<int>: 0: normal, 1:wrap around", NULL, xapi_sloop , 0 },
	{"seekmode ", "<1-3>: seek continuous, to any or to keyframes only", NULL, xapi_sseekmode, 0 },
	{"framerate ", "<float>: override frame rate of video file", NULL, xapi_sframerate , 0 },
	{"override ", "<int>: disable window events", NULL, xapi_soverride , 0 },
	{NULL, NULL, NULL , NULL, 0}
};

Dcommand cmd_root[] = {
	// note: keep 'seek' on top of the list - if an external app wants seek a lot, xjadeo will 
	// not spend time comparing command strings - OTOH I/O takes much longer than this anyway :X
	{"seek ", "<int>: seek to this frame - if jack and midi are offline", NULL, xapi_seek , 0 },
	{"load ", "<filename>: replace current video file.", NULL , xapi_open, 0 },
	{"unload", ": close video file.", NULL , xapi_close, 0 },

	{"window", " .. : monitor window functions", cmd_window, NULL, 0 },
	{"notify", " .. : async remote info messages", cmd_notify, NULL, 0 },
	{"get", " .. : query xjadeo variables or state", cmd_get, NULL, 0 },
	{"set", " .. : set xjadeo variables", cmd_set, NULL, 0 },
	{"osd", " .. : on screen display commands", cmd_osd, NULL, 0 },
	{"jack", " .. : jack sync commands", cmd_jack, NULL, 0 },
	{"midi", " .. : midi sync commands", cmd_midi, NULL, 0 },
	{"ltc", " ..  : LTC sync commands", cmd_ltc, NULL, 0 },
	{"reverse", ": set timescale to reverse playback from now", NULL , xapi_sreverse, 0 },

	{"list videomodes" , ": displays a list of possible video modes", NULL, xapi_lvideomodes, 0 },
	{"ping", ": claim a pong", NULL , xapi_ping, 0 },
	{"exit", ": [close MQ-session]", NULL , xapi_exit, 0 },
	{"help", ": show a quick help", NULL , api_help, 0 },
	{"quit", ": terminate  xjadeo", NULL , xapi_quit, 0 },
	{NULL, NULL, NULL , NULL, 0},
};

// TODO new commands:
//  - welcome message. (on reconnect)
//  - query OSD status (qjadeo - reconnect)
//  - query midi settings  (xapi_midi_status)

void api_help_recursive(Dcommand *r, const char *prefix) {
	int i=0;
	while (r[i].name) {
		if (r[i].children) {
			int len = 2+strlen(prefix)+ strlen(r[i].name);
			char *tmp= malloc(len*sizeof(char));
			snprintf(tmp,len,"%s%s ",prefix,r[i].name);
			//remote_printf(800,"#  %s%s%s",prefix,r[i].name,r[i].help);
			api_help_recursive(r[i].children, tmp);
			free(tmp);
		} else  {
			remote_printf(800,"+  %s%s%s",prefix,r[i].name,r[i].help);
		}
		i++;
	}
}

void api_help(void *d) {
	remote_printf(100, "print help");
	remote_printf(800, "+ xjadeo remote control commands:");
	api_help_recursive(cmd_root,"");
}

void exec_remote_cmd_recursive (Dcommand *leave, char *cmd) {
	int i=0;
	while (*cmd==' ') ++cmd;
//	fprintf(stderr,"DEBUG: %s\n",cmd);

	while (leave[i].name) {
		if (strncmp(cmd,leave[i].name,strlen(leave[i].name))==0) break; 
		i++;
	}
	if (!leave[i].name) {
		remote_printf(400,"unknown command.");
		return; // no cmd found
	}
	if (leave[i].children) 
		exec_remote_cmd_recursive(leave[i].children,cmd+strlen(leave[i].name));
	else if (leave[i].func) {
		char *arg= cmd+strlen(leave[i].name);
		strtok(arg, "\r\n");
		leave[i].func(arg);
	} else 
		remote_printf(401,"command not implemented.");
}

#ifdef HAVE_LASH
# if !(defined HAVE_MQ || defined HAVE_IPCMSG)
#warning 
#warning 
#warning LASH support - but no POSIX message queues!
#warning 
#warning This xjadeo will not be able to (re)connect 
#warning to a GUI when launched by lashd!
#warning 
#warning 
# endif
#endif

//--------------------------------------------
// remote control - STDIO 
//--------------------------------------------


typedef struct {
	char buf[REMOTEBUFSIZ];
	int offset;
}remotebuffer;

remotebuffer *inbuf;
int remote_read_io(void) {
	int rx;
	char *start, *end;

	if ((rx = read(REMOTE_RX, inbuf->buf + inbuf->offset, (REMOTEBUFSIZ-1)-inbuf->offset)) > 0) {
		inbuf->offset+=rx;
		inbuf->buf[inbuf->offset] = '\0';
	}
	start=inbuf->buf;

	while ((end = strchr(start, '\n'))) {
		*(end) = '\0';
		//if (strlen(start) > 0) 
			exec_remote_cmd_recursive(cmd_root,start);
		inbuf->offset-=((++end)-start);
		if (inbuf->offset) memmove(inbuf->buf,end,inbuf->offset);
	}

	return(rx>0?0:-1);
}

#ifdef HAVE_WINDOWS
int remote_read_h(void) {
	int rv=-1;
	char buf[BUFSIZ];
	DWORD bytesAvail = 0;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	do {
		PeekNamedPipe(h, 0, 0, 0, &bytesAvail, 0);
		DWORD sr=0;
		if (bytesAvail > 0 && ReadFile(h, buf, BUFSIZ, &sr, NULL) && sr>0) {
			buf[sr]=0;
			char *start, *end;
			start = buf;
			while (*start && (end = strchr(start, '\n'))) {
				*(end) = '\0';
				strtok(start, "\r");
				//if (strlen(start) > 0) 
					exec_remote_cmd_recursive(cmd_root,start);
				start=end+1;
			}
		  rv=0;
		}
	} while (bytesAvail>0);
	return rv;
}
#endif


#ifdef HAVE_MQ
# define LOGLEN MQLEN
#elif defined(HAVE_IPCMSG)
# define LOGLEN BUFSIZ
#else
# define LOGLEN 1023
#endif

void remote_printf_io(int rv, const char *format, ...) {
	va_list arglist;
	char text[LOGLEN];
	char msg[LOGLEN];

	va_start(arglist, format);
	vsnprintf(text, LOGLEN, format, arglist);
	va_end(arglist);

	text[LOGLEN -1] =0; // just to be safe :)
	snprintf(msg, LOGLEN, "@%i %s\n",rv,text);
	msg[LOGLEN -1] =0; 
 	write(REMOTE_TX,msg,strlen(msg));
}

void open_remote_ctrl (void) {
	inbuf=malloc(sizeof(remotebuffer));
	inbuf->offset=0;
	remote_printf_io(800, "xjadeo - remote control (type 'help<enter>' for info)");
}

void close_remote_ctrl (void) {
	free(inbuf);
}

int remote_fd_set(fd_set *fd) {
	FD_SET(REMOTE_RX,fd);
	return( REMOTE_RX+1);
}


//--------------------------------------------
// POSIX message queeue
//--------------------------------------------

#ifdef HAVE_MQ
// prototypes in mqueue.c
int  mymq_read(char *data);
void mymq_reply(int rv, char *str);
void mymq_close(void);
int mymq_init(char *id);


/* MQ replacement for remote_printf() */
void remote_printf_mq(int rv, const char *format, ...) {
	va_list arglist;
	char text[MQLEN];

	va_start(arglist, format);
	vsnprintf(text, MQLEN, format, arglist);
	va_end(arglist);

	text[MQLEN -1] =0; // just to be safe :)
	mymq_reply(rv,text);
}

int remote_read_mq(void) {
	int rx;
	char data[MQLEN];
	char *t;
	int rv = -1;

	while ((rx=mymq_read(data)) > 0 ) { 
		if ((t =  strchr(data, '\n'))) *t='\0';
		//if (strlen(data) < 1) continue;
		exec_remote_cmd_recursive(cmd_root,data);
		rv=0;
	}
	return(rv);
}

void open_mq_ctrl (void) {
	if(mymq_init(NULL)) mq_en=0;
	else remote_printf_mq(800, "xjadeo - remote control (type 'help<enter>' for info)");
}

void close_mq_ctrl (void) {
	remote_printf(100, "quit.");
	mymq_close();
}
#elif defined HAVE_IPCMSG

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

struct msgbuf1 {
	long    mtype;
	char    mtext[BUFSIZ];
};

int msqtx = 0;
int msqrx = 0;

int myipc_reply(int id, char *msg){
	struct msgbuf1 txbuf;
	txbuf.mtype=1;
  snprintf(txbuf.mtext, BUFSIZ, "@%i %s\n", id, msg); // XXX
  if (msgsnd(msqtx, (const void*) &txbuf, strlen(txbuf.mtext), IPC_NOWAIT) == -1) {
		fprintf(stderr, "msgsnd failed., Error = %d: %s\n", errno, strerror(errno));
		return -1;
	}
	return 0;
}

int remote_read_ipc () {
	int rv;
	struct msgbuf1 rxbuf;

	rv = msgrcv(msqrx, (void*) &rxbuf, BUFSIZ, 1, IPC_NOWAIT);

	if(rv < 0 ) {
		if (errno != EAGAIN && errno != ENOMSG)  {
			fprintf(stderr, "Msgrcv failed., Error = %d: %s\n", errno, strerror(errno));
			close_ipcmsg_ctrl();
			free(ipc_queue);
			ipc_queue = NULL;
		}
		return (-1);
	}

	char *t, *s;
	s=rxbuf.mtext; // TODO remember end of long messages..
	while (s && *s && (t = strchr(s, '\n'))) {
		*t='\0';
	  if (strlen(s) < 1) continue;
	  exec_remote_cmd_recursive(cmd_root,s);
		s=t+1;
	}
	remote_read_ipc(); // read all queued messages..
	return(0);
}

int open_ipcmsg_ctrl (const char *queuename) {

  key_t key_tx = ftok (queuename, 'a');
  key_t key_rx = ftok (queuename, 'b');

	msqrx = msgget(key_rx, IPC_CREAT| S_IRUSR | S_IWUSR);
	msqtx = msgget(key_tx, IPC_CREAT| S_IRUSR | S_IWUSR);
	if(msqrx == -1 || msqtx == -1)  {
		printf("\ngetKey failed., Error = %d: %s\n", errno, strerror(errno));
		return -1;
	}
	return 0;
}

void close_ipcmsg_ctrl () {
	remote_printf(100, "quit.");
#if 0
	msgctl(msqtx, IPC_RMID, NULL);
	msgctl(msqrx, IPC_RMID, NULL);
#endif
}
#endif

//--------------------------------------------
// REMOTE + MQ wrapper 
//--------------------------------------------

void remote_printf(int rv, const char *format, ...) {
	va_list arglist;
	char text[LOGLEN];
	char msg[LOGLEN];

	va_start(arglist, format);
	vsnprintf(text, MQLEN, format, arglist);
	va_end(arglist);

	text[LOGLEN -1] =0; 
#ifdef HAVE_MQ
		/* remote_printf_mq(...) */
	mymq_reply(rv,text);
#elif HAVE_IPCMSG
		/* remote_printf_ipc(...) */
	if (ipc_queue) myipc_reply(rv,text);
#endif

		/* remote_printf_io(...) */
	if (remote_en) {
		snprintf(msg, LOGLEN, "@%i %s\n",rv,text);
		msg[LOGLEN -1] =0; 
		write(REMOTE_TX,msg,strlen(msg));
	}
}
