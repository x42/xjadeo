/* 
   xjadeo - jack video monitor

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/
/* Credits:
 * 
 * xjadeo: 
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 *  Robin Gareus <robin@gareus.org>
 *
 * XLib code:
 * http://www.ac3.edu.au/SGI_Developer/books/XLib_PG/sgi_html/index.html
 *
 * WM_DELETE_WINDOW code:
 * http://biology.ncsa.uiuc.edu/library/SGI_bookshelves/SGI_Developer/books/OpenGL_Porting/sgi_html/apf.html
 *
 * ffmpeg code:
 * http://www.inb.uni-luebeck.de/~boehme/using_libavcodec.html
 *
 */

#define EXIT_FAILURE 1

#include "xjadeo.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include <getopt.h>

#include <unistd.h>


//------------------------------------------------
// Globals
//------------------------------------------------

// Display loop

/* int loop_flag: main xjadeo event loop 
 * if loop_flag is set to 0, xjadeo will exit
 */
int       loop_flag = 1; 

/* int loop_run: video update enable
 * if set to 0 no smpte will be polled and 
 * no video frame updates are performed.
 */
int 	  loop_run = 1; 

      
// Video Decoder 

int               movie_width = 100;
int               movie_height = 100;
AVFormatContext   *pFormatCtx;
int               videoStream=-1;
AVCodecContext    *pCodecCtx;
AVFrame           *pFrame;
AVFrame           *pFrameFMT = NULL;
uint8_t           *buffer = NULL;

// needs to be set before calling movie_open
int               render_fmt = PIX_FMT_YUV420P;

/* Video File Info */
double 	duration = 1;
double 	framerate = 1;
long	frames = 1;


/* Option flags and variables */
char	*current_file = NULL;
long	ts_offset = 0;
long	userFrame = 0; // seek to this frame is jack and midi are N/A
long	dispFrame = 0; // global strorage... = (SMPTE+offset) with boundaries to [0..movie_file_frames]

int want_quiet   =0;	/* --quiet, --silent */
int want_verbose =0;	/* --verbose */
int remote_en =0;	/* --remote, -R */
int remote_mode =0;	/* 0: undirectional ; >0: bidir
			 * bitwise enable async-messages 
			 *  so far only: 
			 *   (1) notify changed timecode 
			 */

int try_codec =0;	/* --try-codec */

#ifdef HAVE_MIDI
int midiid = -2;	/* --midi # -1: autodetect -2: jack*/
#endif

double 		delay = 0.1; // default update rate 10 Hz
int		videomode = 0; // autodect

#if LIBAVFORMAT_BUILD > 4622
 int seekflags    = AVSEEK_FLAG_ANY; /* non keyframe */
#else
 int seekflags    = AVSEEK_FLAG_BACKWARD; /* keyframe */
#endif


// On screen display
char OSD_fontfile[1024] = FONT_FILE;
char OSD_text[128] = "xjadeo!";
char OSD_frame[48] = "";
char OSD_smpte[13] = "";
int OSD_mode = 0 ;

int OSD_fx = OSD_CENTER;
int OSD_tx = OSD_CENTER;
int OSD_sx = OSD_CENTER;
int OSD_fy = 95; // percent
int OSD_sy = 5; // percent
int OSD_ty = 50; // percent

/* The name the program was run with, stripped of any leading path. */
char *program_name;


// prototypes .

static void usage (int status);

static struct option const long_options[] =
{
  {"quiet", no_argument, 0, 'q'},
  {"silent", no_argument, 0, 'q'},
  {"verbose", no_argument, 0, 'v'},
  {"keyframes", no_argument, 0, 'k'},
  {"offset", no_argument, 0, 'o'},
  {"fps", required_argument, 0, 'f'},
  {"videomode", required_argument, 0, 'm'},
  {"vo", required_argument, 0, 'x'},
  {"remote", no_argument, 0, 'R'},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"try-codec", no_argument, 0, 't'},
#ifdef HAVE_MIDI
  {"midi", required_argument, 0, 'm'},
#endif
  {NULL, 0, NULL, 0}
};



/* Set all the option flags according to the switches specified.
   Return the index of the first non-option argument.  */
static int
decode_switches (int argc, char **argv)
{
  int c;
  while ((c = getopt_long (argc, argv, 
			   "q"	/* quiet or silent */
			   "v"	/* verbose */
			   "h"	/* help */
			   "R"	/* remote control */
			   "k"	/* keyframes */
			   "o:"	/* offset */
			   "t"	/* try-codec */
			   "f:"	/* fps */
			   "x:"	/* video-mode */
#ifdef HAVE_MIDI
			   "m:"	/* midi interface */
#endif
			   "V",	/* version */
			   long_options, (int *) 0)) != EOF)
    {
      switch (c)
	{
	case 'q':		/* --quiet, --silent */
	  want_quiet = 1;
	  want_verbose = 0;
	  break;
	case 'v':		/* --verbose */
	  want_verbose = !remote_en;
	  break;
	case 'R':		/* --remote */
	  remote_en = 1;
	  want_quiet = 1;
	  want_verbose = 0;
	  break;
	case 't':		/* --try */
	  try_codec = 1;
	  break;
	case 'o':		/* --offset */
	  ts_offset=atoi(optarg);
	  printf("set time offset to %li frames\n",ts_offset);
	  break;
	case 'k':		/* --keyframes */
	  seekflags=AVSEEK_FLAG_BACKWARD;
	  printf("seeking to keyframes only\n");
	  break;
	case 'f':		/* --fps */
          delay = 1.0 / atof(optarg);
	  break;
	case 'x':		/* --vo --videomode */
          videomode = atoi(optarg);
	  break;
#ifdef HAVE_MIDI
	case 'm':		/* --midi */
          midiid = atoi(optarg);
	  break;
#endif
	case 'V':
	  printf ("xjadeo %s\n", VERSION);
  	  printf("compiled with LIBAVFORMAT_BUILD %i\n", LIBAVFORMAT_BUILD);
	  exit (0);

	case 'h':
	  usage (0);

	default:
	  usage (EXIT_FAILURE);
	}
    }
  return optind;
}

static void
usage (int status)
{
  printf ("%s - \
jack video monitor\n", program_name);
  printf ("usage: %s [option]... <video-file>...\n", program_name);
  printf (""
"Options:\n"
"  -q, --quiet, --silent     inhibit usual output\n"
"  -R,                       remote control (stdin) - implies non verbose quiet mode\n"
"  --verbose                 print more information\n"
"  -f <val>, --fps <val>     override default fps.\n"
"  -k, --keyframes           seek to keyframes only\n"
"  -o <int>, --offset <int>  add/subtract <int> video-frames to/from timecode\n"
"  -x <int>, --vo <int>,     set the video output mode (default: 0 - autodetect\n"
"       --videomode <int>    -1 prints a list of available modes.\n"
#ifdef HAVE_MIDI
"  -m <int>, --midi <int>,   use midi instead of jack (-1: autodetect)\n"
"                            value > -1 specifies midi channel\n" 	  
"                            use -v -m -1 to list midi channels during autodetection\n" 	  
#endif
"  -t, --try-codec           checks if the video-file can be played by jadeo.\n"
"                            exits with code 1 if the file is not supported.\n"
"			     no window is opened in this mode.\n"
"  -h, --help                display this help and exit\n"
"  -V, --version             output version information and exit\n"
"  \n"
"  framerate does not need to be integer, and defaults to 10.0 fps\n"
"  Check the docs to learn how the video should be encoded.\n"
);
  exit (status);
}



//--------------------------------------------
// Main
//--------------------------------------------

int
main (int argc, char **argv)
{
  int i;
  char*   movie= NULL;

  program_name = argv[0];

  i = decode_switches (argc, argv);

  render_fmt = vidoutmode(videomode);

  if ((i+1)== argc) movie = argv[i];
  else if (remote_en && i==argc) movie = "";
  else usage (EXIT_FAILURE);

#ifdef HAVE_FT
  // FIXME
  // stat (OSD_fontfile)
  // look for .ttf files in common paths
  //
#endif
    
  /* do the work */
  avinit();

  // only try to seek to frame 1 and decode it.
  if (try_codec) do_try_this_file_and_exit (movie);

#ifdef HAVE_MIDI
  if (midiid < -1 ) {
    open_jack();
  } else {
    if (want_verbose && midiid == -1) midiid = midi_detectdevices(1);
    else if (midiid == -1) midiid = midi_detectdevices(0);
    midi_open(midiid);
  }
#else
  open_jack();
#endif

  if(remote_en) open_remote_ctrl();

  open_movie(movie);
  
  open_window(&argc,&argv);

  init_moviebuffer();

  display_frame(0LL,1);
  
  event_loop();
  
  close_window();
  
  close_movie();

  if(remote_en) close_remote_ctrl();

#ifdef HAVE_MIDI
  if (midiid >=0 ) midi_close(); 
  else
#endif
  close_jack();
  
  if (!want_quiet)
    fprintf(stdout, "\nBye!\n");

  exit (0);
}
