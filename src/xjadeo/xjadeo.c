/* xjadeo - jack video monitor, decoder and main event loop
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
 *
 */
#include "xjadeo.h"

#include "ffcompat.h"
#include <libswscale/swscale.h>
#include <pthread.h>

#include "remote.h"
#include "gtime.h"

#define USE_DUP_PACKET // needed for some codecs with old ffmpeg

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
struct SwsContext        *pSWSCtx;

// needs to be set before calling movie_open
extern int render_fmt;

/* Video File Info */
extern double  duration;
extern double  framerate;
extern int64_t frames;
extern int64_t file_frame_offset;
extern int have_dropframes;

/* Option flags and variables */
extern char    *current_file;
extern double   filefps;
extern int64_t  ts_offset;
extern char    *smpte_offset;
extern int64_t  userFrame;
extern int64_t  dispFrame;
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
#ifdef HAVE_LTC
extern int  use_ltc;
#endif

// On screen display
extern char OSD_frame[48];
extern char OSD_smpte[13];
extern int  OSD_mode;
extern char OSD_msg[128];

//------------------------------------------------
// globals
//------------------------------------------------

#ifdef TIMEMAP
int64_t timeoffset = 0;
double  timescale = 1.0;
int     wraparound = 0;
#endif


struct FrameIndex {
	int64_t pts;
	int64_t pos;
	int     key;
};

static struct FrameIndex *fidx = NULL;

static int64_t last_decoded_frame = -1;
static int64_t fcnt = 0;
static int seek_threshold = 8;
static int abort_indexing = 0;
static int scan_complete = 0;
static int thread_active = 0;
static int prefer_pts = 0;
static int pos_mismatch = 0;

static pthread_t index_thread;

static AVRational fr_Q = { 1, 1 };
static double     tpf = 1.0; /* pts/dts increments per video-frame - cached value */
static int        fFirstTime=1;


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

//--------------------------------------------
// main event loop
//--------------------------------------------

void event_loop(void) {
	double  elapsed_time;
	int64_t clock1, clock2;
	int64_t newFrame, offFrame;
	float   nominal_delay;
	int64_t splash_timeout;
	int     splashed = want_nosplash;

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
			js_apply();
			continue;
		}

#ifdef HAVE_MIDI
		if (midi_connected()) newFrame = midi_poll_frame();
		else
#endif
#ifdef HAVE_LTC
		if (ltcjack_connected()) newFrame = ltc_poll_frame();
		else
#endif
		newFrame = jack_poll_frame();

		if (newFrame < 0) newFrame = userFrame;

#if 0 // DEBUG
		static int64_t oldFrame = 0;
		if (oldFrame != newFrame) {
			if (oldFrame +1 != newFrame) {
				printf("\ndiscontinuity %"PRId64" -> %"PRId64"\n", oldFrame, newFrame);
			}
			oldFrame = newFrame;
		}
#endif

#ifdef TIMEMAP
		newFrame = floor((double)newFrame * timescale) + timeoffset;
		// TODO: calc newFrames/frames instead of while-loop
		while (newFrame > frames && wraparound && frames != 0)
			newFrame -= frames;
		while (newFrame < 0 && wraparound && frames != 0)
			newFrame += frames;
#endif

		offFrame = newFrame + ts_offset;
		int64_t curFrame = dispFrame;
		display_frame (offFrame, force_redraw, splashed || want_nosplash);
		force_redraw=0;

		if ((remote_en||mq_en||ipc_queue)
				&& ( (remote_mode&NTY_FRAMELOOP) || ((remote_mode&NTY_FRAMECHANGE) && curFrame != dispFrame))
			 )
		{
			/*call xapi_pposition ?? -> rv:200
			 * dispFrame is the currently displayed frame
			 * = SMPTE + offset
			 */
			remote_printf(301,"position=%"PRId64, dispFrame);
		}
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
			fprintf(stdout, "frame: smpte:%"PRId64"    \r", newFrame);
#else
			char tempsmpte[15];
			frame_to_smptestring(tempsmpte,newFrame);
			fprintf(stdout, "smpte: %s f:%"PRId64"\r", tempsmpte, newFrame);
#endif
			fflush(stdout);
		}

		handle_X_events();
		js_apply();

		clock2 = xj_get_monotonic_time();
		nominal_delay *= 1000000.f;
		elapsed_time = (clock2 - clock1);
		if(elapsed_time < nominal_delay) {
			long microsecdelay = (long) floorf(nominal_delay - elapsed_time);
#if 0 // debug timing
			printf("  %7.1f ms, [%"PRId64"]\n", microsecdelay / 1e3, offFrame);
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
			printf("@@ %7.1f ms [%"PRId64"]\n", (nominal_delay - elapsed_time) / 1e3, offFrame);
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
// Video file, rendering and ffmpeg inteface
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

void avinit (void) {
	av_register_all();
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 20, 0)
	avcodec_init();
#endif
	avcodec_register_all();
	if(!want_avverbose) av_log_set_level(AV_LOG_QUIET);
}

static void reset_video_head (AVPacket *packet) {
	int frameFinished=0;
	int seek = av_seek_frame (pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
	if (pCodecCtx->codec->flush) {
		avcodec_flush_buffers(pCodecCtx);
	}

	while (seek >= 0 && !frameFinished) {
		av_read_frame(pFormatCtx, packet);
		if(packet->stream_index==videoStream)
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
			avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
#else
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);
#endif
		if(packet->data) av_free_packet(packet);
	}
	last_decoded_frame = -1;
}

static int seek_keyframe (AVPacket *packet, int64_t timestamp) {
	AVStream *v_stream = pFormatCtx->streams[videoStream];

	if (want_ignstart) {
		timestamp += file_frame_offset;
	}

	if (filefps > 0) {
		timestamp *= tpf;
	} else {
		timestamp = av_rescale_q(timestamp, fr_Q, v_stream->time_base);
	}

	const int seek = av_seek_frame(pFormatCtx, videoStream, timestamp, AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD) ;

	while (seek >= 0) {
		if (av_read_frame(pFormatCtx, packet) < 0) {
			if (!want_quiet)
				fprintf(stderr, "Read failed (during seek)\n");
			reset_video_head (packet);
			break;
		}
#ifdef USE_DUP_PACKET
		if (av_dup_packet (packet) < 0) {
			fprintf(stderr, "Error: Cannot allocate video packet.\n");
			continue; // or break ?
		}
#endif
		if(packet->stream_index != videoStream) {
			av_free_packet (packet);
			continue;
		}
		return 0; // OK
	}
	return -2;
}

static void reset_index () {
	last_decoded_frame = -1;
	fcnt = 0;
	seek_threshold = 8;
	abort_indexing = 0;
	scan_complete = 0;
	prefer_pts = 0;
	pos_mismatch = 0;
}

#define MAX_KEYFRAME_INTERVAL 100

static int seek_indexed (AVPacket *packet, int64_t ts) {
#if 0 // seek byte ~bckward hack
	static int64_t did_contd = 0;
#endif
	int64_t sframe;
	int need_seek = 1;

	if (!want_ignstart) {
		ts -= file_frame_offset;
	}

	if (filefps > 0) {
		ts *= framerate * av_q2d(fr_Q);
	}

	sframe = ts;

	if (sframe < 0 || sframe >= frames) {
		if (!want_quiet)
			fprintf(stderr, "SEEK OUT OF BOUNDS frame: %"PRId64"\n", sframe);
		return -2;
	}

	const int64_t cts = ts;
	// check if we can just continue without seeking
	if (last_decoded_frame > 0 && ts > last_decoded_frame && ts - last_decoded_frame < seek_threshold) {
		need_seek = 0;
		sframe = last_decoded_frame + 1;
#if 0 // seek byte ~bckward hack
		did_contd = 1;
#endif
	} else {
#if 0 // reset av_seek -- hack for AVSEEK_FLAG_BYTE without AVSEEK_FLAG_BACKWARD
		if (did_contd) { av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD); }
		did_contd = 0;
#endif
		// lookup keyframe before target frame
		while (sframe > 0 && !fidx[sframe].key) --sframe;
	}
	last_decoded_frame = -1;


	if (need_seek) {
		int rv = -1;
		if (prefer_pts || fidx[sframe].pos < 0) {
			rv = av_seek_frame(pFormatCtx, videoStream, fidx[sframe].pts, AVSEEK_FLAG_BACKWARD);
		}
		if (rv < 0 && fidx[sframe].pos >= 0) {
			rv = av_seek_frame(pFormatCtx, videoStream, fidx[sframe].pos, AVSEEK_FLAG_BYTE | AVSEEK_FLAG_BACKWARD);
		}
		if (rv < 0) {
			if (!want_quiet)
				fprintf(stderr, "SEEK FAILED\n");
			return -3; // ERR
		}
		if (pCodecCtx->codec->flush) {
			avcodec_flush_buffers(pCodecCtx);
		}
	}

	int bailout = 2 * MAX_KEYFRAME_INTERVAL; // x2 because image may span multiple packets
	while (1) {
		if (av_read_frame(pFormatCtx, packet) < 0) {
			if (!want_quiet)
				fprintf(stderr, "Read failed (during indexed seek)\n");
			reset_video_head (packet);
			return -4;
		}
#ifdef USE_DUP_PACKET
		if (av_dup_packet (packet) < 0) {
			fprintf(stderr, "Error: Cannot allocate video packet.\n");
			continue;
		}
#endif
		if(packet->stream_index != videoStream) {
			av_free_packet (packet);
			continue;
		}

		if (sframe == ts) {
			// found the frame that we're looking for
			last_decoded_frame = cts;
#if 0 // DEBUG
			printf("IDX %"PRId64" %"PRId64" %"PRId64" || PK %"PRId64" %"PRId64"\n",
					sframe, fidx[sframe].pts, fidx[sframe].pos, packet->pts, packet->pos);
#endif
			// check if we can rely on byte position
			if (fidx[sframe].pos != packet->pos) {
				++pos_mismatch;
			} else {
				pos_mismatch = 0;
			}
			if (!prefer_pts && pos_mismatch > 3) {
				prefer_pts = 1;
				if (!want_quiet)
					fprintf(stderr, "Switched to prefer PTS over byte seek.\n");
			}
			return 0; // OK
		}

		// keep decoding until we find the frame we want.
		++sframe;

		int frameFinished = 0;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
		avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
#else
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);
#endif
		av_free_packet(packet);
		if (!frameFinished) {
			++ts;
		}
		if (--bailout == 0) {
			if (!want_quiet)
				fprintf(stderr, "Index-seek: bail out. frame-distance too large.\n");
			reset_video_head (packet); // ??
			return -5;
		}
	}
	return -6;
}

static int seek_frame (AVPacket *packet, int64_t ts) {
	if (!scan_complete) return -1;
	if (videoStream < 0) return -1;
	if (seek_threshold) {
		return seek_indexed (packet, ts);
	} else {
		return seek_keyframe (packet, ts);
	}
}

static int add_idx (int64_t ts, int64_t pos, int key, int duration, AVRational tb) {
	if (fcnt >= frames) {
		++fcnt;
		if (!want_quiet)
			fprintf(stderr, "Index table Overflow: %"PRId64" / %"PRId64" frames.\n", fcnt, frames);
		return -1;
	}
	// TODO use duration delta if both ts and pos are unset
	// -> write ts from prev ts + duration * ts
#if 0 // DEBUG
	printf("IDX; %"PRId64" %"PRId64" %"PRId64" %s\n",
			fcnt, ts, pos, key ? "K" : " ");
#endif
	fidx[fcnt].pts = ts;
	fidx[fcnt].pos = pos;
	fidx[fcnt].key = key;
	++fcnt;
	return 0;
}

static int index_frames () {
	AVPacket packet;
	int      use_dts = 0;
	int      error = 0;

#ifndef HAVE_AV_INIT_PACKET
	memset(&packet, 0, sizeof(AVPacket));
#else
	av_init_packet(&packet);
#endif
	packet.data = NULL;
	packet.size = 0;

	int max_keyframe_interval = 0;
	int keyframe_interval = 0;

	AVRational tb = pFormatCtx->streams[videoStream]->time_base;

	if (want_verbose) {
		printf("Indexing Video...\n");
	}

	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		if (abort_indexing) {
			if (!want_quiet) fprintf(stderr, "Indexing aborted.\n");
			break;
		}
		if(packet.stream_index != videoStream) {
			av_free_packet(&packet);
			continue;
		}

		int64_t ts = AV_NOPTS_VALUE;
		if (!use_dts && packet.pts != AV_NOPTS_VALUE)
			ts = packet.pts;
		if (ts == AV_NOPTS_VALUE)
			use_dts = 1;
		if (use_dts && packet.dts != AV_NOPTS_VALUE)
			ts = packet.dts;

		if (ts == AV_NOPTS_VALUE) {
			if (packet.duration == 0) {
				error |= 1;
				if (!want_quiet)
					fprintf(stderr, "index error: no pts, dts, nor duration\n");
				av_free_packet(&packet);
				break;
			}
#if 1 // TODO use previous PTS, add duration of prev frame.
			// -> see also add_idx()
			fprintf(stderr, "index error: duration not yet supported\n");
			error |= 2;
			av_free_packet(&packet);
			break;
#endif
		}

		const int key = (packet.flags & AV_PKT_FLAG_KEY) ? 1 : 0;
		if (add_idx (ts, packet.pos, key, packet.duration, tb)) {
			break;
		}
		av_free_packet(&packet);

		if (++keyframe_interval > max_keyframe_interval) {
			max_keyframe_interval = keyframe_interval;
		}
#if 1
		if ((fcnt == 500 || fcnt == frames) && max_keyframe_interval == 1) {
			if (want_verbose)
				printf("First 500 frames are all keyframes. Index disabled. Direkt seek mode enabled.\n");
			break;
		}
#endif
		if (key) keyframe_interval = 0;
	}

	seek_threshold = max_keyframe_interval - 1;
	if (seek_threshold > MAX_KEYFRAME_INTERVAL) {
		error |= 4;
		if (!want_quiet)
			fprintf(stderr,
					"WARNING: Keyframe distance is very large (>%d frames).\n"
					"The file is not unsuitable. Please transcode.\n",
					MAX_KEYFRAME_INTERVAL);
		seek_threshold = MAX_KEYFRAME_INTERVAL;
	}

	if (want_verbose) {
		printf("Scan complete err: %d use-dts: %s\n", error, use_dts ? "yes" : "no");
		printf("scanned %"PRId64" of %"PRId64" frames, key-int: %d seek-thresh: %d\n",
				fcnt, frames, max_keyframe_interval, seek_threshold);
	}

	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	if (pCodecCtx->codec->flush) {
		avcodec_flush_buffers(pCodecCtx);
	}
	if (!error) {
		scan_complete = 1;
	}
	return error;
}

static void *index_run(void *arg) {
	OSD_mode |= OSD_MSG;
	sprintf(OSD_msg, "Indexing. Please wait.");
	force_redraw = 1;
	if (!index_frames()) {
		OSD_mode &= ~OSD_MSG;
	} else {
		OSD_mode |= OSD_BOX;
		sprintf(OSD_msg, "Index Error. File is not suitable.");
	}
	force_redraw = 1;
	pthread_exit(NULL);
	return (NULL);
}

static void cancel_index_thread (void) {
	if (!thread_active) return;
	abort_indexing = 1;
	pthread_join(index_thread, NULL);
	thread_active = 0;
}

static int start_index_thread (void) {
	if (thread_active) {
		if (!want_quiet) fprintf(stderr, "Indexing thread is still active. Forcing Re-start.\n");
		cancel_index_thread();
	}
	if(pthread_create(&index_thread, NULL, index_run, NULL)) {
		if (!want_quiet) fprintf(stderr, "Cannot launch index thread.\n");
		return -1;
	}
	thread_active = 1;
	return 0;
}

int open_movie(char* file_name) {
	int i;
	AVCodec		*pCodec;
	AVStream	*av_stream;

	if (pFrameFMT) {
		close_movie();
	}

	OSD_mode &= ~OSD_MSG;
	reset_index ();

	/* set some defaults, in case open fails, the main-loop
	 * will still get some consistent data
	 */
	fFirstTime   = 1;
	pFrameFMT    = NULL;
	pFormatCtx   = NULL;
	movie_width  = ffctv_width = 320;
	movie_height = ffctv_height = 180;
	movie_aspect = (float)movie_width / (float) movie_height;
	duration     = 1;
	frames       = 1;
	framerate    = 10;
	videoStream  = -1;
	file_frame_offset = 0;

	// recalc offset with default framerate
	if (smpte_offset) {
		ts_offset = smptestring_to_frame(smpte_offset);
	}

	if (strlen(file_name) == 0) {
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
			fprintf(stderr, "Cannot open video file '%s'\n", file_name);
		pFormatCtx=NULL;
		return (-1);
	}

	/* Retrieve stream information */
	if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
		fprintf(stderr, "Cannot find stream information in file %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx=NULL;
		return (-1);
	}

	/* dump video information */
	if (!want_quiet) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 2, 0)
		dump_format(pFormatCtx, 0, file_name, 0);
#else
		av_dump_format(pFormatCtx, 0, file_name, 0);
#endif
	}

	/* Find the first video stream */
	for(i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}

	if(videoStream==-1) {
		fprintf(stderr, "Cannot find a video stream in file %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx=NULL;
		return( -1 );
	}

	av_stream = pFormatCtx->streams[videoStream];

	/* framerate.
	 * Note: frame-accurate seek scales by v_stream->time_base
	 * hence here AVRational fractions are inverse.
	 */
	framerate = 0;
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(55, 0, 100) // 9cf788eca8ba (merge a75f01d7e0)
	{
		AVRational fr = av_stream->r_frame_rate;
		if(fr.den > 0 && fr.num > 0) {
			framerate = av_q2d(av_stream->r_frame_rate);
			fr_Q.den = fr.num;
			fr_Q.num = fr.den;
		}
	}
#else
	{
		AVRational fr = av_stream_get_r_frame_rate(av_stream);
		if(fr.den > 0 && fr.num > 0) {
			framerate = av_q2d(fr);
			fr_Q.den = fr.num;
			fr_Q.num = fr.den;
		}
	}
#endif
	if (framerate < 1 || framerate > 1000) {
		AVRational fr = av_stream->avg_frame_rate;
		if(fr.den > 0 && fr.num > 0) {
			framerate = av_q2d(fr);
			fr_Q.den = fr.num;
			fr_Q.num = fr.den;
		}
	}
	if (framerate < 1 || framerate > 1000) {
		AVRational fr = av_stream->time_base;
		if(fr.den > 0 && fr.num > 0) {
			framerate = 1.0 / av_q2d(fr);
			fr_Q.den = fr.den;
			fr_Q.num = fr.num;
		}
	}
	if (framerate < 1 || framerate > 1000) {
		fprintf(stderr, "WARNING: cannot determine video-frame rate, using 25fps.\n");
		framerate = 25;
		fr_Q.den = 25;
		fr_Q.num = 1;
	}

	if (filefps > 0) {
		framerate = filefps;
	}

	// detect drop frame timecode
	if (fabs(framerate - 30000.0/1001.0) < 0.01) {
		have_dropframes=1;
		if(!want_quiet)
			fprintf(stdout, "enabled drop-frame-timecode (use -n to override).\n");
	}

	if (pFormatCtx->streams[videoStream]->nb_frames > 0) {
		frames = pFormatCtx->streams[videoStream]->nb_frames;
		duration = frames * av_q2d(fr_Q);
	} else {
		duration = ((double)pFormatCtx->duration / (double)AV_TIME_BASE); /// XXX
		frames = framerate * duration;
	}

	tpf = 1.0 / (av_q2d(pFormatCtx->streams[videoStream]->time_base) * framerate);
	if (!want_ignstart && pFormatCtx->start_time != AV_NOPTS_VALUE) {
		file_frame_offset = (int64_t) floor(framerate * (double) pFormatCtx->start_time / (double) AV_TIME_BASE);
	}

	fidx = malloc(frames * sizeof(struct FrameIndex));
	for (i = 0; i < frames; ++i) {
		fidx[i].pts = -1;
		fidx[i].pos = -1;
		fidx[i].key = 0;
	}

	// recalc offset with new framerate
	if (smpte_offset) {
		ts_offset = smptestring_to_frame(smpte_offset);
	}

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	if (!want_quiet) {
		if (filefps > 0)
			fprintf(stdout, "user-set frame rate: %g\n", framerate);
		else
			fprintf(stdout, "detected frame rate: %g\n", framerate);
		fprintf(stdout, "duration in seconds: %g\n", duration);
		fprintf(stdout, "total frames: %"PRId64"\n", frames);
		fprintf(stdout, "file start offset: %" PRId64 " video-frames\n", file_frame_offset);
		fprintf(stderr, "image size: %ix%i px\n", pCodecCtx->width, pCodecCtx->height);
	}

#ifdef CROPIMG
		movie_width = (pCodecCtx->width / 2); // TODO allow configuration
		movie_height = pCodecCtx->height;
#else
		movie_width = pCodecCtx->width;
		movie_height = pCodecCtx->height;
#endif

	/* sample aspect ratio, display aspect ratio */
	float sample_aspect = 1.0;
	if (av_stream->sample_aspect_ratio.num)
		sample_aspect = av_q2d(av_stream->sample_aspect_ratio);
	else if (av_stream->codec->sample_aspect_ratio.num)
		sample_aspect = av_q2d(av_stream->codec->sample_aspect_ratio);
	else
		sample_aspect = 1.0;

	movie_aspect = sample_aspect * (float)pCodecCtx->width / (float) pCodecCtx->height;

	/* calculate effective width, height */
	ffctv_height = movie_height;
	ffctv_width = ((int)rint(pCodecCtx->height * movie_aspect));
	if (ffctv_width > pCodecCtx->width) {
		ffctv_width = movie_width;
		ffctv_height = ((int)rint(pCodecCtx->width / movie_aspect));
	}

	if (render_fmt == PIX_FMT_RGB24 || render_fmt == PIX_FMT_BGRA32) {
		;
	} else {
		// YV12 needs 2x2 area for color
		movie_width  = movie_width  & ~1;
		movie_height = movie_height & ~1;
		ffctv_width  = ffctv_width  & ~1;
		ffctv_height = ffctv_height & ~1;
	}

#ifdef AVFMT_FLAG_GENPTS // XXX check if enum
	if (want_genpts)
		pFormatCtx->flags|=AVFMT_FLAG_GENPTS;
#endif

	if (!want_quiet) {
		fprintf(stderr, "display size: %ix%i px\n", movie_width, movie_height);
	}
	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Cannot find a codec for file: %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return( -1 );
	}

	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		fprintf(stderr, "Cannot open the codec for file %s\n", file_name);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return( -1 );
	}

	pFrame=av_frame_alloc();
	if(pFrame==NULL) {
		fprintf(stderr, "Cannot allocate video frame buffer\n");
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return(-1);
	}

	pFrameFMT=av_frame_alloc();
	if(pFrameFMT==NULL) {
		fprintf(stderr, "Cannot allocate display frame buffer\n");
		av_free(pFrame);
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return(-1);
	}

	current_file = strdup(file_name);

	start_index_thread();

	return( 0 );
}

void override_fps (double fps) {
	if (fps <= 0) return;

	framerate = fps;
	tpf = 1.0/(av_q2d(pFormatCtx->streams[videoStream]->time_base)*framerate);
	// recalc offset with new framerate
	if (smpte_offset) ts_offset=smptestring_to_frame(smpte_offset);
}

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

void display_frame(int64_t timestamp, int force_update, int do_render) {
	static AVPacket packet;

	if (!buffer) {
		return;
	}

	if (!scan_complete || !current_file) {
		render_empty_frame(do_render);
		return;
	}

	if (timestamp - file_frame_offset < 0) timestamp=0;
	else if(timestamp - file_frame_offset >= frames) timestamp = frames - 1;

	if (!force_update && dispFrame == timestamp) return;

	if(want_verbose)
		fprintf(stdout, "\t\t\t\tdisplay:%07"PRId64"  \r", timestamp);

	dispFrame = timestamp;

	if (OSD_mode&OSD_FRAME)
		snprintf(OSD_frame, 48, "Frame: %"PRId64, dispFrame);
	if (OSD_mode&OSD_SMPTE)
		frame_to_smptestring(OSD_smpte, dispFrame - ts_offset);

	if(fFirstTime) {
		fFirstTime=0;
#ifndef HAVE_AV_INIT_PACKET
		memset(&packet, 0, sizeof(AVPacket));
#else
		av_init_packet(&packet);
		packet.data = NULL;
		packet.size = 0;
#endif
	}

	if (pFrameFMT && !seek_frame (&packet, timestamp)) {
		/* Decode video frame */
		while (1) {
			int frameFinished = 0;
			if(packet.stream_index == videoStream) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
				avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet.data, packet.size);
#else
				avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
#endif
			}

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
					render_buffer(buffer);
				av_free_packet(&packet);
				break;
			}
			else
			{
				av_free_packet (&packet);
				if(av_read_frame (pFormatCtx, &packet) < 0) {
					if (!want_quiet)
						fprintf(stderr, "Read failed (during decode)\n");
					// read error or EOF
					reset_video_head (&packet);
					render_empty_frame(do_render);
					break;
				}
#ifdef USE_DUP_PACKET
				if (av_dup_packet (&packet) < 0) {
					fprintf(stderr, "Error: Cannot allocate video packet.\n");
					break;
				}
#endif
			}
		} /* end while !frame_finished */
	}
	else
	{
		// seek failed of no format
		if (pFrameFMT && want_debug) printf("DEBUG: frame seek unsucessful.\n");
		render_empty_frame(do_render);
		last_decoded_frame = -1;
	}
}

int close_movie() {
	if(current_file)
		free(current_file);
	current_file=NULL;

	cancel_index_thread();
	free(fidx);
	fidx = NULL;

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
	framerate = 5;
	return (0);
}
