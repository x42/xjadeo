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
#include "xjadeo.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include <time.h>
#include <getopt.h>

#include <unistd.h>


//------------------------------------------------
// extern Globals (main.c)
//------------------------------------------------
extern int       loop_flag;
extern int 	  loop_run;

extern int               movie_width;
extern int               movie_height;
extern AVFormatContext   *pFormatCtx;
extern int               videoStream;
extern AVCodecContext    *pCodecCtx;
extern AVFrame           *pFrame;
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

extern double 		delay;

extern int seekflags;

// On screen display
extern char OSD_frame[48];
extern char OSD_smpte[13];
extern int OSD_mode;

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
		max_fd=remote_fd_set(&fd);
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
    if (midi_connected()) newFrame = midi_poll_frame();
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
	fprintf(stdout, "\t\t\tdisplay:%li        \r", (long int) timestamp);

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

void do_try_this_file_and_exit(char *movie) {
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
