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
#include "ffcompat.h"
#include "remote.h"
#include "gtime.h"

//------------------------------------------------
// extern Globals (main.c)
//------------------------------------------------
extern int loop_flag;
extern int loop_run;

extern int               movie_width;
extern int               movie_height;
extern int               ffctv_width;
extern int               ffctv_height;
extern float             movie_aspect;
extern AVFormatContext   *pFormatCtx;
extern int               videoStream;
extern AVCodecContext    *pCodecCtx;
extern AVFrame           *pFrame;
extern AVFrame           *pFrameFMT;
extern uint8_t           *buffer;
struct SwsContext *pSWSCtx;

// needs to be set before calling movie_open
extern int render_fmt;

/* Video File Info */
extern double  duration;
extern double  framerate;
extern long    frames;
extern int64_t file_frame_offset;
extern int have_dropframes;

/* Option flags and variables */
extern char    *current_file;
extern double   filefps;
extern long     ts_offset;
extern char    *smpte_offset;
extern long     userFrame;
extern long     dispFrame;
extern int      force_redraw;
extern int      want_quiet;
extern int      want_debug;
extern int      want_verbose;
extern int      want_avverbose;
extern int      want_nosplash;
extern int      want_genpts;
extern int      want_ignstart;
extern int      remote_en;
extern int      remote_mode;
extern int      mq_en;
extern char    *ipc_queue;
extern double   delay;
extern int      seekflags;
#if defined (HAVE_LTCSMPTE) || defined (HAVE_LTC)
extern int  use_ltc;
#endif

#ifdef TIMEMAP
long   timeoffset = 0;
double timescale = 1.0;
int    wraparound = 0;
#endif

// On screen display
extern char OSD_frame[48];
extern char OSD_smpte[13];
extern int  OSD_mode;

const AVRational c1_Q = { 1, 1 };
double tpf = 1.0; /* pts/dts increments per video-frame - cached value */

static int fFirstTime=1;

#ifdef JACK_SESSION
extern int jack_session_restore;
extern int js_winx;
extern int js_winy;
extern int js_winw;
extern int js_winh;
#endif

static void js_apply() {
#ifdef JACK_SESSION
	if (jack_session_restore) {
		jack_session_restore = 0;
		if (js_winx > 0 && js_winy > 0)
			Xposition(js_winx, js_winy);
		if (js_winw > 0 && js_winh > 0)
			Xresize(js_winw,js_winh);
	}
#endif
}
//--------------------------------------------
// main event loop
//--------------------------------------------

static int select_sleep (const long usec) {
	int remote_activity = 0;
#ifndef PLATFORM_WINDOWS
	fd_set fd;
	int max_fd=0;
	struct timeval tv = { 0, 0 };
	if (usec > 500) {
		tv.tv_sec = usec / 1000000L;
		tv.tv_usec = (usec % 1000000L);
	}
	FD_ZERO(&fd);
	if (remote_en) {
		max_fd=remote_fd_set(&fd);
	}
#endif
#if defined HAVE_MQ
	if (mq_en) {
		if (!remote_read_mq()) remote_activity=1;
	}
#elif defined HAVE_IPCMSG
	if (ipc_queue) {
		if (!remote_read_ipc()) remote_activity=1;
	}
#endif
#ifdef HAVE_LIBLO
	remote_activity |= process_osc();
#endif
#ifndef PLATFORM_WINDOWS
	if (remote_activity) {
		tv.tv_sec = 0; tv.tv_usec = 1;
	}
#endif
#ifdef PLATFORM_WINDOWS
	if ((!remote_en || remote_read_h()) && usec > 1000) {
		Sleep((usec + 999) / 1000); // XXX not nearly good enough.
	}
#else
	if (select(max_fd, &fd, NULL, NULL, &tv)) {
		remote_read_io();
		return 1;
	}
#endif
	return remote_activity;
}

void event_loop(void) {
	double  elapsed_time;
	int64_t clock1, clock2;
	long    newFrame, offFrame;
	float   nominal_delay;
	int64_t splash_timeout;
	int     splashed = 0;

	if (want_verbose) printf("\nentering video update loop @%.2f fps.\n",delay>0?(1.0/delay):framerate);
	clock1 = xj_get_monotonic_time();
	splash_timeout = clock1 + 2000000; // 2 sec;

	while(loop_flag) { /* MAIN LOOP */

		if (loop_run == 0) {
			/* video offline - (eg. window minimized)
			 * do not update frame
			 */
			select_sleep(2e5L);
			handle_X_events();
			lash_process();
			js_apply();
			continue;
		}

#ifdef HAVE_MIDI
		if (midi_connected()) newFrame = midi_poll_frame();
		else
#endif
#if (defined HAVE_LTCSMPTE || defined HAVE_LTC)
		if (ltcjack_connected()) newFrame = ltc_poll_frame();
		else
#endif
		newFrame = jack_poll_frame();

		if (newFrame < 0) newFrame = userFrame;

#if 0 // DEBUG
		static long		oldFrame = 0;
		if (oldFrame != newFrame) {
			if (oldFrame +1 != newFrame) {
				printf("\ndiscontinuity %li -> %li\n", oldFrame, newFrame);
			}
			oldFrame=newFrame;
		}
#endif

#ifdef TIMEMAP
		newFrame = (long) floor((double) newFrame * timescale) + timeoffset;
		// TODO: calc newFrames/frames instead of while-loop
		while (newFrame > frames && wraparound && frames!=0)
			newFrame-=frames;
		while (newFrame < 0 && wraparound && frames!=0)
			newFrame+=frames;
#endif

		offFrame = newFrame + ts_offset;
		long curFrame = dispFrame;
		display_frame((int64_t)(offFrame), force_redraw, splashed || want_nosplash);

		if ((remote_en||mq_en||ipc_queue)
				&& ( (remote_mode&NTY_FRAMELOOP) || ((remote_mode&NTY_FRAMECHANGE)&& curFrame!=dispFrame))
			 )
		{
			/*call xapi_pposition ?? -> rv:200
			 * dispFrame is the currently displayed frame
			 * = SMPTE + offset
			 */
			remote_printf(301,"position=%li",dispFrame);
		}
		force_redraw=0;
		nominal_delay = delay > 0 ? delay : (1.0/framerate);

		if (!splashed) {
			if (splash_timeout > clock1) {
				splash(buffer);
			} else {
				splashed = 1;
				force_redraw = 1;
			}
		}

		if(want_verbose) {
#if 0
			fprintf(stdout, "frame: smpte:%li    \r", newFrame);
#else
			char tempsmpte[15];
			frame_to_smptestring(tempsmpte,newFrame);
			fprintf(stdout, "smpte: %s f:%li\r", tempsmpte,newFrame);
#endif
			fflush(stdout);
		}

		handle_X_events();
		lash_process();
		js_apply();

		clock2 = xj_get_monotonic_time();
		nominal_delay *= 1000000.f;
		elapsed_time = (clock2 - clock1);
		if(elapsed_time < nominal_delay) {
			long microsecdelay = (long) floorf(nominal_delay - elapsed_time);
#if 0 // debug timing
			printf("  %7.1f ms, [%ld]\n", microsecdelay / 1e3, offFrame);
#endif
#if 1 // poll 10 times per frame, unless -f delay is given explicitly
			const long pollinterval = ceilf(nominal_delay * .1f);
			if (microsecdelay > pollinterval && delay <= 0) microsecdelay = pollinterval;
#endif
			if (!select_sleep(microsecdelay)) {
				; // remote event occured
			}
			if (curFrame != dispFrame) {
				clock1 = clock2;
			}
		}
		else {
			clock1 = clock2;
			if (!splashed) {
				force_redraw = 1;
			}
#if 0 // debug timing
			printf("@@ %7.1f ms [%ld]\n", (nominal_delay - elapsed_time) / 1e3, offFrame);
#endif
		}
	}

	if ((remote_en||mq_en||ipc_queue) && (remote_mode&4)) {
		// send current settings
		xapi_pfullscreen(NULL);
		xapi_pontop(NULL);
		xapi_posd(NULL);
		xapi_pletterbox(NULL);
		xapi_pwinpos(NULL);
		xapi_pwinsize(NULL);
		xapi_poffset(NULL);
	}
}

//--------------------------------------------
// Manage video file
//--------------------------------------------

static void render_empty_frame(int blit);

void init_moviebuffer(void) {
	int     numBytes;
	if (buffer) free(buffer);
	if (want_debug)
		printf("DEBUG: init_moviebuffer - render_fmt: %i\n",render_fmt);
	/* Determine required buffer size and allocate buffer */
#ifdef CROPIMG
	numBytes=avpicture_get_size(render_fmt, movie_width*2, movie_height);
#else
	numBytes=avpicture_get_size(render_fmt, movie_width, movie_height);
#endif
	buffer=(uint8_t *) calloc(1,numBytes);

	// Assign appropriate parts of buffer to image planes in pFrameFMT
	if (pFrameFMT) {
		avpicture_fill((AVPicture *)pFrameFMT, buffer, render_fmt, movie_width, movie_height);
		pSWSCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, movie_width, movie_height, render_fmt, SWS_BICUBIC, NULL, NULL, NULL);
	}
	render_empty_frame(0);
}

/* Open video file */

void avinit (void) {
	av_register_all();
#if LIBAVFORMAT_BUILD < 0x351400
	avcodec_init();
#endif
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

	fFirstTime = 1;
	pFrameFMT = NULL;
	pFormatCtx=NULL;
	movie_width  = 320;
	movie_height = 180;
	movie_aspect = (float)movie_width / (float) movie_height;
	duration = frames = 1;
	framerate = 10; // prevent slow reaction to remote-ctl (event loop).
	file_frame_offset = 0;
	videoStream=-1;
	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset);

	if (strlen(file_name) == 0
			&& (remote_en || mq_en || ipc_queue)) {
		return -1;
	}

	/* Open video file */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 7, 0)
	if(av_open_input_file(&pFormatCtx, file_name, NULL, 0, NULL)!=0)
#else
	if(avformat_open_input(&pFormatCtx, file_name, NULL, NULL)!=0)
#endif
	{
		if (!remote_en && !mq_en && !ipc_queue)
			fprintf( stderr, "Cannot open video file %s\n", file_name);
		pFormatCtx=NULL;
		return (-1);
	}

	/* Retrieve stream information */
	if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
		fprintf( stderr, "Cannot find stream information in file %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx=NULL;
		return (-1);
	}

#if LIBAVFORMAT_BUILD < 0x350200
	if (!want_quiet) dump_format(pFormatCtx, 0, file_name, 0);
#else
	if (!want_quiet) av_dump_format(pFormatCtx, 0, file_name, 0);
#endif

	/* Find the first video stream */
	for(i=0; i<pFormatCtx->nb_streams; i++)
#if LIBAVFORMAT_BUILD > 4629
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
#else
		if(pFormatCtx->streams[i]->codec.codec_type==AVMEDIA_TYPE_VIDEO)
#endif
		{
			videoStream=i;
			break;
		}

	if(videoStream==-1) {
		fprintf( stderr, "Cannot find a video stream in file %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx=NULL;
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

	// detect drop frame timecode
	if (fabs(framerate - 30000.0/1001.0) < 0.01) {
		have_dropframes=1;
		if(!want_quiet)
			fprintf(stdout, "enabled drop-frame-timecode (use -n to override).\n");
	}

#if defined(__BIG_ENDIAN__) && (__ppc__) && LIBAVFORMAT_BUILD <= 4616
	// this cast is weird, but it works.. the bytes seem to be in 'correct' order, but the two
	// 4byte-words are swapped. ?!
	// I wonder how this behaves on a 64bit arch
	// - maybe it's bug in ffmpeg or all video files I tried had a bad header :D
	int64_t dur = (int64_t) (pFormatCtx->duration);
	duration = ( ((double) (((dur&0xffffffff)<<32)|((dur>>32)&0xffffffff))) / (double) AV_TIME_BASE );
#else
	duration = (double) (((double) (pFormatCtx->duration))/ (double) AV_TIME_BASE);
#endif
	frames = (long) (framerate * duration);
#if LIBAVFORMAT_BUILD <= 4623  // check if correct;
	tpf = (double) framerate / (double) av_stream->codec.frame_rate * (double) av_stream->codec.frame_rate_base;
#elif LIBAVFORMAT_BUILD <= 4629 // check if correct;
	tpf = (av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate)/framerate);
#else
	tpf = 1.0/(av_q2d(pFormatCtx->streams[videoStream]->time_base)*framerate);
#endif
	if (!want_ignstart && pFormatCtx->start_time != AV_NOPTS_VALUE) {
		file_frame_offset = (int64_t) floor(framerate * (double) pFormatCtx->start_time / (double) AV_TIME_BASE);
	}

	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset);

	if (!want_quiet) {
		if (filefps >0 )
			fprintf(stdout, "overridden frame rate: %g\n", framerate);
		else
			fprintf(stdout, "original frame rate: %g\n", framerate);
		fprintf(stdout, "length in seconds: %g\n", duration);
		fprintf(stdout, "total frames: %ld\n", frames);
		fprintf(stdout, "file start offset: %" PRId64 " video-frames\n",file_frame_offset);
	}

	// Get a pointer to the codec context for the video stream
#if LIBAVFORMAT_BUILD > 4629
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;
#else
	pCodecCtx=&(pFormatCtx->streams[videoStream]->codec);
#endif

	if (!want_quiet) {
		fprintf(stderr, "image size: %ix%i px\n", pCodecCtx->width, pCodecCtx->height);
	}

#ifdef CROPIMG
	movie_width = (pCodecCtx->width / 2)&~1; // TODO allow configuration
	movie_height = pCodecCtx->height &~1;
#else
	movie_width = pCodecCtx->width &~1;
	movie_height = pCodecCtx->height &~1;
#endif

	float sample_aspect = 1.0;

	if (av_stream->sample_aspect_ratio.num)
		sample_aspect = av_q2d(av_stream->sample_aspect_ratio);
	else if (av_stream->codec->sample_aspect_ratio.num)
		sample_aspect = av_q2d(av_stream->codec->sample_aspect_ratio);
	else
		sample_aspect = 1.0;

	movie_aspect = sample_aspect * (float)pCodecCtx->width / (float) pCodecCtx->height;

	ffctv_height = movie_height;
	ffctv_width = ((int)rint(pCodecCtx->height * movie_aspect)) & ~1;
	if (ffctv_width > pCodecCtx->width) {
		ffctv_width = movie_width;
		ffctv_height = ((int)rint(pCodecCtx->width / movie_aspect)) & ~1;
	}

	// somewhere around LIBAVFORMAT_BUILD  4630
#ifdef AVFMT_FLAG_GENPTS
	if (want_genpts)
		pFormatCtx->flags|=AVFMT_FLAG_GENPTS;
#endif

	if (!want_quiet) {
		fprintf(stderr, "display size: %ix%i px\n", movie_width, movie_height);
	}
	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf( stderr, "Cannot find a codec for file: %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return( -1 );
	}

	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		fprintf( stderr, "Cannot open the codec for file %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return( -1 );
	}

	pFrame=av_frame_alloc();
	if(pFrame==NULL) {
		fprintf( stderr, "Cannot allocate video frame buffer\n");
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return(-1);
	}

	pFrameFMT=av_frame_alloc();
	if(pFrameFMT==NULL) {
		fprintf( stderr, "Cannot allocate display frame buffer\n");
		av_free(pFrame);
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return(-1);
	}

	current_file=strdup(file_name);
	return( 0 );
}

void override_fps (double fps) {
	if (fps <= 0) return;

	framerate =  fps;
	frames = (long) (framerate * duration);
#if LIBAVFORMAT_BUILD <= 4623  // check if correct;
	tpf = (double) framerate / (double) pCodecCtx->frame_rate * (double) pCodecCtx->frame_rate_base;
#elif LIBAVFORMAT_BUILD <= 4629 // check if correct;
	tpf = (av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate)/framerate);
#else
	tpf = 1.0/(av_q2d(pFormatCtx->streams[videoStream]->time_base)*framerate);
#endif
	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset);
}

static int64_t my_avprev = 0; // last recent seeked timestamp

static void render_empty_frame(int blit) {
	if (!buffer) return;
	// clear image (black / or YUV green)
	if (render_fmt == PIX_FMT_UYVY422) {
		int i;
		for (i=0;i<movie_width*movie_height*2;i+=2) {
			buffer[i]=0x80;
			buffer[i+1]=0x00;
		}
	}
	else if (render_fmt == PIX_FMT_YUV420P) {
		size_t Ylen  = movie_width * movie_height;
		memset(buffer,0,Ylen);
		memset(buffer+Ylen,0x80,Ylen/2);
	} else if (render_fmt == PIX_FMT_RGBA32) {
		int i;
		for (i=0; i < movie_width * movie_height * 4; i+=4) {
			buffer[i]   = 0x00;
			buffer[i+1] = 0x00;
			buffer[i+2] = 0x00;
			buffer[i+3] = 0xff;
		}
	} else if (render_fmt == PIX_FMT_BGRA32) {
		int i;
		for (i=0; i < movie_width * movie_height * 4; i+=4) {
			buffer[i]   = 0x00;
			buffer[i+1] = 0x00;
			buffer[i+2] = 0x00;
			buffer[i+3] = 0xff;
		}
	} else {
		memset(buffer,0,avpicture_get_size(render_fmt, movie_width, movie_height));
	}
#ifdef DRAW_CROSS
	int x,y;
	if (render_fmt == PIX_FMT_UYVY422)
		for (x=0,y=0;x< movie_width-1; x++,y= movie_height*x/movie_width) {
			int off=(2*x+2*movie_width*y);
			buffer[off]=127; buffer[off+1]=127;

			off=(2*x+2*movie_width*(movie_height-y-1));
			buffer[off]=127; buffer[off+1]=127;
		}
	if (render_fmt == PIX_FMT_YUV420P)
		for (x=0,y=0;x< movie_width-1; x++,y= movie_height*x/movie_width) {
			int yoff=(x+movie_width*y);
			//int uvoff=((x/2)+movie_width/2*(y/2));
			buffer[yoff]=127; buffer[yoff+1]=127;

			yoff=(x+movie_width*(movie_height-y-1));
			//uvoff=((x/2)+movie_width/2*((movie_height-y-1)/2));
			buffer[yoff]=127; buffer[yoff+1]=127;
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
	if (render_fmt == PIX_FMT_RGBA32 || render_fmt == PIX_FMT_BGRA32)
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
	if (blit)
		render_buffer(buffer);
}

static void reset_video_head(AVPacket *packet) {
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
#if LIBAVCODEC_VERSION_MAJOR < 52 || ( LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR < 21)
			avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
#else
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);
#endif
		if(packet->data) av_free_packet(packet);
	}
	my_avprev=dispFrame=0;
}

// TODO: set this high (>1000) if transport stopped and to a low value (<100) if transport is running.
#define MAX_CONT_FRAMES (500)

static int my_seek_frame (AVPacket *packet, int64_t timestamp) {
	AVStream *v_stream;
	int rv=1;
	int nolivelock = 0;
	int64_t mtsb = 0;
	static int ffdebug = 0;

	if (videoStream < 0) return (0);
	v_stream = pFormatCtx->streams[videoStream];

	if (want_ignstart==1) {
		timestamp+= file_frame_offset;
	}

	// TODO: assert  0 < timestamp + ts_offset - (..->start_time)   < length

#if LIBAVFORMAT_BUILD > 4629 // verify this version
# ifdef FFDEBUG
	printf("\nDEBUG: want frame=%li  ", (long int) timestamp);
# endif

	if (filefps > 0) {
		timestamp*=tpf;
	} else {
		// does not work with -F <double>, but it's more accurate when rounding ratios
		timestamp=av_rescale_q(timestamp,c1_Q,v_stream->time_base);
		timestamp=av_rescale_q(timestamp,c1_Q,v_stream->r_frame_rate); //< timestamp/=framerate;
	}

# ifdef FFDEBUG
	printf("ts=%li   ##\n", (long int) timestamp);
# endif
#endif

#if LIBAVFORMAT_BUILD < 4617
	rv= av_seek_frame(pFormatCtx, videoStream, timestamp / framerate * 1000000LL);
#else
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
		//printf("SEEK: %d\n",timestamp);
		rv= av_seek_frame(pFormatCtx, videoStream, timestamp, AVSEEK_FLAG_BACKWARD) ;
		avcodec_flush_buffers(pCodecCtx);
	}
#endif

	if (rv < 0 && (timestamp == 0 || seekflags == SEEK_CONTINUOUS)) {
		rv = av_seek_frame(pFormatCtx, videoStream, timestamp, 0);
	}

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
	printf("\nDEBUG: got pts=%li dts:%li frame: p:%g d:%g    ##\n",
			(long int) packet->pts , (long int) packet->dts,
			packet->pts*framerate*av_q2d(v_stream->time_base),
			packet->dts*framerate*av_q2d(v_stream->time_base));
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
#if 0
	if (mtsb != AV_NOPTS_VALUE && mtsb!=0) my_avprev = mtsb;
	if (mtsb > timestamp) { printf("WRONG want:%lli got:%lli\n", timestamp, mtsb); my_avprev = mtsb;}
	if (mtsb == timestamp) printf("Right.\n");
#endif

#if 1
	if (mtsb >= timestamp)
#else // test & check time-base/ framerate re-scaling
	if ((mtsb*framerate*av_q2d(v_stream->time_base)) >= (timestamp*framerate*av_q2d(v_stream->time_base)))
#endif
	{
		my_avprev = mtsb;
		return (1); // ok!
	}

	/* skip to next frame */
#ifdef FFDEBUG
	printf("To Skip %li -> %li\n",(long int) mtsb, (long int) timestamp);
#endif
	//my_avprev= mtsb;

	int frameFinished;
#if LIBAVCODEC_VERSION_MAJOR < 52 || ( LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR < 21)
	avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
#else
	avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);
#endif
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

void display_frame(int64_t timestamp, int force_update, int do_render) {
	static AVPacket packet;
	int             frameFinished;

	if (!buffer || !current_file) { return; }

	if (timestamp - file_frame_offset< 0) timestamp=0;
	else if(timestamp - file_frame_offset>= frames) timestamp = frames - 1;

	if (!force_update && dispFrame == timestamp) return;

	if(want_verbose)
		fprintf(stdout, "\t\t\t\tdisplay:%07li  \r", (long int) timestamp);

	dispFrame = timestamp;
	if (OSD_mode&OSD_FRAME)
		snprintf(OSD_frame,48,"Frame: %li", dispFrame);
	if (OSD_mode&OSD_SMPTE) frame_to_smptestring(OSD_smpte,dispFrame - ts_offset);

	if(fFirstTime) {
		fFirstTime=0;
		memset(&packet, 0, sizeof(AVPacket));
	}

	if (pFrameFMT && my_seek_frame(&packet, timestamp)) {
		/* Decode video frame */
		while (1) {
			frameFinished=0;
			if(packet.stream_index==videoStream)
#if LIBAVCODEC_VERSION_MAJOR < 52 || ( LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR < 21)
				avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet.data, packet.size);
#else
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
#endif

			/* Did we get a video frame? */
			if(frameFinished) {
				/* Convert the image from its native format to FMT */
				// TODO: this can be done once per Video output.
				int dstStride[8] = {0,0,0,0,0,0,0,0};
				switch (render_fmt) {
					case PIX_FMT_RGBA32:
					case PIX_FMT_BGRA32:
						dstStride[0] = movie_width*4;
						break;
					case PIX_FMT_BGR24:
						dstStride[0] = movie_width*3;
						break;
					case PIX_FMT_UYVY422:
						dstStride[0] = movie_width*2;
						break;
					case PIX_FMT_YUV420P:
					default:
						dstStride[0] = movie_width;
						dstStride[1] = movie_width/2;
						dstStride[2] = movie_width/2;
				}
				sws_scale(pSWSCtx, (const uint8_t * const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameFMT->data, dstStride);
				if (do_render)
					render_buffer(buffer); // in pFrameFMT
				av_free_packet(&packet); /* XXX */
				break;
			} else  {
				//fprintf( stderr, "Frame not finished\n");
				if(packet.data) av_free_packet(&packet);
				if(av_read_frame(pFormatCtx, &packet)<0) {
					//fprintf( stderr, "read error!\n");
					reset_video_head(&packet);
					render_empty_frame(1);
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
		render_empty_frame(1);
	}
}

int close_movie() {
	if(current_file) free(current_file);
	current_file=NULL;

	if (!pFrameFMT) return(-1);
	// Free the software scaler
	sws_freeContext(pSWSCtx);

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
	avformat_close_input(&pFormatCtx);
	duration = frames = 1;
	pCodecCtx = NULL;
	pFormatCtx = NULL;
	framerate = 10; // prevent slow reaction to remote-ctl (event loop).
	return (0);
}
