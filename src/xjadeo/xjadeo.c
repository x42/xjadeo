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

#include <jack/jack.h>
#include <jack/transport.h>

#include <time.h>
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


// Jack
extern jack_client_t *jack_client;
extern char jackid[16];


/* The name the program was run with, stripped of any leading path. */
char *program_name;

// TODO: someday we can implement 'mkfifo' or open / pipe
#define REMOTE_RX fileno(stdin) 
#define REMOTE_TX fileno(stdout)

#define REMOTEBUFSIZ 4096
int remote_read(void);
void remote_printf(int val, const char *format, ...);

static void usage (int status);
static int decode_switches (int argc, char **argv);
void display_frame(int64_t ts, int force_update);
int close_movie();

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
// main event loop
//--------------------------------------------
//
void select_sleep (int usec) {
	fd_set fd;
	int max_fd=0;
	struct timeval tv = { 0, 0 };
	tv.tv_sec = 0; tv.tv_usec = usec;

	FD_ZERO(&fd);
	if (remote_en) {
		FD_SET(REMOTE_RX,&fd);
		max_fd=REMOTE_RX+1;
	}

	if (select(max_fd, &fd, NULL, NULL, &tv)) remote_read();
}

void event_loop(void)
{
  double              elapsed_time;
  clock_t             clock1, clock2;
  long                newFrame, offFrame, currentFrame;
  long                nanos;
  struct timespec     ts;
  
  clock1 = clock();
  
  currentFrame = -1;
  
  while(loop_flag)
  {

    if (loop_run==0) { 
      select_sleep(2e5L);
      handle_X_events();
      continue;
    }
    
#ifdef HAVE_MIDI
    if (midiid>=0) newFrame = midi_poll_frame();
    else
#endif
    newFrame = jack_poll_frame();

    if (newFrame <0 ) newFrame=userFrame;

    offFrame = newFrame + ts_offset;

    if(offFrame != currentFrame)
    {
      currentFrame = offFrame;
      display_frame((int64_t)(currentFrame),0);
    }
    if(want_verbose) {
	fprintf(stdout, "frame: smpte:%li\r", newFrame);
    	fflush(stdout); 
    }
    if (remote_en && (remote_mode&1)) {
		//call 	xapi_pposition ?? -> rv:200
		// dispFrame is the currently displayed frame 
		// = SMPTE + offset
		remote_printf(300,"position=%li",dispFrame);
    }


    handle_X_events();

    
    clock2 = clock();
    elapsed_time = ((double) (clock2 - clock1)) / CLOCKS_PER_SEC;
    if(elapsed_time < delay)
    {
      nanos = (long) floor(1e9L * (delay - elapsed_time));
      ts.tv_sec = (nanos / 1000000000L);
      ts.tv_nsec = (nanos % 1000000000L);
      select_sleep(nanos/1000L);
    }

    clock1 = clock2;
  } 

}


//--------------------------------------------
// Manage video file
//--------------------------------------------

void init_moviebuffer(void) {
  
  int     numBytes;
  // Determine required buffer size and allocate buffer
  numBytes=avpicture_get_size(render_fmt, movie_width, movie_height);
  buffer=(uint8_t *) calloc(1,numBytes);

  // Assign appropriate parts of buffer to image planes in pFrameFMT
  if (pFrameFMT)
	  avpicture_fill((AVPicture *)pFrameFMT, buffer, render_fmt, pCodecCtx->width, pCodecCtx->height);
}

// Open video file
char *current_file= NULL;

void avinit (void) {
  av_register_all();
  avcodec_init();
  avcodec_register_all();
}

int open_movie(char* file_name)
{

  int                 i;
  AVCodec             *pCodec;

  if (pFrameFMT) {
	  fprintf(stderr,"replacing current video file buffer\n");
	  // close currently open movie
	  close_movie();
  }

  pFrameFMT = NULL;
  movie_width  = 160;
  movie_height = 90;
  framerate = duration = frames = 1;
  
  // Open video file
  if(av_open_input_file(&pFormatCtx, file_name, NULL, 0, NULL)!=0)
  {
      (void) fprintf( stderr, "Cannot open video file %s\n", file_name);
      return( -1 );
  }

  // Retrieve stream information
  if(av_find_stream_info(pFormatCtx)<0)
  {
      (void) fprintf( stderr, "Cannot find stream information in file %s\n", file_name);
      av_close_input_file(pFormatCtx);
      return( -1 );
  }

  if (!want_quiet) dump_format(pFormatCtx, 0, file_name, 0);

  // Find the first video stream
  for(i=0; i<pFormatCtx->nb_streams; i++)
#if LIBAVFORMAT_BUILD >  4629
      if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO)
#else
      if(pFormatCtx->streams[i]->codec.codec_type==CODEC_TYPE_VIDEO)
#endif
      {
          videoStream=i;
          break;
      }
  if(videoStream==-1)
  {
      fprintf( stderr, "Cannot find a video stream in file %s\n", file_name);
      av_close_input_file(pFormatCtx);
      return( -1 );
  }

  AVStream *av_stream = pFormatCtx->streams[videoStream];
#if LIBAVFORMAT_BUILD > 4628
  framerate = (double) av_stream->r_frame_rate.num / av_stream->r_frame_rate.den;
#else
  framerate = (double) av_stream->r_frame_rate / av_stream->r_frame_rate_base;
#endif

  duration = (double) ( (int64_t) pFormatCtx->duration / (int64_t) AV_TIME_BASE);
  frames = (long) (framerate * duration);
  
  if (!want_quiet) {
    fprintf(stdout, "original frame rate: %g\n", framerate);
    fprintf(stdout, "length in seconds: %g\n", duration);
    fprintf(stdout, "total frames: %ld\n", frames);
  }
  
  // Get a pointer to the codec context for the video stream
#if LIBAVFORMAT_BUILD > 4629
  pCodecCtx=pFormatCtx->streams[videoStream]->codec;
#else
  pCodecCtx=&(pFormatCtx->streams[videoStream]->codec);
#endif
  
  movie_width = pCodecCtx->width;
  movie_height = pCodecCtx->height;

  // Find the decoder for the video stream
  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
  if(pCodec==NULL)
  {
      fprintf( stderr, "Cannot find a codec for %s\n", file_name);
      av_close_input_file(pFormatCtx);
      return( -1 );
  }
    
  // Open codec
  if(avcodec_open(pCodecCtx, pCodec)<0)
  {
      (void) fprintf( stderr, "Cannot open the codec for file %s\n", file_name);
      av_close_input_file(pFormatCtx);
      return( -1 );
  }
  
  pFrame=avcodec_alloc_frame();
  if(pFrame==NULL)
  {
      (void) fprintf( stderr, "Cannot allocate video frame buffer\n");
      avcodec_close(pCodecCtx);
      av_close_input_file(pFormatCtx);
      return(-1);
  }

  // TODO: share this memory with the 'hardware' display framebuffer if possible.
  pFrameFMT=avcodec_alloc_frame();
  if(pFrameFMT==NULL)
  {
      (void) fprintf( stderr, "Cannot allocate display frame buffer\n");
      av_free(pFrame);
      avcodec_close(pCodecCtx);
      av_close_input_file(pFormatCtx);
      return(-1);
  }
  current_file=strdup(file_name);
  return( 0 );
}

int my_seek_frame (AVPacket *packet, int timestamp) 
{
	// check if timestamp >0 && < length
	// esp if there is a ts_offset :)

  if(av_seek_frame(pFormatCtx, videoStream, timestamp, seekflags) >= 0)
  {
   
    avcodec_flush_buffers(pCodecCtx);
  
    // Find a video frame.
    
    read_frame:
    
    if(av_read_frame(pFormatCtx, packet)<0)
    {
	    // remove in quiet mode ??  (only if to stdout!)
      fprintf(stderr, "Reached movie end\n");
      return (0);
    }
    
    if(packet->stream_index!=videoStream)
    {
	    // remove in quiet mode ?? (only if to stdout!)
      fprintf(stderr, "Not a video frame\n");
      av_free_packet(packet);
      goto read_frame;
    }
    return (1);
  }
  return (0);
}

void display_frame(int64_t timestamp, int force_update)
{

  static AVPacket packet;
//   static int      bytesRemaining=0;
  static int      fFirstTime=1;
  int             frameFinished;
  
  if (timestamp < 0) timestamp=0;
  else if(timestamp >= frames) timestamp = frames - 1;

  if (!force_update && dispFrame == timestamp) return;

  if(want_verbose)
	fprintf(stdout, "                        display:%li         \r", (long int) timestamp);

  dispFrame = timestamp;

  if (OSD_mode&OSD_FRAME) snprintf(OSD_frame,48,"Frame: %li", dispFrame);
  if (OSD_mode&OSD_SMPTE) frame_to_smptestring(OSD_smpte,dispFrame);

  // First time we're called, set packet.data to NULL to indicate it
  // doesn't have to be freed
  if(fFirstTime)
  {
      fFirstTime=0;
      packet.data=NULL;
  }

  if (pFrameFMT && my_seek_frame(&packet, timestamp)) {
   
    // Decode video frame
    avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, 
        packet.data, packet.size);
  
    // Did we get a video frame?
    if(frameFinished)
    {
	
	// TODO: include this step in the render function
        // Convert the image from its native format to FMT
        img_convert((AVPicture *)pFrameFMT, render_fmt, 
            (AVPicture*)pFrame, pCodecCtx->pix_fmt, pCodecCtx->width, 
            pCodecCtx->height);

	render_buffer(buffer); // in pFrameFMT

    }
    else  fprintf( stderr, "Frame not finished\n");
  
    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
    
  } else {
	if (pFrameFMT) fprintf( stderr, "Error seeking frame\n");
	
	// clear image (black / or YUV green)
	memset(buffer,0,avpicture_get_size(render_fmt, movie_width, movie_height));
	render_buffer(buffer); // in pFrameFMT
  }
    
}
  
int close_movie()
{
  if(current_file) free(current_file);
  current_file=NULL;

  if (!pFrameFMT) return(-1);

// Free the formatted image
  free(buffer);
  av_free(pFrameFMT);
  pFrameFMT=NULL;
  
  //Free the YUV frame
  av_free(pFrame);
  
  //Close the codec
  avcodec_close(pCodecCtx);
  
  //Close the video file
  av_close_input_file(pFormatCtx);
  return (0);
}

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
	open_window(0,NULL); // else VOutout callback fn will fail.

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

// TODO: allow video output mode switching
//render_fmt = vidoutmode(videomode);
//  init_moviebuffer();
	vidoutmode(videomode); // init VOutput
	open_window(0,NULL);
	remote_printf(100, "window opened.");
}

void xapi_pvideomode(void *d) {
	remote_printf(200,"videomode=%i", getvidmode());
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
	remote_printf(200,"updatefps=%li",(float) 1/delay);
}

void xapi_sfps(void *d) {
	char *off= (char*)d;
        delay = 1.0 / atof(off);
	remote_printf(100,"updatefps=%li",(float) 1/delay);
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
}

void xapi_midi_status(void *d) {
#ifdef HAVE_MIDI
	if (midiid<0)
		remote_printf(100,"midi not connected.");
	else
		remote_printf(200,"midiport=%i",midiid);
#else
	remote_printf(499,"midi not available.");
#endif
}
void xapi_open_midi(void *d) {
#ifdef HAVE_MIDI
	midiid = atoi((char*)d);
	remote_printf(100,"opening midi channel %i.",midiid);
	midi_open(midiid);
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_close_midi(void *d) {
#ifdef HAVE_MIDI
	midi_close();
	midiid=-2; // back to jack
	remote_printf(100,"midi close.");
#else
	remote_printf(499,"midi not available.");
#endif
}

void xapi_detect_midi(void *d) {
#ifdef HAVE_MIDI
	midiid = midi_detectdevices(0);
	midi_open(midiid);
	remote_printf(100,"opening midi channel %i.",midiid);
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
	struct _command *children;
	myfunction *func;
	int sticky; // unused - again - now children always overwrite func (precenance parser)
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

	{"get seekmode", ": returns 1 if decoding keyframes only", NULL, xapi_null, 0 },
	{"set seekmode ", "<1|0>: set to one to seek only keyframes", NULL, xapi_null, 0 },

	{"window close", ": close window", NULL, xapi_close_window, 0 },
	{"window open", ": open window", NULL, xapi_open_window, 0 },
	{"window mode " , "<int>: changes video mode and opens window", NULL, xapi_set_videomode, 0 },
	{"window resize " , "<int>|<int>x<int>: resize window (percent of movie or absolute)", NULL, xapi_swinsize, 0 },
	{"window position " , "<int>x<int>: move window", NULL, xapi_swinpos, 0 },
	{"window pos " , "<int>x<int>: move window", NULL, xapi_swinpos, 0 },

	{"get windowsize" , ": show current window size", NULL, xapi_pwinsize, 0 },
	{"get windowpos" , ": show current window position", NULL, xapi_null, 0 },

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
	{"osd available" , "tells you if freetype OSD is available", NULL, xapi_osd_avail, 0 },
	{"osd font " , "<filename>: use this TTF font file", NULL, xapi_osd_font, 0 },

	{"osd box" , ": forces a black box around the OSD", NULL, xapi_osd_box, 0 },
	{"osd nobox" , ": make OSD backgroung transparent", NULL, xapi_osd_nobox, 0 },

	{"notify frame" , ": enable async frame-update messages", NULL, xapi_bidir_frame, 0 },
	{"notify disable" , ": disable async messages", NULL, xapi_bidir_noframe, 0 },

	{"midi autoconnect", ": discover and connect to midi time source", NULL, xapi_detect_midi, 0 },
	{"midi connect ", "<int>: connect to midi time source", NULL, xapi_open_midi, 0 },
	{"midi disconnect", ": unconect from midi device", NULL, xapi_close_midi, 0 },
	{"midi status", ": show connected midi port", NULL, xapi_midi_status, 0 },

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
	//TODO: strip leading and  trailing whitespaces..
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
	remote_printf(800, "xjadeo - remote ctrl alpha-1 (type 'help<enter>' for info)");
}

void close_remote_ctrl (void) {
	free(inbuf);
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
  if (try_codec) {
    AVPacket packet;
    packet.data=NULL;
    int frameFinished=0;
    
    open_movie(movie);
    init_moviebuffer();
    if (my_seek_frame(&packet, 1)) {
    avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet.data, packet.size);
    }
    close_movie();
    if (!frameFinished) {
      if (!want_quiet)
        printf("sorry. this video codec not supported.\n");
      exit(1);
    }
    if (!want_quiet)
      printf("ok. great codec, dude.\n");
    exit (0);
  }

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
