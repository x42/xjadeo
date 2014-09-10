/* xjadeo - jack video monitor main
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

#include <libavcodec/avcodec.h> // needed for PIX_FMT
#include <libavformat/avformat.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifdef PLATFORM_OSX
#include <pthread.h>
void osx_main ();
void osx_shutdown();
#endif

// configuration strings
char const * const cfg_features = ""
#define CFG_STRING ""
#ifdef HAVE_LTC
"LTC "
#endif
#ifdef JACK_SESSION
	"JACK-SESSION "
#endif
#ifdef HAVE_MQ
	"POSIX-MQueue "
#elif defined HAVE_IPCMSG
	"IPC-MSG "
#endif
#ifdef HAVE_LIBLO
	"OSC "
#endif
	;

char const * const cfg_midi = ""
#ifndef HAVE_MIDI
	"*disabled*"
#else /* have Midi */
# ifdef HAVE_JACKMIDI
	"jack-midi "
# endif
# ifdef ALSA_SEQ_MIDI
	"alsa-sequencer "
# endif
# ifdef HAVE_PORTMIDI
	"portmidi "
# endif
# ifdef ALSA_RAW_MIDI
	"alsa-raw "
# endif
#endif /* HAVE_MIDI */
	;

char const * const cfg_displays = ""
#if HAVE_GL
	"openGL "
#endif
#if HAVE_LIBXV
	"Xv "
#endif
#ifdef HAVE_SDL
	"SDL "
#endif
#if HAVE_IMLIB2
	"X11/imlib2"
# ifdef IMLIB2RGBA
	"(RGBA32) "
# else
	"(RGB24) "
# endif
#endif
#ifdef PLATFORM_OSX
	"OSX/quartz "
#endif
	;

#ifndef CFG_COMPAT
#define CFG_COMPAT "xjadeo1"
#endif

char cfg_compat[1024] = CFG_COMPAT;

//------------------------------------------------
// Globals
//------------------------------------------------

// Display loop

/* int loop_flag: main xjadeo event loop
 * if loop_flag is set to 0, xjadeo will exit
 */
int loop_flag = 1;

/* int loop_run: video update enable
 * if set to 0 no smpte will be polled and
 * no video frame updates are performed.
 */
int loop_run = 1;

// Video Decoder

int               movie_width  = 640;
int               movie_height = 360;
int               ffctv_width  = 640;
int               ffctv_height = 360;
float             movie_aspect = 640.0 / 360.0;
AVFormatContext   *pFormatCtx = NULL;
int               videoStream = -1;
AVCodecContext    *pCodecCtx = NULL;
AVFrame           *pFrame = NULL;
AVFrame           *pFrameFMT = NULL;
uint8_t           *buffer = NULL;

int               render_fmt = PIX_FMT_YUV420P; ///< needs to be set before calling movie_open

/* Video File Info */
double  duration = 1;
double  framerate = 1;
int64_t frames = 1;
int64_t file_frame_offset = 0;

/* Option flags and variables */
char *current_file = NULL;
char *smpte_offset = NULL;
int64_t ts_offset = 0;
int64_t userFrame = 0; // seek to this frame if jack and midi are N/A
int64_t dispFrame = 0; // global strorage... = (SMPTE+offset) with boundaries to [0..movie_file_frames]
int   force_redraw = 0;
int   hide_mouse = 0;

int   interaction_override = 0; // disable some options.
/* 1 (bit 0) : ignore 'ESC' quit key
 * 2 (bit 1) : igore window-manager close button
 * 4 (bit 2) : ignore OSX-Menu QUIT
 * 8 (bit 3) : ignore left-mouse click
 * 16(bit 4) : disable '+' '-' '{' '}' offset keys
 */

int want_quiet   =0;	/* --quiet, --silent */
int want_debug   =0;	/* -D --debug  (hidden option) */
int want_verbose =0;	/* --verbose */
int want_avverbose =0;	/* --avverbose */
int want_genpts =0;	/* --genpts */
int want_ignstart =0;	/* --ignorefileoffset */
int want_nosplash =0;	/* --nosplash */
int want_noindex =0;	/* --noindex */
int start_ontop =0;	/* --ontop // -a */
int start_fullscreen =0;/* --fullscreen // -s */
int want_letterbox =1;  /* --letterbox -b */
int want_dropframes =0; /* --dropframes -N  -- force using drop-frame timecode */
int want_autodrop =1;   /* --nodropframes -n (hidden option) -- allow using drop-frame timecode */
int remote_en =0;	/* --remote, -R */
int no_initial_sync =0; /* --nosyncsource, -J */
int jack_autostart =0; /* linked to no_initial_sync */
int osc_port =0;	/* --osc, -O */
int mq_en =0;		/* --mq, -Q */
char *ipc_queue = NULL; /* --ipc, -W */
int remote_mode =0;	/* 0: undirectional ; >0: bidir
			 * bitwise enable async-messages
			 *  so far only:
			 *   (1) notify timecode
			 *   (2) notify changed timecode
			 */

#ifdef HAVE_MIDI
char midiid[128] = "-2";  /* --midi # -1: autodetect -2: jack-transport, -3: none/userFrame */
int midi_clkconvert =0;	  /* --midifps [0:MTC|1:VIDEO|2:RESAMPLE] */
char *midi_driver = NULL; /* --mididriver */
#endif

int have_dropframes =0; /* detected from MTC;  TODO: force to zero if jack or user TC */
int jack_clkconvert =1; /* --jackfps  - NOT YET IMPLEMENTED
			 * [0:audio_frames_per_video_frame
			 * 1:video-file] */
int use_jack = 1;
#ifdef HAVE_LTC
int use_ltc = 0;        /* -l , --ltc */
#endif
char *load_rc = NULL;
char *load_movie = NULL;
#ifdef JACK_SESSION
char *jack_uuid = NULL;
int jack_session_restore = 0;
int js_winx = -1;
int js_winy = -1;
int js_winw = -1;
int js_winh = -1;
#endif

int    midi_clkadj =1;	/* --midiclk  */
int    videomode = 0; // --vo <int>  - default: autodetect
double delay = -1; // use file's FPS
int keyframe_interval_limit = 100;


// On screen display
char OSD_fontfile[1024] = FONT_FILE;
char OSD_text[128] = "xjadeo!";
char OSD_msg[128] = "";
char OSD_frame[48] = "";
char OSD_smpte[20] = "";
char OSD_nfo_tme[5][48] = {"", "", "", "", ""};
char OSD_nfo_geo[5][48] = {"", "", "", "", ""};
int OSD_mode = OSD_BOX;

int OSD_fx = OSD_CENTER;
int OSD_tx = OSD_CENTER;
int OSD_sx = OSD_CENTER;
int OSD_fy = 98; // percent
int OSD_sy =  2; // percent
int OSD_ty = 50; // percent

// prototypes .

int init_weak_jack();
static void usage (int status);
static void printversion (void);

static struct option const long_options[] =
{
	{"avverbose",           no_argument, 0,       'A'},
	{"ontop",               no_argument, 0,       'a'},
	{"no-letterbox",        no_argument, 0,       'b'},
	{"midiclk",             no_argument, 0,       'C'},
	{"no-midiclk",          no_argument, 0,       'c'},
	{"debug",               no_argument, 0,       'D'},
	{"midi-driver",         required_argument, 0, 'd'},
	{"screen-fps",          required_argument, 0, 'f'},
	{"help",                no_argument, 0,       'h'},
	{"ignore-file-offset",  no_argument, 0,       'I'},
	{"no-file-offset",      no_argument, 0,       'I'},
	{"info",                required_argument, 0, 'i'},
	{"no-initial-sync",     no_argument, 0,       'J'},
	{"keyframe-limit",      required_argument, 0, 'K'},
	{"ltc",                 no_argument, 0,       'l'},
	{"midifps",             required_argument, 0, 'M'},
	{"midi",                required_argument, 0, 'm'},
	{"drop-frames",         no_argument, 0,       'N'},
	{"no-drop-frames",      no_argument, 0,       'n'},
#ifdef HAVE_LIBLO
	{"osc",                 required_argument, 0, 'O'},
#endif
	{"offset",              required_argument, 0, 'o'},
	{"genpts",              no_argument, 0,       'P'},
	{"mq",                  no_argument, 0,       'Q'},
	{"quiet",               no_argument, 0,       'q'},
	{"silent",              no_argument, 0,       'q'},
	{"remote",              no_argument, 0,       'R'},
#ifdef JACK_SESSION
	{"rc",                  required_argument, 0, 'r'},
#endif
	{"no-splash",           no_argument, 0,       'S'},
	{"fullscreen",          no_argument, 0,       's'},
	{"ttf-font",            required_argument, 0, 'T'},
#ifdef JACK_SESSION
	{"uuid",                required_argument, 0, 'U'},
#endif
	{"version",             no_argument, 0,       'V'},
	{"verbose",             no_argument, 0,       'v'},

	{"ipc",                 required_argument, 0, 'W'},
	{"videomode",           required_argument, 0, 'x'},
	{"vo",                  required_argument, 0, 'x'},

	{"osc-doc",             no_argument, 0,       0x100},
	{"no-index",            no_argument, 0,       0x101},
	{NULL, 0, NULL, 0}
};


/* Set all the option flags according to the switches specified.
   Return the index of the first non-option argument.  */
static int
decode_switches (int argc, char **argv)
{
	int c;
	while ((c = getopt_long (argc, argv,
		"A"  /* avverbose */
		"a"  /* always on top */
		"b"  /* letterbox */
		"C"  /* --midiclk */
		"c"  /* --no-midiclk */
		"D"  /* debug */
		"d:" /* midi driver */
		"F:" /* file FPS */
		"f:" /* screen fps */
		"h"  /* help */
		"I"  /* ignorefileoffset */
		"i:" /* info - OSD-mode */
		"J"  /* no jack / no-initial sync */
		"K:" /* keyframe limit */
		"l"  /* --ltc */
		"M:" /* midi clk convert */
		"m:" /* midi interface */
		"N"  /* --dropframes */
		"n"  /* --nodropframes */
		"O:" /* --osc  */
		"o:" /* offset */
		"P"  /* genpts */
		"Q"  /* POSIX rt-message queues */
		"q"  /* quiet or silent */
		"R"  /* stdio remote control */
		"r:" /* --rc */
		"S"  /* nosplash */
		"s"  /* start in full-screen mode */
		"T:" /* ttf-font */
#ifdef JACK_SESSION
		"U:" /* --uuid */
#endif
		"V"  /* version */
		"v"  /* verbose */
		"W:" /* IPC message queues */
		"x:" /* video-mode */
			, long_options, (int *) 0)) != EOF)
	{
		switch (c) {
			case 'A':
				want_avverbose = !remote_en;
				break;
			case 'a':
				start_ontop = 1;
				break;
			case 'b':
				want_letterbox = 0;
				break;
			case 'D':
				want_debug = 1;
				break;
			case 'f':
				if(atof(optarg) > 0) {
					delay = 1.0 / atof(optarg);
				} else {
					delay = -1; // use file-framerate
				}
				break;
			case 'h':
				usage (0);
				break;
			case 'I':
				want_ignstart = 1;
				break;
			case 'i':
				OSD_mode = atoi(optarg) & (OSD_FRAME | OSD_SMPTE);
				if (!want_quiet) printf("On screen display: [%s%s%s]\n",
						(!OSD_mode) ? "off":
						(OSD_mode & OSD_FRAME) ? "frames":"",
						(OSD_mode & (OSD_FRAME | OSD_SMPTE)) == (OSD_FRAME | OSD_SMPTE) ? " " : "",
						(OSD_mode & OSD_SMPTE) ? "SMPTE" : ""
						);
				break;
			case 'J':
				no_initial_sync = 1;
				break;
			case 'K':
				keyframe_interval_limit = atoi(optarg);
				if (keyframe_interval_limit < 10) keyframe_interval_limit = 10;
				if (keyframe_interval_limit > 5000) keyframe_interval_limit = 5000;
				if (!want_quiet) printf("Set Keyframe Limit to: %d\n", keyframe_interval_limit);
				break;
			case 'l':
#ifdef HAVE_LTC
				use_ltc = 1;
#else
				printf("This version of xjadeo is compiled without LTC support\n");
#endif
				break;
			case 'N':
				want_dropframes = 1;
				break;
			case 'n':
				want_autodrop = 0;
				break;
			case 'O':
#ifdef HAVE_LIBLO
				osc_port = atoi(optarg);
				if (osc_port < 0) osc_port = 0;
				if (osc_port > 65535) osc_port = 65535;
#else
				printf("This version of xjadeo is compiled without OSC support\n");
#endif
				break;
			case 'o':
				smpte_offset = strdup(optarg);
				break;
			case 'P':
				want_genpts = 1;
				break;
			case 'Q':
				mq_en = 1;
				break;
			case 'q':
				want_quiet = 1;
				want_verbose = 0;
				want_avverbose = 0;
				break;
			case 'R':
				remote_en = 1;
				want_quiet = 1;
				want_verbose = 0;
				want_avverbose = 0;
				break;
			case 'r':
				if (load_rc) free(load_rc);
				load_rc = strdup(optarg);
				break;
			case 'S':
				want_nosplash = 1;
				break;
			case 's':
				start_fullscreen = 1;
				break;
			case 'T':
				strcpy(OSD_fontfile, optarg);
				break;
			case 'U':
#ifdef JACK_SESSION
				if (jack_uuid) free(jack_uuid);
				jack_uuid = strdup(optarg);
				jack_autostart = 1;
#endif
				break;
			case 'V':
				printversion();
				exit(0);
			case 'W':
				if (ipc_queue) free(ipc_queue);
				ipc_queue = strdup(optarg);
				break;
			case 'v':
				want_verbose = !remote_en;
				break;
			case 'x':
				videomode = atoi(optarg);
				if (videomode == 0) videomode = parsevidoutname(optarg);
				break;
#ifdef HAVE_MIDI
			case 'C':
				midi_clkadj = 1;
				break;
			case 'c':
				midi_clkadj = 0;
				break;
			case 'd':
				if (midi_driver) free(midi_driver);
				midi_driver = strdup(optarg);
				break;
			case 'M':
				midi_clkconvert = atoi(optarg);
				break;
			case 'm':
				strncpy(midiid,optarg,sizeof(midiid));
				midiid[(sizeof(midiid)-1)]=0;
				break;
#else
			case 'C':
			case 'c':
			case 'd':
			case 'M':
			case 'm':
				printf("This version of xjadeo is compiled without MIDI support\n");
				break;
#endif
			case 0x100:
				xjosc_documentation ();
				exit (0);
				break;
			case 0x101:
				want_noindex = 1;
				break;
			default:
				usage (EXIT_FAILURE);
				break;
		}
	}
	return optind;
}

static void
usage (int status)
{
  printf ("xjadeo - the X Jack Video Monitor\n\n");
  printf ("Usage: xjadeo [ OPTIONS ] [ video-file ]\n\n");
  printf ("\n\n"
"Xjadeo is a software video player that displays a video clip synchronized to an\n"
"external time source (MTC, LTC, JACK transport).\n"
"\n"
"Xjadeo is intended for soundtrack composition, video monitoring and useful for\n"
"any task that requires to synchronizing movie frames with audio events.\n"
"\n"
/*-------------------------------------------------------------------------------|" */
"Options:\n"
" -A, --avverbose           Display verbose video decoder messages.\n"
" -a, --ontop               Keep xjadeo window on top of other applications.\n"
" -b, --no-letterbox        Scale movie to fit window. Without this option a\n"
"                           letterbox is used to retain the aspect ratio.\n"
#if 0 // hidden option
" -C, --midiclk             Use MIDI quarter frames (default)\n"
" -c, --no-midiclk          Ignore MTC quarter frames.\n"
#endif
" -D, --debug               Print development related information.\n"
" -d <name>, --midi-driver <name>\n"
"                           Specify midi driver to use. Run 'xjadeo -V' to\n"
"                           list supported driver(s). <name> is case insensitive\n"
"                           and can be shortened to the first unique name.\n"
"                           eg '-d j' for jack, '-d alsa-r' for alsa-raw\n"
" -f <val>, --screen-fps <val>\n"
"                           Desired refresh rate of the video display in frames\n"
"                           If this value is equal or less than zero, xjadeo\n"
"                           will use the FPS of the video file as its update\n"
"                           frequency (which is the default).\n"
"                           Note: This does not affect screen/vblank sync.\n"
"                           Synchronizing to the screen's vertical refresh is\n"
"                           hardware dependent (and always used if available).\n"
/*-------------------------------------------------------------------------------|" */
" -h, --help                Display this help and exit.\n"
" -I, --ignore-file-offset\n"
"                           This option is only useful for video files with a\n"
"                           start offset, such as split vob files.\n"
"                           Per default xjadeo honors offsets specified in the\n"
"                           video file header. This option allows one to\n"
"                           override (and subtract) this offset to align the\n"
"                           start of the file with timecode 00:00:00:00.\n"
" -i <int>, --info <int>    Display time information using the OSD (on screen \n"
"                           display).\n"
"                           0:Off, %d: Frame number, %d: Timecode, %d: both.\n"
"", OSD_FRAME,OSD_SMPTE,OSD_FRAME|OSD_SMPTE); // :)
  printf ("" /* take a breath */
" -J, --no-initial-sync     Do not connect to JACK, nor use any other sync\n"
"                           source at application start.\n"
" -K <int>, --keyframe-limit <int>\n"
"                           Specify a maximum key-frame interval limit.\n"
"                           With most video codecs, a video frame is the sum\n"
"                           of a key-frame plus differences from the closest\n"
"                           key-frame.\n"
"                           For non continuous playback and random seeks\n"
"                           xjadeo will have to go back to a key-frame and\n"
"                           decode sequentially to the target frame.\n"
"                           This can be quite CPU intense and hence the max.\n"
"                           sequential decoding sequence is limited. By\n"
"                           default to 100 frames\n"
"                           For fast frame accurate seeks, it is highly\n"
"                           recommended to transcode the video file using a\n"
"                           codec where every frame is a key-frame (eg. mjpeg),\n"
"                           and the video consists only of \"intra\" frames.\n"
"                           (the key-frame interval is sometimes also called\n"
"                           \"group of pictures\"s or GOP).\n"
/*-------------------------------------------------------------------------------|" */
" -l, --ltc                 Sync to Linear Time Code (audio jack).\n"
" -M <int>, --midifps <int>\n"
"                           Specify MTC conversion mode:\n"
"\n"
"                           0:  use frame-rate from MTC clock (default)\n"
"                           1:  use video file's fps\n"
"                           2:  \"resample\" file's fps / MTC \n"
" -m <port>, --midi <port>\n"
"                           Use MTC as sync source\n"
"                           The <port> argument is midi driver specific:\n"
"\n"
"                           jack-midi:  specify a midi port name to connect to,\n"
"                             or \"\" to not auto connect.\n"
"                           alsa-seq:  specify ID to connect to (-1: none)\n"
"                             eg. -m ardour or -m 80 \n"
"                           alsa-raw:  specify hardware device\n"
"                             eg. -m hw:1,0 or -m 1 \n"
"                           portmidi:  numeric id; -1: autodetect\n"
"                             a value > -1 specifies the port number to use.\n"
"                             use '-v -m -1' to list midi ports.\n"
#if 0 // - undocumented /hidden/ options
" -N , --drop-frames        Force the SMPTE converter to use the drop frames\n"
"                           algorithm. (Frame dropping is only useful in \n"
"                           combination with a 29fps MIDI time source.\n"
"                           MTC in 29.97 frame drop format is automatically\n"
"                           detected and it is illegal to use this algorithm\n"
"                           for other frame-rates.) DO NOT USE THIS OPTION,\n"
"                           unless you really know what you are doing.\n"
" -n , --no-drop-frames     Parse MTC as announced, but do not use frame drop\n"
"                           algorithm for OSD - useful for debugging\n"
#endif
/*-------------------------------------------------------------------------------|" */
" -O <port>, --osc <port>   Listen for OSC messages on the given port.\n"
"                           Xjadeo can be remote controlled using Open Sound\n"
"                           Control. For a list of available commands, please\n"
"                           see the source code or online documentation.\n"
" -o <val>, --offset <val>\n"
"                           Time offset video from timecode source.\n"
"                           This allows to offset the video playback a certain\n"
"                           number of (video) frames relative to the time\n"
"                           source. Positive and negative values are permitted.\n"
"                           The offset van be specified either as integer frame\n"
"                           number of as colon separated timecode.\n"
#if 0
" -P , --genpts             This option passed on to ffmpeg. It can be used to\n"
"                           generate "presentation timestamps" if they're\n"
"                           missing or invalid in a given file.\n"
"                           Note that this may requires parsing future frames\n"
"                           and impact performance\n"
#endif
" --osc-doc                 Print available OSC commands and exit.\n"
" -Q, --mq                  Enable POSIX realtime message queues.\n"
"                           This sets up a communication channel for remote,\n"
"                           intended to be used with `xjremote`.\n"
" -q, --quiet, --silent     inhibit usual output.\n"
" -R, --remote              Enable interactive remote control mode\n"
"                           using standard I/O. This option implies non-verbose\n"
"                           and quiet as the terminal is used for interaction.\n"
" -r <file>, --rc <file>    Specify a custom configuration file to load.\n"
" -S, --no-splash           Skip the on screen display startup sequence.\n"
" -s, --fullscreen          Start xjadeo in full screen mode.\n"
" -T <file>, --ttf-file <file>\n"
"                           path to .ttf font for on screen display\n"
" -U, --uuid                specify JACK SESSION UUID.\n"
" -V, --version             Print version information and exit.\n"
" -W <rpc-id>, --ipc <rpc-id>\n"
"                           Setup IPC message queues for remote control\n"
"                           Inter Process Communication is used by `xjremote`\n"
"                           on OSX and other platforms that do not support\n"
"                           realtime message queues\n"
" -v, --verbose             print more information\n"
" -x <int>, --vo <int>, --videomode <int>\n"
"                           Select a video output mode (default: 0: autodetect)\n"
"                           A value of -1 lists the available mode and exits\n"
"\n");

  printf (""
"Synchronization Sources:\n"
" JACK:  JACK transport\n"
" LTC:  Linear/Longitudinal Time Code - via JACK audio\n"
" MTC:  MIDI Time Code via JACK MIDI\n"
" MTC:  MIDI Time Code via ALSA sequencer (Linux only)\n"
" MTC:  MIDI Time Code via ALSA raw devices (Linux only)\n"
" MTC:  MIDI Time Code via portmidi (OSX, Windows)\n"
" Manual:  Remote controlled manual seeks.\n"
"\n"
"If neither -m nor -l options are given, xjadeo synchronizes to jack transport\n"
"by default.\n"
"\n");
/*-------------------------------------------------------------------------------|" */
  printf (""
"Video Codecs and Formats:\n"
"Xjadeo uses ffmpeg to decode video files, so a wide range of formats and codecs\n"
"are supported. Note however that not all the codecs support reliable seeking.\n"
"It is highly recommended to transcode the video file into a suitable\n"
"format/codec. The recommend combination is avi/mjpeg.\n"
"e.g. ffmpeg -i input-file.xxx -an -vcodec mjpeg output-file.avi\n"
"This creates from your input-file.xxx an AVI mjpeg encoded video file without\n"
"sound, and no compression between frames (motion jpeg - every frame is a\n"
"key-frame). You may want also to shrink the size of the file by scaling down\n"
"its geometry. This uses fewer system resources for decoding and display and\n"
"leaves more space on the screen for your audio software.\n"
"see ffmpeg -s <width>x<height> option and read up on the ffmpeg man page\n"
"for further options. e.g. -qscale 0 to retain image quality.\n"
"\n");
/*-------------------------------------------------------------------------------|" */
  printf (""
"Configuration Files:\n"
"At startup xjadeo reads the following resource configuration files in the\n"
"following order:\n"
" system wide:       /etc/xjadeorc or /usr/local/etc/xjadeorc\n"
" old user config:   $HOME/.xjadeorc\n"
" user config:       $XDM_CONFIG_HOME/xjadeo/xjadeorc (usually $HOME/.config/)\n"
" on OSX:            $HOME/Library/Preferences/xjadeorc\n"
" on Windows:        $HOMEDRIVE$HOMEPATH\\xjadeorc\n"
"                and $HOMEDRIVE$HOMEPATH\\Local Settings\\xjadeorc\n"
"                    (usually C:\\Documents and Settings\\<Username>\\)\n"
" project specific:  $PWD/xjadeorc\n"
"Every line in the configuration file is a KEY=VALUE pair. If the first\n"
"character on a line is either is a literal '#' or ';', the line is ignored.\n"
"KEYS are case insensitive. Boolean values are specified as 'yes' or 'no'.\n"
"As for a list of available keys, please see the example configuration file,\n"
"which is available in the documentation folder of the source code.\n"
"\n"
"If xjadeo is compiled with jack session support, it will save its current\n"
"state as config file and pass it as handle to the jack session manager.\n"
"\n");
/*-------------------------------------------------------------------------------|" */
  printf (""
"User Interaction:\n"
"The xjadeo window offers a right-click context menu (except on OSX where the\n"
"application has a main menu bar) which provides easy access to common\n"
"functionality.\n"
"On OSX and Windows this menu offers a file open dialog to change the video file\n"
"that is being monitored. On Linux new files can be loaded by dragging the file\n"
"onto the window itself.\n"
"In addition xjadeo reacts to key presses. The following shortcuts are defined:\n"
" 'Esc'        Close window and quit\n"
" 'a'          Toggle always-on-top mode\n"
" 'b'          Toggle On Screen Display black border\n"
" 'Shift+C'    Clear all OSD display messages.\n"
" 'f'          Toggle full screen mode\n"
" 'g'          Toggle On Screen file geometry display\n"
" 'i'          Toggle On Screen file time info display\n"
" 'l'          Toggle letterbox scaling\n"
" 'm'          Toggle mouse-cursor visibility\n"
" 'o'          Cycle though offset display modes.\n"
" 'p'          Swap OSD timecode/frame number position.\n"
" 's'          Toggle On Screen sync source timecode display\n"
" 'v'          Cycle On Screen VTS/frame number display\n"
" 'x'          Toggle seek-bar (experimental)\n"
" '\\'          Reset timecode offset to zero\n"
" '+'          Increase timecode offset by one frame\n"
" '-'          Decrease timecode offset by one frame\n"
" '{'          Decrement timecode offset by one minute\n"
" '}'          Increment timecode offset by one minute\n"
" ','          Resize window to match aspect ratio\n"
" '.'          Resize window to original video size\n"
" '<'          Decrease window size by 20%%\n"
" '>'          Increase window size by 20%%\n"
" 'backspace'  Return jack transport to 00:00:00:00\n"
" 'space'      Toggle jack transport play/pause\n"
#ifdef CROPIMG
" '['          Shift cropped image 2px to the left.\n"
" ']'          Shift cropped image 2px to the right.\n"
#endif
#if 1
" 'e'          Show color equalizer (x11/imlib and XV only)\n"
" 'Shift+E'    Reset color equalizer (x11/imlib and XV only)\n"
" '0-9'        Change color equalization (x11/imlib and XV only)\n"
" 'Shift+1-4'  Fine tune color equalization (x11/imlib and XV only)\n"
"              brightness:1+2, contrast:3+4, gamma:5+6, saturation:7+8\n"
"              hue:9+0. XV color balance is hardware dependant.\n"
"\n"
"Note that it is possible to disable certain features using the remote control.\n"
"A Host can take control of certain aspects e.g. offset or disallow closing the\n"
"video monitor, except by host-provided means.\n"
#endif
/*-------------------------------------------------------------------------------|" */
"\n"
"Report bugs at <https://github.com/x42/xjadeo/issues>\n"
"Website: <http://xjadeo.sf.net/>\n"
);
  exit (status);
}

static void printversion (void) {
	printf ("xjadeo version %s\n\n", VERSION);

#ifdef SUBVERSION
	if (strlen(SUBVERSION)>0 && strcmp(SUBVERSION, "exported")) {
		printf (" built from:    scm-%s\n", SUBVERSION);
	}
#endif
	printf (" compiled with: AVFORMAT=0x%x AVCODEC=0x%x AVUTIL:0x%x\n",
			LIBAVFORMAT_VERSION_INT, LIBAVCODEC_VERSION_INT, LIBAVUTIL_VERSION_INT);
	printf (" configuration: [ %s]\n", cfg_features);
	printf (" MTC/MIDI:      [ %s]\n", cfg_midi);
	printf (" Display(s):    [ %s]\n", cfg_displays);
	printf ("\n"
			"Copyright (C) GPL 2006-2014 Robin Gareus <robin@gareus.org>\n"
			"Copyright (C) GPL 2006-2011 Luis Garrido <luisgarrido@users.sourceforge.net>\n"
			"This is free software; see the source for copying conditions.  There is NO\n"
			"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
}

static int stat_osd_fontfile(void) {
#ifdef HAVE_FT
	struct stat s;
	if (stat(OSD_fontfile, &s) == 0) {
		if (want_verbose)
			fprintf(stdout,"OSD font file: %s\n", OSD_fontfile);
		return 0;
	} else {
		if (want_verbose)
			fprintf(stdout,"font: '%s' was not found.\n", OSD_fontfile);
		return 1;
	}
#else
	return 0;
#endif
}


//--------------------------------------------
// Main
//--------------------------------------------

static void clean_up (int status) {
	if(remote_en) close_remote_ctrl();
#ifdef HAVE_MQ
	if(mq_en) close_mq_ctrl();
#elif defined HAVE_IPCMSG
	if(ipc_queue) {
		close_ipcmsg_ctrl();
		free(ipc_queue);
	}
#endif
	xjosc_shutdown();

	close_window();

	close_movie();

#ifdef HAVE_MIDI
	if (midi_driver) free(midi_driver);
	if (midi_connected()) midi_close();
	else
#endif
#ifdef HAVE_LTC
		if (ltcjack_connected()) close_ltcjack();
		else
#endif
			close_jack();
	free_freetype();

	if (smpte_offset) free(smpte_offset);
	if (load_rc) free(load_rc);
	if (load_movie) free(load_movie);

	x_fib_save_recent (x_fib_recent_file ("xjadeo"));
	x_fib_free_recent ();

	if (!want_quiet)
		fprintf(stdout, "\nBye!\n");
	exit(status);
}

void catchsig (int sig) {
#ifndef PLATFORM_WINDOWS
	signal(SIGHUP, catchsig); /* reset signal */
	signal(SIGINT, catchsig);
#endif
	if (!want_quiet)
		fprintf(stdout,"\n CAUGHT SIGINT - shutting down.\n");
	loop_flag=0;
	clean_up(1);
	exit(1);
}

static void *xjadeo (void *arg) {

	open_window();

	// try fallbacks if window open failed in autodetect mode
	if (videomode==0 && getvidmode() == VO_AUTO) { // re-use cmd-option variable as counter.
		if (want_verbose) printf("trying video driver fallbacks.\n");
		while (getvidmode() == VO_AUTO) { // check if window is open.
			videomode++;
			int tv=try_next_vidoutmode(videomode);
			if (tv<0) break; // no videomode found!
			if (want_verbose) printf("trying videomode: %i: %s\n",videomode,vidoutname(videomode));
			if (tv==0) continue; // this mode is not available
			render_fmt = vidoutmode(videomode);
			open_window();
		}
	}

	if (getvidmode() == VO_AUTO) {
		fprintf(stderr,"Could not open display.\n");
		if(!remote_en) { // && !mq_en && !ipc_queue) /* TODO: allow windowless startup with MQ ?! */
#ifdef HAVE_MIDI
			if (midi_driver) free(midi_driver);
			if (midi_connected()) midi_close();
			else
#endif
#ifdef HAVE_LTC
				if (ltcjack_connected()) close_ltcjack();
				else
#endif
					close_jack();
			exit(1);
		}
	}

	/* setup sync source */
#ifdef HAVE_MIDI
	midi_choose_driver(midi_driver);

#ifdef JACK_SESSION
	if (jack_uuid && !strcmp(midi_driver_name(), "JACK-MIDI")) {
		// don't auto-connect jack-midi on session restore.
		if (atoi(midiid) == 0) midiid[0]='\0';
	}
#endif
#endif

	if (no_initial_sync
#ifdef JACK_SESSION
			&& !jack_uuid
#endif
		 ) {
		if (!(remote_en || mq_en || ipc_queue || osc_port)) {
			fprintf(stderr,
					"Warning: There is no Initial sync-source, and no remote-control enbled to\n"
					"change the sync source. Do not use '-J' option (unless you're testing).\n");
		}
	}
#ifdef HAVE_MIDI
	else if (atoi(midiid) >= -1 ) {
		if (!want_quiet)
			printf("using MTC as sync-source.\n");
		midi_open(midiid);
	} else
#endif
#ifdef HAVE_LTC
		if (use_ltc) {
			if (!want_quiet)
				printf("using LTC as sync source.\n");
			open_ltcjack(NULL);
		} else
#endif
			if (use_jack) {
				if (!want_quiet)
					printf("using JACK-transport as sync source.\n");
				open_jack();
			}
	if (!no_initial_sync) {
		jack_autostart = 1;
	}

#ifdef HAVE_MQ
	if(mq_en) open_mq_ctrl();
#elif defined HAVE_IPCMSG
	if(ipc_queue) open_ipcmsg_ctrl(ipc_queue);
#endif
	if(remote_en) open_remote_ctrl();

	/* MAIN LOOP */
	event_loop();

#ifdef PLATFORM_OSX
	osx_shutdown();
#endif

	return NULL;
}


#if defined PLATFORM_WINDOWS && defined USE_WINMAIN

#ifdef HAVE_SDL
#include <SDL/SDL.h>
#endif

int xjadeo_main (int argc, char **argv);

#include <fcntl.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int nCmdShow) {

	_fmode = O_BINARY;

	LPWSTR *szArglist;
	int nArgs;
	int i;
	int argc = 0;
	char **argv = NULL;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (NULL == szArglist) {
		return -1;
	}
	argv = alloca(sizeof(char*) * (nArgs + 2));

	if (!argv) {
		return -1;
	}

	for (i = 0; i < nArgs; ++i) {
		int argChars = wcslen (szArglist[i]);
		argv[argc] = alloca(sizeof(char) * (argChars + 1));
		if (!argv[argc]) continue;
		wcstombs (argv[argc++], szArglist[i], argChars + 1);
	}
	argv[argc] = 0;

#ifdef HAVE_SDL
	SDL_SetModuleHandle(hInst);
#endif

	return xjadeo_main (argc, argv);
}

int xjadeo_main (int argc, char **argv)
#else
int main (int argc, char **argv)
#endif
{
	int i;
	char *movie;

	xjadeorc(); // read config files - default values before parsing cmd line.

#ifdef PLATFORM_OSX
	if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 ) {
		char **gArgv = (char **) malloc(sizeof (char *) * argc);
		int i;
		gArgv[0] = argv[0];
		for (i=1;i<argc-1;++i) {
			gArgv[i] = argv[i+1];
		}
		--argc;
		argv=gArgv;
	}
#endif

	i = decode_switches (argc, argv);

	if (init_weak_jack()) {
		if (!want_quiet)
			fprintf(stderr, "Failed load JACK shared library.\n");
	}

	if (videomode < 0) vidoutmode(videomode); // dump modes and exit.

	x_fib_load_recent (x_fib_recent_file ("xjadeo"));

	if ((i+1)== argc) movie = argv[i];
	else movie = "";
	// NB without WINMENU or XDND as well with SDL
	// we should exit if no remote-ctrl was enabled
	// and no file-name is given. Oh, well.

	if (load_rc) {
		if (testfile(load_rc)) readconfig(load_rc);
		if (load_movie) movie = load_movie;
	}

	if (want_verbose) printf ("xjadeo %s\n", VERSION);

	if (stat_osd_fontfile()
#ifdef PLATFORM_WINDOWS
			// we need an absolute path including drive-letter
			// otherwise drive changes (video file on a different partition)
			// will screw things up on windows.
			|| !strchr(OSD_fontfile, ':')
#endif
		 )
	{
#ifdef PLATFORM_WINDOWS
		HKEY key;
		DWORD size = PATH_MAX;
		char path[PATH_MAX+1];
		if ( (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\RSS\\xjadeo", 0, KEY_READ, &key))
				&&  (ERROR_SUCCESS == RegQueryValueExA (key, "Install_Dir", 0, NULL, (LPBYTE)path, &size))
			 )
		{
			strcpy(OSD_fontfile, path);
			strcat(OSD_fontfile, "\\" FONT_FILE);
		} else {
			GetModuleFileNameW(NULL, (LPWCH)path, MAX_PATH);
			char *tmp;
			if ((tmp = strrchr(path, '\\'))) *tmp = 0;
			strcpy(OSD_fontfile, path);
			strcat(OSD_fontfile, "\\" FONT_FILE);
		}
		if (stat_osd_fontfile())
#endif
		{
			OSD_fontfile[0] = '\0';
#ifdef WITH_EMBEDDED_FONT
			if (want_verbose) {
				fprintf(stderr,"TTF font file was not found. Using built-in font for OSD.\n");
			}
#else
			if (!want_quiet) {
				fprintf(stderr,"no TTF font found. OSD will not be available until you set one.\n");
			}
#endif
		}
	}

	/* do the work */
	avinit();

	// format needs to be set before calling init_moviebuffer
	render_fmt = vidoutmode(videomode);

#ifndef PLATFORM_WINDOWS
	signal (SIGHUP, catchsig);
	signal (SIGINT, catchsig);
#endif

	if (osc_port > 0) xjosc_initialize(osc_port);

	open_movie(movie);
	init_moviebuffer();

#ifdef PLATFORM_OSX
	// Cocoa can only handle UI events in the main thread since
	// various OSX Frameworks hardcode pthread_main_np for their use.
	// Oh well, fight hacks with hacks.
	pthread_t xjadeo_thread;
	if (pthread_create (&xjadeo_thread, NULL, xjadeo, NULL)) {
		return -1;
	}
	osx_main();
	pthread_join (xjadeo_thread, NULL);
#else
	xjadeo(NULL);
#endif

	clean_up(0);
	return (0);
}
