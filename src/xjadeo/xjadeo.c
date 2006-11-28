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
#include <sys/time.h>
#include <unistd.h>

//------------------------------------------------
// extern Globals (main.c)
//------------------------------------------------
extern int	loop_flag;
extern int 	loop_run;

extern int               movie_width;
extern int               movie_height;
extern AVFormatContext   *pFormatCtx;
extern int               videoStream;
extern AVCodecContext    *pCodecCtx;
extern AVFrame           *pFrame;
extern AVFrame           *pFrameFMT;
extern uint8_t           *buffer;

// needs to be set before calling movie_open
extern int	render_fmt;

/* Video File Info */
extern double	duration;
extern double	framerate;
extern long	frames;
extern double	file_frame_offset;

/* Option flags and variables */
extern char    *current_file;
extern double 	filefps;
extern long	ts_offset;
extern char    *smpte_offset;
extern long	userFrame;
extern long	dispFrame;
extern int 	force_redraw;
extern int	want_quiet;
extern int	want_debug;
extern int	want_verbose;
extern int	want_avverbose;
extern int	want_genpts;
extern int	want_ignstart;
extern int	remote_en;
extern int	remote_mode;
extern int	mq_en;
extern double	delay;
extern int	seekflags;

// On screen display
extern char	OSD_frame[48];
extern char	OSD_smpte[13];
extern int	OSD_mode;

const AVRational c1_Q = { 1, 1 };
double 		tpf = 1.0; /* pts/dts increments per video-frame - cached value */

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

	if (select(max_fd, &fd, NULL, NULL, &tv)) remote_read_io();
#if HAVE_MQ
	if (mq_en) remote_read_mq();
#endif
}

void event_loop(void) {
	double 		elapsed_time;
	struct timeval	clock1, clock2;
	long		newFrame, offFrame;
	long		nanos;
	struct timespec	ts;
	double 		dly;
	static int	splashed = -1;

	if (want_verbose) printf("\nentering video update loop @%.2f fps.\n",delay>0?(1.0/delay):framerate);
	gettimeofday(&clock1, NULL);

	while(loop_flag) { /* MAIN LOOP */

		if (loop_run==0) {  // CHECK: && !force_redraw 
			/* video offline - (eg. window minimized)
			 * do not update frame 
			 */
			select_sleep(2e5L);
			handle_X_events();
			lash_process();
			continue;
		}
    
#ifdef HAVE_MIDI
		if (midi_connected()) newFrame = midi_poll_frame();
		else
#endif
		newFrame = jack_poll_frame();

		if (newFrame <0 ) newFrame=userFrame;

		offFrame = newFrame + ts_offset;

		if ((remote_en||mq_en) && ((remote_mode&1) || ((remote_mode&2)&& offFrame!=dispFrame)) ) {
		/*call 	xapi_pposition ?? -> rv:200
		 * dispFrame is the currently displayed frame 
		 * = SMPTE + offset
		 */
			remote_printf(301,"position=%li",dispFrame);
		}
		display_frame((int64_t)(offFrame), force_redraw);
		force_redraw=0;

		if (splashed) {
			if (splashed == -1) {
				splashed =  5.5/delay;	
				splash(buffer);
			}
			if(!--splashed) force_redraw=1;
		}

		if(want_verbose) {
		#if 0
			fprintf(stdout, "frame: smpte:%li    \r", newFrame);
		#else
			char tempsmpte[15];
			frame_to_smptestring(tempsmpte,newFrame,midi_connected());
			fprintf(stdout, "smpte: %s f:%li\r", tempsmpte,newFrame);
		#endif
			fflush(stdout); 
		}

		handle_X_events();
		lash_process();
		dly = delay>0?delay:(1.0/framerate);
    
		gettimeofday(&clock2, NULL);
		elapsed_time = ((double) (clock2.tv_sec-clock1.tv_sec)) + ((double) (clock2.tv_usec-clock1.tv_usec)) / 1000000.0;
		if(elapsed_time < dly) {
			nanos = (long) floor(1e9L * (dly - elapsed_time));
			ts.tv_sec = (nanos / 1000000000L);
			ts.tv_nsec = (nanos % 1000000000L);
			select_sleep(nanos/1000L);
		}
		clock1.tv_sec=clock2.tv_sec;
		clock1.tv_usec=clock2.tv_usec;
	} 
}

//--------------------------------------------
// Manage video file
//--------------------------------------------

void init_moviebuffer(void) {
  
	int     numBytes;
	if (buffer) free(buffer);
	if (want_debug)
		printf("DEBUG: init_moviebuffer - render_fmt: %i\n",render_fmt);
	/* Determine required buffer size and allocate buffer */
	numBytes=avpicture_get_size(render_fmt, movie_width, movie_height);
	buffer=(uint8_t *) calloc(1,numBytes);

// Assign appropriate parts of buffer to image planes in pFrameFMT
	if (pFrameFMT)
		avpicture_fill((AVPicture *)pFrameFMT, buffer, render_fmt, pCodecCtx->width, pCodecCtx->height);
}

/* Open video file */

void avinit (void) {
	av_register_all();
	avcodec_init();
	avcodec_register_all();
	if(!want_avverbose) av_log_set_level(AV_LOG_QUIET);
}

int open_movie(char* file_name) {
	int		i;
	AVCodec		*pCodec;
	AVStream	*av_stream;

	if (pFrameFMT) {
		/* close currently open movie */
		//fprintf(stderr,"replacing current video file buffer\n");
		close_movie();
	}

	pFrameFMT = NULL;
	movie_width  = 320;
	movie_height = 180;
	framerate = duration = frames = 1;
	file_frame_offset = 0.0;
	videoStream=-1;
	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset,midi_connected());
  
	/* Open video file */
	if(av_open_input_file(&pFormatCtx, file_name, NULL, 0, NULL)!=0) {
		if (!remote_en || !mq_en) //TODO prevent msg only when starting up with no file...
			fprintf( stderr, "Cannot open video file %s\n", file_name);
		return (-1);
	}

	/* Retrieve stream information */
	if(av_find_stream_info(pFormatCtx)<0) {
		fprintf( stderr, "Cannot find stream information in file %s\n", file_name);
		av_close_input_file(pFormatCtx);
		return (-1);
	}

	if (!want_quiet) dump_format(pFormatCtx, 0, file_name, 0);

	/* Find the first video stream */
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

	if(videoStream==-1) {
		fprintf( stderr, "Cannot find a video stream in file %s\n", file_name);
		av_close_input_file(pFormatCtx);
		return( -1 );
	}

	av_stream = pFormatCtx->streams[videoStream];

// At LIBAVFORMAT_BUILD==4624 r_frame_rate becomes an AVRational. Before it was an int.
	if (filefps >0 ) framerate=filefps;
#if LIBAVFORMAT_BUILD <= 4616
	else framerate = (double) av_stream->codec.frame_rate / (double) av_stream->codec.frame_rate_base;
#elif LIBAVFORMAT_BUILD <= 4623 // I'm not sure that this is correct:
	else framerate = (double) av_stream->r_frame_rate / (double) av_stream->r_frame_rate_base;
#else
	else if(av_stream->r_frame_rate.den && av_stream->r_frame_rate.num) {
		framerate = av_q2d(av_stream->r_frame_rate);
		if ((framerate < 4 || framerate > 100 ) && (av_stream->time_base.num && av_stream->time_base.den))
			framerate = 1.0/av_q2d(av_stream->time_base);
	}
	else framerate = 1.0/av_q2d(av_stream->time_base);
#endif

	// TODO: detect drop frame timecode !

#if defined(__BIG_ENDIAN__) && (__ppc__) 
// this cast is weird, but it works.. the bytes seem to be in 'correct' order, but the two
// 4byte-words are swapped. ?!
// I wonder how this behaves on a 64bit arch 
// - maybe it's bug in ffmpeg or all video files I tried had a bad header :D
	int64_t dur = (int64_t) (pFormatCtx->duration);
	duration = ( ((double) (((dur&0xffffffff)<<32)|((dur>>32)&0xffffffff))) / (double) AV_TIME_BASE );
#else
//	duration = (double) (((double) (pFormatCtx->duration - pFormatCtx->start_time))/ (double) AV_TIME_BASE);
	duration = (double) (((double) (pFormatCtx->duration))/ (double) AV_TIME_BASE);
#endif
	frames = (long) (framerate * duration);
	tpf = 1.0/(av_q2d(pFormatCtx->streams[videoStream]->time_base)*framerate);
	if (!want_ignstart) 
		file_frame_offset = (double) framerate*(pFormatCtx->start_time/ (double) AV_TIME_BASE);

	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset,midi_connected());

	if (!want_quiet) {
		if (filefps >0 ) 
			fprintf(stdout, "overridden frame rate: %g\n", framerate);
		else 
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

// somewhere around LIBAVFORMAT_BUILD  4630 
#ifdef AVFMT_FLAG_GENPTS
	if (want_genpts)
		pFormatCtx->flags|=AVFMT_FLAG_GENPTS;
	//pFormatCtx->flags|=AVFMT_FLAG_IGNIDX;
#endif

	if (!want_quiet) {
		fprintf( stderr, "movie size:  %ix%i px\n", movie_width,movie_height);
	}
  // Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf( stderr, "Cannot find a codec for file: %s\n", file_name);
		av_close_input_file(pFormatCtx);
		return( -1 );
	}
    
  // Open codec
	if(avcodec_open(pCodecCtx, pCodec)<0) {
		fprintf( stderr, "Cannot open the codec for file %s\n", file_name);
		av_close_input_file(pFormatCtx);
		return( -1 );
	}
  
	pFrame=avcodec_alloc_frame();
	if(pFrame==NULL) {
		fprintf( stderr, "Cannot allocate video frame buffer\n");
		avcodec_close(pCodecCtx);
		av_close_input_file(pFormatCtx);
		return(-1);
	}

	pFrameFMT=avcodec_alloc_frame();
	if(pFrameFMT==NULL) {
		fprintf( stderr, "Cannot allocate display frame buffer\n");
		av_free(pFrame);
		avcodec_close(pCodecCtx);
		av_close_input_file(pFormatCtx);
		return(-1);
	}

	current_file=strdup(file_name);
	return( 0 );
}


void override_fps (double fps) {
	if (fps <= 0) return;

	framerate =  fps;
	frames = (long) (framerate * duration); 
	tpf = 1.0/(av_q2d(pFormatCtx->streams[videoStream]->time_base)*framerate);
	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset,midi_connected());
}

int64_t my_avprev = 0; // last recent seeked timestamp

void render_empty_frame(void) {
	// clear image (black / or YUV green)
	memset(buffer,0,avpicture_get_size(render_fmt, movie_width, movie_height));
#ifdef DRAW_CROSS
	int x,y;
	if (render_fmt == PIX_FMT_YUV420P) 
	for (x=0,y=0;x< movie_width-1; x++,y= movie_height*x/movie_width) {
		 int yoff=(x+movie_width*y);
		 int uvoff=((x/2)+movie_width/2*(y/2));
		 buffer[yoff]=127; buffer[yoff+1]=127; 

		 yoff=(x+movie_width*(movie_height-y-1));
		 uvoff=((x/2)+movie_width/2*((movie_height-y-1)/2));
		 buffer[yoff]=127;
		 buffer[yoff+1]=127;
	}
	if (render_fmt == PIX_FMT_RGB24) 
	for (x=0,y=0;x< movie_width-1; x++,y= movie_height*x/movie_width) {
		int yoff=3*(x+movie_width*y);
		buffer[yoff]=255;
		buffer[yoff+1]=255;
		buffer[yoff+2]=255;
		yoff=3*(x+movie_width*(movie_height-y-1));
		buffer[yoff]=255;
		buffer[yoff+1]=255;
		buffer[yoff+2]=255;
	}
	if (render_fmt == PIX_FMT_RGBA32) 
	for (x=0,y=0;x< movie_width-1; x++,y= movie_height*x/movie_width) {
		int yoff=4*(x+movie_width*y);
		buffer[yoff]=255;
		buffer[yoff+1]=255;
		buffer[yoff+2]=255;
		buffer[yoff+3]=255;
		yoff=4*(x+movie_width*(movie_height-y-1));
		buffer[yoff]=255;
		buffer[yoff+1]=255;
		buffer[yoff+2]=255;
		buffer[yoff+3]=255;
	}
#endif
	render_buffer(buffer); 
}

void reset_video_head(AVPacket *packet) {
	int             frameFinished=0;
#if LIBAVFORMAT_BUILD < 4617
	av_seek_frame(pFormatCtx, videoStream, 0);
#else
	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
#endif
	avcodec_flush_buffers(pCodecCtx);

	while (!frameFinished) {
		av_read_frame(pFormatCtx, packet);
		if(packet->stream_index==videoStream) 
		avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
		if(packet->data) av_free_packet(packet);
	}
	my_avprev=dispFrame=0;
}

// TODO: set this high (>1000) if transport stopped and to a low value (<100) if transport is running.
#define MAX_CONT_FRAMES (500)

int my_seek_frame (AVPacket *packet, int64_t timestamp) {
	AVStream *v_stream;
	int rv=1;
	int nolivelock = 0;
	int64_t mtsb = 0;
	static int ffdebug = 0;

	if (videoStream < 0) return (0); 
	v_stream = pFormatCtx->streams[videoStream];
	
	if (want_ignstart)  // timestamps in the file start counting at ..->start_time 
		timestamp+= (long int) ( framerate*(pFormatCtx->start_time/ (double) AV_TIME_BASE));

	// TODO: assert  0 < timestamp + ts_offset - (..->start_time)   < length   
#if LIBAVFORMAT_BUILD < 4617
	rv= av_seek_frame(pFormatCtx, videoStream, timestamp / framerate * 1000000LL); 
#else
	
# ifdef FFDEBUG
	printf("\nDEBUG: want frame=%li  ", (long int) timestamp);
# endif

# if 1  // TODO -> -F <ratio>
	timestamp*=tpf;
# else	// THIS is eqivalent - or even better at rounding but
	// does not work with -F <double> 
	timestamp=av_rescale_q(timestamp,c1_Q,v_stream->time_base); 
	timestamp=av_rescale_q(timestamp,c1_Q,v_stream->r_frame_rate); //< timestamp/=framerate; 
# endif

# ifdef FFDEBUG
	printf("ts=%li   ##\n", (long int) timestamp);
# endif

	if (seekflags==SEEK_ANY) { 
		rv= av_seek_frame(pFormatCtx, videoStream, timestamp, AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD) ;
		avcodec_flush_buffers(pCodecCtx);
	} else if (seekflags==SEEK_KEY) { 
		rv= av_seek_frame(pFormatCtx, videoStream, timestamp, AVSEEK_FLAG_BACKWARD) ;
		avcodec_flush_buffers(pCodecCtx);
	} else /* SEEK_CONTINUOUS */ if (my_avprev >= timestamp || ((my_avprev + 32*tpf) < timestamp) ) { 
		// NOTE: only seek if last-frame is less then 32 frames behind 
		// else read continuously until we get there :D
		// FIXME 32: use keyframes interval of video file or cmd-line-arg as threshold.
		// TODO: do we know if there is a keyframe inbetween now (my_avprev)
		// and the frame to seek to?? - if so rather seek to that frame than read until then.
		// and if no keyframe in between my_avprev and ts - prevent backwards seeks even if 
		// timestamp-my_avprev > threshold! - Oh well.

		// seek to keyframe *BEFORE* this frame
		// printf("SEEK!\n");
		rv= av_seek_frame(pFormatCtx, videoStream, timestamp, AVSEEK_FLAG_BACKWARD) ;
		avcodec_flush_buffers(pCodecCtx);
	} 
#endif 

	my_avprev = timestamp;
	if (rv < 0) return (0); // seek failed.

read_frame:
	nolivelock++;
	if(av_read_frame(pFormatCtx, packet)<0) {
		if (!want_quiet) printf("Reached movie end\n");
		return (0);
	}
#if LIBAVFORMAT_BUILD >=4616
	if (av_dup_packet(packet) < 0) {
		printf("can not allocate packet\n");
		goto read_frame;
	}
#endif
	if(packet->stream_index!=videoStream) {
//		fprintf(stderr, "Not a video frame\n");
		if (packet->data)
			av_free_packet(packet);
		goto read_frame;
	}
  /* backwards compatible - no cont. seeking (seekmode ANY or KEY ; cmd-arg: -K, -k)
   * do we want a AVSEEK_FLAG_ANY + SEEK_CONTINUOUS option ?? not now.  */
#if LIBAVFORMAT_BUILD < 4617
	return (1);
#endif

	if (seekflags!=SEEK_CONTINUOUS) return (1);

#ifdef FFDEBUG
	printf("\nDEBUG: got pts=%li dts:%li frame:%g/%g    ##\n", 
		(long int) packet->pts , (long int) packet->dts,
		packet->dts*av_q2d(v_stream->time_base)*framerate,
		packet->pts*av_q2d(v_stream->time_base)*framerate);
#endif
	mtsb = packet->pts;  

	if (mtsb == AV_NOPTS_VALUE) { 
		mtsb = packet->dts;
		if (ffdebug==0) { ffdebug=1; if (!want_quiet) fprintf(stderr,"WARNING: video file does not report pts information.\n         resorting to ffmpeg decompression timestamps.\n         consider to transcode the file or use the --genpts option.\n"); }
	}

	if (mtsb == AV_NOPTS_VALUE) { 
		if (ffdebug<2) { ffdebug=2; if (!want_quiet) fprintf(stderr,"ERROR: neither the video file nor the ffmpeg decoder were able to\n       provide a video frame timestamp."); }
		av_free_packet(packet);
		return (0);
	}

	if (mtsb >= timestamp) return (1); // ok!

	/* skip to next frame */
#ifdef FFDEBUG
	printf("To Skip %li -> %li\n",(long int) mtsb, (long int) timestamp);
#endif
	//my_avprev= mtsb;

	int frameFinished; 
	avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
	av_free_packet(packet);
	if (!frameFinished) {
		if (want_debug)
			fprintf(stderr, "seek decode not finished!\n"); /*XXX*/
		goto read_frame;
	}

	if (nolivelock < MAX_CONT_FRAMES) goto read_frame;
	
	reset_video_head(packet);

	return (0); // seek failed.
}

void display_frame(int64_t timestamp, int force_update) {
	static AVPacket packet;
	static int      fFirstTime=1;
	int             frameFinished;

	if (timestamp - file_frame_offset< 0) timestamp=0;
	else if(timestamp - file_frame_offset>= frames) timestamp = frames - 1;

	if (!force_update && dispFrame == timestamp) return;

	if(want_verbose)
		fprintf(stdout, "\t\t\t\tdisplay:%07li  \r", (long int) timestamp);

	dispFrame = timestamp;
	if (OSD_mode&OSD_FRAME) snprintf(OSD_frame,48,"Frame: %li", dispFrame);
	if (OSD_mode&OSD_SMPTE) frame_to_smptestring(OSD_smpte,dispFrame,midi_connected());

	if(fFirstTime) {
		fFirstTime=0;
		packet.data=NULL;
	}

	if (pFrameFMT && my_seek_frame(&packet, timestamp)) {
		#if 0
		AVStream *v_stream = pFormatCtx->streams[videoStream];
		long int myts = (packet.pts==AV_NOPTS_VALUE)?packet.dts:packet.pts;
		long int myf = (long int) (myts*av_q2d(v_stream->time_base)*framerate);
		if(want_verbose && myf != dispFrame) //< TODO: make it want_debug 
			fprintf(stdout, "\t\t\t\tdecoder:%07li  \r", myf);
		#endif
		/* Decode video frame */
		while (1) {
		#if 0
			if (packet.pts> 0 && OSD_mode&OSD_FRAME) {
				myts = (packet.pts==AV_NOPTS_VALUE)?packet.dts:packet.pts;
				myf = (long int) (myts*av_q2d(v_stream->time_base)*framerate);
				if (want_ignstart)  // timestamps in the file start counting at ..->start_time 
					myf-= (long int) ( framerate*(pFormatCtx->start_time/ (double) AV_TIME_BASE));
				if (myf != dispFrame) 
					snprintf(OSD_frame,49,"Frame: %li[%li]", dispFrame, myf);
			}
		#endif
			frameFinished=0;	
			if(packet.stream_index==videoStream) 
				avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet.data, packet.size);
	  
			/* Did we get a video frame? */
			if(frameFinished) {
				/* Convert the image from its native format to FMT */
				img_convert((AVPicture *)pFrameFMT, render_fmt, 
					(AVPicture*)pFrame, pCodecCtx->pix_fmt, pCodecCtx->width, 
					pCodecCtx->height);

				render_buffer(buffer); // in pFrameFMT
				av_free_packet(&packet); /* XXX */
				break;
			} else  { 
			//	fprintf( stderr, "Frame not finished\n");
				if(packet.data) av_free_packet(&packet);
				if(av_read_frame(pFormatCtx, &packet)<0) { 
					fprintf( stderr, "read error!\n");
					reset_video_head(&packet);
					render_empty_frame();
					break;
				}
#if LIBAVFORMAT_BUILD >=4616
				if (av_dup_packet(&packet) < 0) {
					printf("can not allocate packet\n");
					break;
				}
#endif
			}
		} /* end while !frame_finished */
	} else {
		if (pFrameFMT && want_debug) fprintf( stderr, "frame seek unsucessful.\n");
		render_empty_frame();
	}
}
  
int close_movie() {
	if(current_file) free(current_file);
	current_file=NULL;

	if (!pFrameFMT) return(-1);

	// Free the formatted image 
	if(buffer) free(buffer);
	buffer=NULL;
	if (pFrameFMT)
		av_free(pFrameFMT);
	pFrameFMT=NULL;

	//Free the YUV frame
	if (pFrame)
		av_free(pFrame);
	pFrame=NULL;

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

	if(open_movie(movie)){
		if (!want_quiet)
			printf("File not found or invalid.\n");
		exit(1);
	}
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
		printf("ok. great encoding, dude.\n");
	exit (0);
}
