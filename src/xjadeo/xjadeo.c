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
#include <assert.h>

#include "remote.h"
#include "gtime.h"

#ifndef MIN
#define MIN(A,B) (((A)<(B)) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) (((A)>(B)) ? (A) : (B))
#endif

//#define USE_DUP_PACKET // needed for some codecs with old ffmpeg

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
extern int      keyframe_interval_limit;
extern int      want_noindex;
#ifdef HAVE_LTC
extern int  use_ltc;
#endif

// On screen display
extern char OSD_frame[48];
extern char OSD_smpte[20];
extern int  OSD_mode;
extern char OSD_msg[128];
extern char OSD_nfo_tme[5][48];
extern char OSD_nfo_geo[5][48];
uint64_t    osd_smpte_ts;
uint64_t    osd_vtc_oob;

//------------------------------------------------
// globals
//------------------------------------------------

#ifdef TIMEMAP
int64_t timeoffset = 0;
double  timescale = 1.0;
int     wraparound = 0;
#endif

struct FrameIndex {
	int64_t pkt_pts;
	int64_t pkt_pos;
	int64_t frame_pts;
	int64_t frame_pos;
	int64_t timestamp; //< corresponds to array's [i]
	//int64_t frame_num; //< corresponds to frame_pts;
	int64_t seekpts;
	int64_t seekpos;
	uint8_t key;
};

static struct FrameIndex *fidx = NULL;

static int64_t last_decoded_pts = -1;
static int64_t last_decoded_frameno = -1;
static int64_t fcnt = 0;
static int seek_threshold = 8;
static uint8_t abort_indexing = 0;
static uint8_t scan_complete = 0;
static uint8_t thread_active = 0;
static uint8_t byte_seek = 0;
static uint8_t pts_warn = 0;

static pthread_t index_thread;

static AVRational fr_Q = { 1, 1 };
static int64_t    one_frame;
static int        fFirstTime=1;
static uint8_t    syncnidx = 0;
static const char syncname[4][5] =
{
	"CTL ",
	"JCK ",
	"LTC ",
	"MTC "
};


#ifdef JACK_SESSION
extern int jack_session_restore;
extern int js_winx;
extern int js_winy;
extern int js_winw;
extern int js_winh;
#endif

static void js_apply () {
#ifdef JACK_SESSION
	if (jack_session_restore) {
		jack_session_restore = 0;
		if (js_winx > 0 && js_winy > 0)
			Xposition (js_winx, js_winy);
		if (js_winw > 0 && js_winh > 0)
			Xresize (js_winw,js_winh);
	}
#endif
}

static int select_sleep (const long usec) {
	int remote_activity = 0;
#ifndef PLATFORM_WINDOWS
	fd_set fd;
	int max_fd = 0;
	struct timeval tv = { 0, 0 };
	if (usec > 500) {
		tv.tv_sec = usec / 1000000L;
		tv.tv_usec = (usec % 1000000L);
	}
	FD_ZERO(&fd);
	if (remote_en) {
		max_fd = remote_fd_set (&fd);
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
	remote_activity |= xjosc_process ();
#endif
#ifndef PLATFORM_WINDOWS
	if (remote_activity) {
		tv.tv_sec = 0; tv.tv_usec = 1;
	}
#endif
#ifdef PLATFORM_WINDOWS
	if ((!remote_en || remote_read_h()) && usec > 1000) {
		Sleep ((usec + 999) / 1000); // XXX not nearly good enough.
	}
#else
	if (select (max_fd, &fd, NULL, NULL, &tv)) {
		remote_read_io();
		return 1;
	}
#endif
	return remote_activity;
}

//--------------------------------------------
// main event loop
//--------------------------------------------
static void cancel_index_thread (void);
uint8_t splashed = 0;

void event_loop (void) {
	double  elapsed_time;
	int64_t clock1, clock2;
	int64_t newFrame, offFrame;
	float   nominal_delay;
	int64_t splash_timeout;
	uint8_t prev_syncidx = 0xff;

	splashed = want_nosplash;
	force_redraw = 1;

	if (want_verbose) printf("\nentering video update loop @%.2f fps.\n",delay>0?(1.0/delay):framerate);
	clock1 = xj_get_monotonic_time();
	splash_timeout = clock1 + 2500000; // 2.5 sec;

	while (loop_flag) { /* MAIN LOOP */
		uint8_t we_know_transport_is_not_rolling = 0;

		if (loop_run == 0) {
			/* video offline - (eg. window minimized)
			 * do not update frame
			 */
			select_sleep (2e5L);
			handle_X_events();
			js_apply();
			continue;
		}

#ifdef HAVE_MIDI
		if (midi_connected()) { newFrame = midi_poll_frame(); syncnidx = 3; }
		else
#endif
#ifdef HAVE_LTC
		if (ltcjack_connected()) { newFrame = ltc_poll_frame(); syncnidx = 2; }
		else
#endif
		{
			uint8_t jack_rolling = 1;
			newFrame = jack_poll_frame(&jack_rolling);
			syncnidx = 1;
			if (!jack_rolling)
				we_know_transport_is_not_rolling = 1;
		}

		if (newFrame < 0) {
			syncnidx = 0;
			newFrame = userFrame;
		}

		if (prev_syncidx != syncnidx) {
			force_redraw = 1;
			osd_smpte_ts = -1;
			osd_vtc_oob  = -1;
			prev_syncidx = syncnidx;
		}

#if 0 // experimental PLL
		static uint8_t dll_initialized = 0;
		static double dll_e2, dll_e0;
		static double dll_t0, dll_t1;
		static double dll_b, dll_c;
		static double dll_frameno;
		static int64_t prevFrame = 0;
		if (!scan_complete) dll_initialized = 0;
		if (syncnidx != 3) {
			// USE DLL to smooth over large jack cycles
			// with jack -> we know transport rolling state -> use it
			// this still jitters on start/stop,:( more work is needed.
			if (newFrame < prevFrame || newFrame > prevFrame + 4 || we_know_transport_is_not_rolling || !dll_initialized) {
				// reset DLL
				dll_initialized = 1;
				dll_e0 = dll_t0 = 0;
				dll_e2 = (delay > 0) ? (.5 * framerate * (float)delay) : .2;
				dll_t1 = newFrame + dll_e2;

				const double omega = 2. * M_PI / framerate;
				dll_b = 1.4142135623730950488 * omega; // sqrt(2)
				dll_c = omega * omega;
				if (!want_quiet && !we_know_transport_is_not_rolling)
					printf("RE-INIT DLL %g %lld <> %lld\n", dll_e2, prevFrame, newFrame);
				prevFrame = newFrame;
			} else {
				double expect = prevFrame + (xj_get_monotonic_time() - clock1) * framerate / 1000000.f;
				dll_e0 = expect - dll_t1;
				dll_t0 = dll_t1;
				dll_t1 += dll_b * dll_e0 + dll_e2;
				dll_e2 += dll_c * dll_e0;
				//printf("%.1f %+7.4f %lld %s\n", dll_t1, dll_e0, newFrame,  floor(dll_t1) != newFrame ? "!":"");
				prevFrame = newFrame;
				newFrame = floor(dll_t1);
			}
		}
#endif

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
		newFrame = floor ((double)newFrame * timescale) + timeoffset;
		// TODO: calc newFrames/frames instead of while-loop
		while (newFrame > frames && wraparound && frames != 0)
			newFrame -= frames;
		while (newFrame < 0 && wraparound && frames != 0)
			newFrame += frames;
#endif

		offFrame = newFrame + ts_offset;
		int64_t curFrame = dispFrame;
		const int fd = force_redraw;
		force_redraw = 0;
		display_frame (offFrame, fd);

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
			if (splash_timeout <= clock1) {
				splashed = 1;
				force_redraw = 1;
			}
		}

		if (want_verbose) {
#if 0
			fprintf(stdout, "frame: smpte:%"PRId64"    \r", newFrame);
#else
			char tempsmpte[15];
			frame_to_smptestring (tempsmpte, newFrame, 1);
			fprintf(stdout, "smpte: %s f:%"PRId64"\r", tempsmpte, newFrame);
#endif
			fflush (stdout);
		}

		handle_X_events();
		js_apply();

		clock2 = xj_get_monotonic_time();
		nominal_delay *= 1000000.f;
		elapsed_time = (clock2 - clock1);
		if (elapsed_time < nominal_delay) {
			long microsecdelay = (long) floorf (nominal_delay - elapsed_time);
#if 0 // debug timing
			printf("  %7.1f ms, [%"PRId64"]\n", microsecdelay / 1e3, offFrame);
#endif
#if 1 // poll 5 times per frame, unless -f delay is given explicitly
			const long pollinterval = ceilf (nominal_delay * .2f);
			if (microsecdelay > pollinterval && delay <= 0) microsecdelay = pollinterval;
#endif
			if (!select_sleep (microsecdelay)) {
				; // remote event occured
			}
			if (curFrame != dispFrame) {
				clock1 = clock2;
			}
		}
		else {
			clock1 = clock2;
#if 0 // debug timing
			printf("@@ %7.1f ms [%"PRId64"]\n", (nominal_delay - elapsed_time) / 1e3, offFrame);
#endif
		}
	}

	if ((remote_en||mq_en||ipc_queue) && (remote_mode&4)) {
		// send current settings
		xapi_pfullscreen (NULL);
		xapi_pontop (NULL);
		xapi_posd (NULL);
		xapi_pletterbox (NULL);
		xapi_pwinpos (NULL);
		xapi_pwinsize (NULL);
		xapi_poffset (NULL);
	}
	cancel_index_thread();
}


//--------------------------------------------
// Video file, rendering and ffmpeg inteface
//--------------------------------------------

static void render_empty_frame (int blit, int splashagain);
static uint8_t displaying_valid_frame = 0;

static int vbufsize = 0;

size_t video_buffer_size() {
	return vbufsize;
}

void init_moviebuffer (void) {
	if (buffer) free (buffer);
	if (want_debug)
		printf("DEBUG: init_moviebuffer - render_fmt: %i\n",render_fmt);
	/* Determine required buffer size and allocate buffer */
#ifdef CROPIMG
	vbufsize = avpicture_get_size (render_fmt, movie_width*2, movie_height);
#else
	vbufsize = avpicture_get_size (render_fmt, movie_width, movie_height);
#endif
	buffer = (uint8_t *)calloc (1, vbufsize);

	// Assign appropriate parts of buffer to image planes in pFrameFMT
	if (pFrameFMT) {
		avpicture_fill ((AVPicture *)pFrameFMT, buffer, render_fmt, movie_width, movie_height);
		pSWSCtx = sws_getContext (pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, movie_width, movie_height, render_fmt, SWS_BICUBIC, NULL, NULL, NULL);
	}
	render_empty_frame (0, 0);
}

void avinit (void) {
	av_register_all ();
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 20, 0)
	avcodec_init ();
#endif
	avcodec_register_all ();
	if (!want_avverbose) av_log_set_level (AV_LOG_QUIET);
}

static void reset_index () {
	last_decoded_pts = -1;
	last_decoded_frameno = -1;
	fcnt = 0;
	seek_threshold = 8;
	abort_indexing = 0;
	scan_complete = 0;
	byte_seek = 0;
}

static uint64_t parse_pts_from_frame (AVFrame *f) {
	uint64_t pts = AV_NOPTS_VALUE;

	pts = AV_NOPTS_VALUE;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51, 49, 100)
	if (pts == AV_NOPTS_VALUE) {
		pts = av_frame_get_best_effort_timestamp (f);
		if (pts != AV_NOPTS_VALUE) {
			if (!(pts_warn & 1) && !want_quiet)
				fprintf(stderr, "PTS: Best effort.\n");
			pts_warn |= 1;
		}
	}
#else
#warning building with libavutil < 51.49.100 is highly discouraged
#endif

	if (pts == AV_NOPTS_VALUE) {
		pts = f->pkt_pts;
		if (pts != AV_NOPTS_VALUE) {
			if (!(pts_warn & 2) && !want_quiet)
				fprintf(stderr, "Used PTS from packet instead frame's PTS.\n");
			pts_warn |= 2;
		}
	}

	if (pts == AV_NOPTS_VALUE) {
		pts = f->pts; // sadly bogus with many codecs :(
		if (pts != AV_NOPTS_VALUE) {
			if (!(pts_warn & 8) && !want_quiet)
				fprintf(stderr, "Used AVFrame assigned pts (instead frame PTS).\n");
			pts_warn |= 8;
		}
	}

	if (pts == AV_NOPTS_VALUE) {
		pts = f->pkt_dts;
		if (pts != AV_NOPTS_VALUE) {
			if (!(pts_warn & 4) && !want_quiet)
				fprintf(stderr, "Used decode-timestamp from packet (instead frame PTS).\n");
			pts_warn |= 4;
		}
	}

	return pts;
}

static int seek_frame (AVPacket *packet, int64_t framenumber) {
	if (!scan_complete) return -1;
	if (videoStream < 0) return -1;

	if (want_ignstart) {
		framenumber += file_frame_offset;
	}

	if (framenumber < 0 || framenumber >= fcnt) {
		return -1;
	}

	const int64_t timestamp = fidx[framenumber].timestamp;

	if (timestamp < 0 || framenumber >= frames) {
		return -1;
	}

	if (last_decoded_pts == timestamp) {
		assert(last_decoded_frameno == framenumber);
		return 0;
	}

	int need_seek = 0;
	if (last_decoded_pts < 0 || last_decoded_frameno < 0) {
		need_seek = 1;
		assert(last_decoded_pts < 0);
		assert(last_decoded_frameno < 0);
	} else if (last_decoded_pts > timestamp) {
		assert(last_decoded_frameno > framenumber);
		need_seek = 1;
	} else if ((framenumber - last_decoded_frameno) == 1) {
		; // don't seek for consecutive frames
	} else if (fidx[framenumber].seekpts != fidx[last_decoded_frameno].seekpts) {
		need_seek = 1;
	}

	last_decoded_pts = -1;
	last_decoded_frameno = -1;

	if (need_seek) {
		int seek;
		if (byte_seek && fidx[framenumber].seekpos > 0) {
#if 0 // DEBUG
			printf("Seek to POS: %"PRId64"\n", fidx[framenumber].seekpos);
#endif
			seek = av_seek_frame (pFormatCtx, videoStream, fidx[framenumber].seekpos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE);
		} else {
#if 0 // DEBUG
			printf("Seek to PTS: %"PRId64"\n", fidx[framenumber].seekpts);
#endif
			seek = av_seek_frame (pFormatCtx, videoStream, fidx[framenumber].seekpts, AVSEEK_FLAG_BACKWARD);
		}

		if (pCodecCtx->codec->flush) {
			avcodec_flush_buffers (pCodecCtx);
		}

		if (seek < 0) {
			if (!want_quiet)
				fprintf(stderr, "SEEK FAILED\n");
			return -3; // ERR
		}
	}

	int bailout = 2 * seek_threshold;
	while (bailout > 0) {
		int err;
		if ((err = av_read_frame (pFormatCtx, packet)) < 0) {
			if (err != AVERROR_EOF) {
				if (!want_quiet)
					fprintf(stderr, "Read failed (during seek)\n");
				av_free_packet (packet);
				return -1;
			} else {
				--bailout;
			}
		}
		if (packet->stream_index != videoStream) {
			av_free_packet (packet);
			continue;
		}

#ifdef USE_DUP_PACKET
			if (av_dup_packet (&packet) < 0) {
				if (!want_quiet)
					fprintf(stderr, "Error: Cannot allocate video packet.\n");
				break;
			}
#endif

#if 0 // DEBUG
		const int key = (packet->flags & AV_PKT_FLAG_KEY) ? 1 : 0;
#endif

		int frameFinished = 0;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
		err = avcodec_decode_video (pCodecCtx, pFrame, &frameFinished, packet->data, packet->size);
#else
		err = avcodec_decode_video2 (pCodecCtx, pFrame, &frameFinished, packet);
#endif

		av_free_packet (packet);

		if (err < 0) {
			if (!want_quiet)
				fprintf(stderr, "Decompression failed.\n");
			return -10;
		}

		if (!frameFinished) {
			--bailout;
			continue;
		}

		int64_t pts = parse_pts_from_frame (pFrame);
#if 0 // DEBUG
		printf("DECODE: WANT %"PRId64", GOT: %"PRId64" KEY %s\n", timestamp, pts, key ? "Y" : "N");
#endif

		if (pts == AV_NOPTS_VALUE) {
			if (!want_quiet)
				fprintf(stderr, "No presentation timestamp (PTS) for video frame.\n");
			return -7;
		}

		const int64_t prefuzz = one_frame > 10 ? 1 : 0;
		if (pts + prefuzz >= timestamp) {

#if 0 // DEBUG
			if (want_debug && pts != timestamp) {
				printf("PTS mismatch  WANT: %"PRId64" <> GOT: %"PRId64" KEY %s\n",
						timestamp, pts, key ? "Y" : "N");
			}
#endif

			// fuzzy match
			if (pts - timestamp < one_frame) {
				last_decoded_pts = timestamp;
				last_decoded_frameno = framenumber;
				return 0; // OK
			}

			if (!want_quiet) {
				fprintf(stderr, "Cannot reliably seek to target frame:\n");
				fprintf(stderr, " PTS mismatch want: %"PRId64" got: %"PRId64" %s\n", timestamp, pts, need_seek ?"did-seek":"no-seek");
			}
			return -2;
		}

#if 0 // DEBUG
		if (want_debug) {
			printf("DECODE >> WANT: %"PRId64" <> GOT: %"PRId64" KEY %s\n",
					timestamp, pts, key ? "Y" : "N");
		}
#endif

		--bailout;
	}

	if (!want_quiet)
		fprintf(stderr, "Index-seek: bail out. frame-distance too large.\n");
	return -5;
}

float index_progress = 0;

static void report_idx_progress (const char *msg, float percent) {
	static int lastval = 0;
	if (!(OSD_mode & OSD_MSG)) return;
	if (floorf (percent) == lastval) return; // also check msg?
	lastval = floorf (percent);
	index_progress = percent;
	sprintf(OSD_msg, "%s%3.0f%%", msg, percent);
	force_redraw = 1;
}

static int add_idx (int64_t ts, int64_t pos, uint8_t key, int _duration, AVRational tb) {
	if (fcnt >= frames) {
		++fcnt;
		if (!want_quiet)
			fprintf(stderr, "Index table Overflow: %"PRId64" / %"PRId64" frames.\n", fcnt, frames);
		fidx = realloc (fidx, fcnt * sizeof(struct FrameIndex));
		return -1;
	}
	report_idx_progress ("Pass 1: Scanning File:", 100.f * fcnt / frames);

	fidx[fcnt].pkt_pts = ts;
	fidx[fcnt].pkt_pos = pos;
	fidx[fcnt].timestamp = av_rescale_q (fcnt, fr_Q, tb);
	fidx[fcnt].key = key;

	fidx[fcnt].frame_pts = -1;
	fidx[fcnt].frame_pos = -1;
	fidx[fcnt].seekpts = 0;
	fidx[fcnt].seekpos = 0;
#if 0 // DEBUG
	if (fcnt < 50 || key)
	printf("IDX %"PRId64" PKT-PTS %"PRId64"  TS %"PRId64"\n", fcnt, fidx[fcnt].pkt_pts, fidx[fcnt].timestamp);
#endif
	++fcnt;
	return 0;
}

static int64_t keyframe_lookup_helper (const int64_t last, const int64_t ts) {
	int64_t i;
	assert(last < fcnt);
	for (i = last; i >= 0; --i) {
		if (!fidx[i].key) continue;
		if (fidx[i].pkt_pts == AV_NOPTS_VALUE || fidx[i].frame_pts == AV_NOPTS_VALUE) {
			continue;
		}
		if (fidx[i].frame_pts <= ts) {
			return i;
		}
	}
	return -1;
}

static int index_frames () {
	AVPacket packet;
	int      use_dts = 0;
	int      error = 0;

#ifndef HAVE_AV_INIT_PACKET
	memset (&packet, 0, sizeof(AVPacket));
#else
	av_init_packet (&packet);
#endif
	packet.data = NULL;
	packet.size = 0;

	int max_keyframe_interval = 0;
	int keyframe_interval = 0;
	int64_t keyframe_byte_pos = 0;
	int64_t keyframe_byte_distance = 0;

	AVRational const tb = pFormatCtx->streams[videoStream]->time_base;

	if (!want_noindex && want_verbose) {
		printf("Indexing Video...\n");
	}

	pts_warn = 0;
	/* pass 1: read all packets
	 * -> find keyframes
	 * -> check if file is complete
	 * -> discover max. keyframe distance
	 * -> get PTS/DTS of every *packet*
	 */
	while (!want_noindex && av_read_frame (pFormatCtx, &packet) >= 0) {
		if (abort_indexing) {
			if (!want_quiet) fprintf(stderr, "Indexing aborted.\n");
			av_free_packet (&packet);
			return -1;
		}
#ifdef USE_DUP_PACKET
		if (av_dup_packet (&packet) < 0) {
			if (!want_quiet)
				fprintf(stderr, "Error: Cannot allocate video packet.\n");
			break;
		}
#endif
		if (packet.stream_index != videoStream) {
			av_free_packet (&packet);
			continue;
		}

		int64_t ts = AV_NOPTS_VALUE;

		if (!use_dts && packet.pts != AV_NOPTS_VALUE)
			ts = packet.pts;
		if (ts == AV_NOPTS_VALUE) {
			if (!use_dts && want_verbose) {
				printf("Index: switch to DTS @ %"PRId64"\n", fcnt);
			}
			use_dts = 1;
		}
		if (use_dts && packet.dts != AV_NOPTS_VALUE)
			ts = packet.dts;

		if (ts == AV_NOPTS_VALUE) {
			if (!want_quiet)
				fprintf(stderr, "Index error: no PTS, nor DTS.\n");
			av_free_packet (&packet);
			error |= 1;
			break;
		}

		const uint8_t key = (packet.flags & AV_PKT_FLAG_KEY) ? 1 : 0;
		if (add_idx (ts, packet.pos, key, packet.duration, tb)) {
			av_free_packet (&packet);
			break;
		}

		if (key) {
			int byte_distance =  packet.pos - keyframe_byte_pos;
			keyframe_byte_pos = packet.pos;
			if (keyframe_byte_distance < byte_distance) {
				keyframe_byte_distance = byte_distance;
			}
		}

		av_free_packet (&packet);

		if (++keyframe_interval > max_keyframe_interval) {
			max_keyframe_interval = keyframe_interval;
		}
		if (max_keyframe_interval > keyframe_interval_limit &&
				(keyframe_byte_distance > 0 && keyframe_byte_distance > 5242880 /* 5 MB */)
			 )
		{
			error |=4;
			break;
		}
#if 1
		if ((fcnt == 500 || fcnt == frames) && max_keyframe_interval == 1 &&
				file_frame_offset == av_rescale_q (fidx[0].pkt_pts, tb, fr_Q)
			 )
		{
			if (want_verbose)
				printf("First 500 frames are all keyframes. Index disabled. Direkt seek mode enabled.\n");
			break;
		}
#endif
		if (key) {
			keyframe_interval = 0;
		}
	}

	pts_warn = 0;
	int64_t i;
	int64_t keyframecount = 0; // debug, info only.

	if (want_noindex ||
			(
			 (fcnt == 500 || fcnt == frames) && max_keyframe_interval == 1 &&
			 file_frame_offset == av_rescale_q (fidx[0].pkt_pts, tb, fr_Q)
			)
		 )
	{
		const int64_t pts_offset = fidx[0].pkt_pts;
		for (i = 0; i < frames; ++i) {
			fidx[i].key = 1;
			fidx[i].pkt_pts = fidx[i].frame_pts = pts_offset + av_rescale_q (i, fr_Q, tb);
			fidx[i].frame_pos = -1;
			fidx[i].timestamp = av_rescale_q (file_frame_offset + i, fr_Q, tb);
		}
		fcnt = frames;
		keyframecount = frames;
	}

	else

	/* pass 2: verify keyframes
	 * seek to [all] keyframe, decode one frame after
	 * the keyframe and check *frame* PTS
	 */

	// TODO: Check if one could skip this process in part
	// for [long] files where a pattern (constant offset) is detected.
	for (i = 0; i < fcnt; ++i)
	{
		if (abort_indexing) {
			if (!want_quiet) fprintf(stderr, "Indexing aborted.\n");
			return -1;
		}
		if (!fidx[i].key) continue;

		report_idx_progress ("Pass 2: Indexing Frames:", 100.f * i / fcnt);

		int got_pic = 0;
		int64_t pts = AV_NOPTS_VALUE;
		if (av_seek_frame (pFormatCtx, videoStream, fidx[i].pkt_pts, AVSEEK_FLAG_BACKWARD)) {
			fprintf(stderr, "IDX2: Seek failed.\n");
			error |= 16;
			break;
		}
		if (pCodecCtx->codec->flush) {
			avcodec_flush_buffers (pCodecCtx);
		}

		int err = 0;
		int bailout = 100;
		while (!got_pic && --bailout) {

			if ((err = av_read_frame (pFormatCtx, &packet)) < 0) {
				if (err == AVERROR_EOF) {
					fprintf(stderr, "IDX2: Read/Seek compensate for premature EOF\n");
					fidx[i].key = 0;
					av_free_packet (&packet);
					break;
				}
				fprintf(stderr, "IDX2: Read failed @ %"PRId64" / %"PRId64".\n", i, fcnt);
				error |= 32;
				break;
			}

#ifdef USE_DUP_PACKET
			if (av_dup_packet (&packet) < 0) {
				if (!want_quiet)
					fprintf(stderr, "Error: Cannot allocate video packet.\n");
				break;
			}
#endif
			if (packet.stream_index==videoStream) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
				err = avcodec_decode_video (pCodecCtx, pFrame, &got_pic, packet.data, packet.size);
#else
				err = avcodec_decode_video2 (pCodecCtx, pFrame, &got_pic, &packet);
#endif
			}
			av_free_packet (&packet);

			if (err < 0) {
				break;
			}

			if (!got_pic) {
				continue;
			}

			pts = parse_pts_from_frame (pFrame);

			if (pts == AV_NOPTS_VALUE) {
				err = -1;
				if (!want_quiet)
					fprintf(stderr, "No presentation timestamp (PTS) for video frame.\n");
				break;
			}
		}

		if (err < 0 || !bailout) continue;

		fidx[i].frame_pts = pts;
		fidx[i].frame_pos = av_frame_get_pkt_pos (pFrame);
		if (pts != AV_NOPTS_VALUE) {
#if 0 // DEBUG
			printf("FN %"PRId64", PKT-PTS %"PRId64" FRM-PTS: %"PRId64"\n", i, fidx[i].pkt_pts, fidx[i].frame_pts);
#endif
			++keyframecount;
		}
	}

	if (!want_quiet) {
		const int64_t ppts_offset = fidx[0].pkt_pts;
		const int64_t fpts_offset = fidx[0].frame_pts;
		if (file_frame_offset != av_rescale_q (ppts_offset, tb, fr_Q)) {
			fprintf(stderr, "FILE OFFSET MISMATCH %"PRId64" vs PKT-PTS: %"PRId64"\n",
					file_frame_offset, av_rescale_q (ppts_offset, tb, fr_Q));
		}
		if (file_frame_offset != av_rescale_q (fpts_offset, tb, fr_Q)) {
			fprintf(stderr, "FILE OFFSET MISMATCH %"PRId64" vs FRM-PTS: %"PRId64"\n",
					file_frame_offset, av_rescale_q (fpts_offset, tb, fr_Q));
		}
	}

	if (want_verbose)
		fprintf(stdout, "Good Keyframes %"PRId64"\n", keyframecount);

	/* pass 3: Create Seek-Table
	 * -> assign seek-[key]frame to every frame
	 */
	for (i = 0; i < fcnt; ++i) {
		if (abort_indexing) {
			if (!want_quiet) fprintf(stderr, "Indexing aborted.\n");
			return -1;
		}
		report_idx_progress ("Pass 3: Creating Index:", 100.f * i / fcnt);

		int64_t kfi = keyframe_lookup_helper (MIN(fcnt - 1, i + 2 + max_keyframe_interval), fidx[i].timestamp);
		if (kfi < 0) {
			if (!want_quiet)
				fprintf(stderr, "Cannot find keyframe for %"PRId64" %"PRId64"\n", i, fidx[i].timestamp);
			fidx[i].seekpts = 0;
			fidx[i].seekpos = 0;
		} else {
			//fprintf(stderr, "using keyframe %"PRId64" for %"PRId64"\n", kfi, fidx[i].timestamp);
			fidx[i].seekpts = fidx[kfi].pkt_pts;
			fidx[i].seekpos = fidx[kfi].frame_pos;
		}
	}

#if 0 // DEBUG, TESTING
	// check if byte-seeking is OK
	byte_seek = 1;
	srandom (time (NULL));
	for (i = 0; i < 10 && byte_seek; ++i) {
		int got_pic = 0;
		int64_t n = random () % fcnt; // pick some random frames
		if (fidx[n].seekpos < 0) {
			byte_seek = 0;
			printf("NOBYTE 1\n");
			break;
		}
		if (av_seek_frame (pFormatCtx, videoStream, fidx[n].seekpos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE) < 0) {
			byte_seek = 0;
			printf("NOBYTE 2\n");
			break;
		}
		if (pCodecCtx->codec->flush) {
			avcodec_flush_buffers (pCodecCtx);
		}

		int64_t pts = AV_NOPTS_VALUE;
		while (!got_pic) {
			if (av_read_frame (pFormatCtx, &packet) < 0) {
				byte_seek = 0;
				printf("NOBYTE 3\n");
				av_free_packet (&packet);
				break;
			}

#ifdef USE_DUP_PACKET
			if (av_dup_packet (&packet) < 0) {
				if (!want_quiet)
					fprintf(stderr, "Error: Cannot allocate video packet.\n");
				break;
			}
#endif
			int err = 0;
			if (packet.stream_index==videoStream) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
				err = avcodec_decode_video (pCodecCtx, pFrame, &got_pic, packet.data, packet.size);
#else
				err = avcodec_decode_video2 (pCodecCtx, pFrame, &got_pic, &packet);
#endif
			}

			av_free_packet (&packet);

			if (err < 0) {
				byte_seek = 0;
				printf("NOBYTE 4\n");
				break;
			}
			if (!got_pic) {
				//--bailout;
				continue;
			}
			pts = parse_pts_from_frame (pFrame);
		}
		if (fidx[n].timestamp < pts || pts == AV_NOPTS_VALUE) {
			printf("NOBYTE 5\n");
			byte_seek = 0;
			break;
		}
		if (fidx[n].seekpts > pts) {
			printf("NOBYTE 6\n");
			byte_seek = 0;
			break;
		}
	}
#endif

#if 0 // VERIFY -- DEBUG, TESTING
	for (i = 0; i < fcnt; ++i) {
		int got_pic = 0;
		printf("\t\t %"PRId64" / %"PRId64"    %s   \r",i, fcnt, fidx[i].seekpos > 0 ? "B" : "P"); fflush (stdout);
		int64_t pts = AV_NOPTS_VALUE;
		if (byte_seek && fidx[i].seekpos > 0) {
			av_seek_frame (pFormatCtx, videoStream, fidx[i].seekpos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE);
		} else {
			av_seek_frame (pFormatCtx, videoStream, fidx[i].seekpts, AVSEEK_FLAG_BACKWARD);
		}
		if (pCodecCtx->codec->flush) {
			avcodec_flush_buffers (pCodecCtx);
		}
		while (!got_pic) {

			if (av_read_frame (pFormatCtx, &packet) < 0) {
				fprintf(stderr, "IDX2: Read failed.\n");
				av_free_packet (&packet);
				break;
			}

#ifdef USE_DUP_PACKET
			if (av_dup_packet (&packet) < 0) {
				if (!want_quiet)
					fprintf(stderr, "Error: Cannot allocate video packet.\n");
				break;
			}
#endif
			if (packet.stream_index==videoStream) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 21, 0)
				avcodec_decode_video (pCodecCtx, pFrame, &got_pic, packet.data, packet.size);
#else
				avcodec_decode_video2 (pCodecCtx, pFrame, &got_pic, &packet);
#endif
			}
			av_free_packet (&packet);
			if (!got_pic) {
				//--bailout;
				continue;
			}
			pts = parse_pts_from_frame (pFrame);
			if (pts == AV_NOPTS_VALUE) {
				if (!want_quiet)
					fprintf(stderr, "No presentation timestamp (PTS) for video frame.\n");
				break;
			}
		}
		if (fidx[i].timestamp < pts) {
			printf("FAIL! fn:%d  want: %"PRId64", seek: %"PRId64" got:%"PRId64"\n",
					i, fidx[i].timestamp, fidx[i].seekpts, pts);
		}
	}
#endif

	if (want_noindex) {
		max_keyframe_interval = keyframe_interval_limit;
	}
	seek_threshold = MAX(2, max_keyframe_interval - 1);
	if (seek_threshold >= keyframe_interval_limit &&
			// TODO: relax the filter to use 'current'
			//  byte distance instead of global max
			// may be appropriate (for most files)
			keyframe_byte_distance > 5242880 /* 5 MB */
		 )
	{
		error |= 4;
		if (!want_quiet)
			fprintf(stderr,
					"WARNING: Keyframe distance is very large (>%d frames).\n"
					"The file is not unsuitable. Please transcode.\n",
					keyframe_interval_limit);
		seek_threshold = keyframe_interval_limit;
	}

	if (want_verbose) {
		printf("Scan complete err: %d use-dts: %s\n", error, use_dts ? "yes" : "no");
		printf("scanned %"PRId64" of %"PRId64" frames, key-int: %d seek-thresh: %d\n",
				fcnt, frames, max_keyframe_interval, seek_threshold);
		printf("max keyframe distance: %.1f kBytes\n",
				keyframe_byte_distance / 1024.f);
		printf("Seek by %s\n", byte_seek ? "Byte" : "PTS");
	}

	av_seek_frame (pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	if (pCodecCtx->codec->flush) {
		avcodec_flush_buffers (pCodecCtx);
	}
	if (!error) {
		scan_complete = 1;
	}
	return error;
}

static void *index_run (void *arg) {
	OSD_mode |= OSD_MSG | OSD_IDXNFO;
	OSD_mode &= ~(OSD_EQ | OSD_OFFF | OSD_OFFS);
	sprintf(OSD_msg, "Indexing. Please wait.");
	OSD_frame[0] = '\0';
	OSD_smpte[0] = '\0';
	osd_smpte_ts = -1;
	osd_vtc_oob  = -1;
	index_progress = 0;
	force_redraw = 1;
	if (!index_frames()) {
		OSD_mode &= ~OSD_MSG;
	} else {
		OSD_mode |= OSD_BOX;
		sprintf(OSD_msg, "Index Error. File is not suitable.");
	}
	OSD_mode &= ~OSD_IDXNFO;
	index_progress = -1;
	force_redraw = 1;
	pthread_exit (NULL);
	return (NULL);
}

static void cancel_index_thread (void) {
	if (!thread_active) return;
	abort_indexing = 1;
	pthread_join (index_thread, NULL);
	abort_indexing = 0;
	OSD_mode &= ~OSD_MSG;
	thread_active = 0;
	index_progress = -1;
	force_redraw = 1;
}

static int start_index_thread (void) {
	if (thread_active) {
		if (!want_quiet) fprintf(stderr, "Indexing thread is still active. Forcing Re-start.\n");
		cancel_index_thread ();
	}
	if (pthread_create (&index_thread, NULL, index_run, NULL)) {
		if (!want_quiet) fprintf(stderr, "Cannot launch index thread.\n");
		return -1;
	}
	thread_active = 1;
	return 0;
}

int have_open_file () {
	if (current_file && pFrameFMT)
		return 1;
	else
		return 0;
}

static void clear_info () {
	strcpy(OSD_nfo_tme[0], "-/- No File Open -\\-");
	OSD_nfo_tme[1][0] = '\0';
	OSD_nfo_tme[2][0] = '\0';
	OSD_nfo_tme[3][0] = '\0';
	OSD_nfo_tme[4][0] = '\0';
	strcpy(OSD_nfo_geo[0], "-/- No File Open -\\-");
	OSD_nfo_geo[1][0] = '\0';
	OSD_nfo_geo[2][0] = '\0';
	OSD_nfo_geo[3][0] = '\0';
	OSD_nfo_geo[4][0] = '\0';
	force_redraw = 1;
}

int open_movie (char* file_name) {
	int i;
	AVCodec		*pCodec;
	AVStream	*av_stream;

	if (pFrameFMT) {
		close_movie ();
	}

	OSD_mode &= ~OSD_MSG;
	reset_index ();

	/* set some defaults, in case open fails, the main-loop
	 * will still get some consistent data
	 */
	fFirstTime   = 1;
	pFrameFMT    = NULL;
	pFormatCtx   = NULL;
	movie_width  = ffctv_width = 640;
	movie_height = ffctv_height = 320;
	movie_aspect = (float)movie_width / (float) movie_height;
	duration     = 1;
	frames       = 1;
	framerate    = 10;
	one_frame    = 1;
	videoStream  = -1;
	file_frame_offset = 0;
	clear_info();

	// recalc offset with default framerate
	if (smpte_offset) {
		ts_offset = smptestring_to_frame (smpte_offset);
	}

	if (strlen (file_name) == 0) {
		return -1;
	}

	/* Open video file */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 7, 0)
	if (av_open_input_file (&pFormatCtx, file_name, NULL, 0, NULL)!=0)
#else
	if (avformat_open_input (&pFormatCtx, file_name, NULL, NULL)!=0)
#endif
	{
		if (!remote_en && !mq_en && !ipc_queue)
			if (!want_quiet) fprintf(stderr, "Cannot open video file '%s'\n", file_name);
		pFormatCtx=NULL;
		return (-1);
	}

	/* Retrieve stream information */
	if (avformat_find_stream_info (pFormatCtx, NULL) < 0) {
		if (!want_quiet) fprintf(stderr, "Cannot find stream information in file %s\n", file_name);
		avformat_close_input (&pFormatCtx);
		pFormatCtx=NULL;
		return (-1);
	}

	/* dump video information */
	if (!want_quiet) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 2, 0)
		dump_format (pFormatCtx, 0, file_name, 0);
#else
		av_dump_format (pFormatCtx, 0, file_name, 0);
#endif
	}

	/* Find the first video stream */
	for (i = 0; i < pFormatCtx->nb_streams; ++i)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	if (videoStream == -1) {
		if (!want_quiet) fprintf(stderr, "Cannot find a video stream in file %s\n", file_name);
		avformat_close_input (&pFormatCtx);
		pFormatCtx=NULL;
		return -1;
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
		if (fr.den > 0 && fr.num > 0) {
			framerate = av_q2d (av_stream->r_frame_rate);
			fr_Q.den = fr.num;
			fr_Q.num = fr.den;
		}
	}
#else
	{
		AVRational fr = av_stream_get_r_frame_rate (av_stream);
		if (fr.den > 0 && fr.num > 0) {
			framerate = av_q2d (fr);
			fr_Q.den = fr.num;
			fr_Q.num = fr.den;
		}
	}
#endif
	if (framerate < 1 || framerate > 1000) {
		AVRational fr = av_stream->avg_frame_rate;
		if (fr.den > 0 && fr.num > 0) {
			framerate = av_q2d (fr);
			fr_Q.den = fr.num;
			fr_Q.num = fr.den;
		}
	}
	if (framerate < 1 || framerate > 1000) {
		AVRational fr = av_stream->time_base;
		if (fr.den > 0 && fr.num > 0) {
			framerate = 1.0 / av_q2d (fr);
			fr_Q.den = fr.den;
			fr_Q.num = fr.num;
		}
	}
	if (framerate < 1 || framerate > 1000) {
		if (!want_quiet)
			fprintf(stderr, "WARNING: cannot determine video-frame rate, using 25fps.\n");
		framerate = 25;
		fr_Q.den = 25;
		fr_Q.num = 1;
	}

	// detect drop frame timecode
	if (fabs (framerate - 30000.0 / 1001.0) < 0.01) {
		have_dropframes=1;
		if (!want_quiet)
			fprintf(stdout, "enabled drop-frame-timecode (use -n to override).\n");
	}

	if (av_stream->nb_frames > 0) {
		frames = av_stream->nb_frames;
		duration = frames * av_q2d (fr_Q);
	} else {
		duration = pFormatCtx->duration / (double)AV_TIME_BASE;
		frames = pFormatCtx->duration * framerate / (double)AV_TIME_BASE;
	}

	one_frame = av_rescale_q (1, fr_Q, av_stream->time_base);

	if (pFormatCtx->start_time != AV_NOPTS_VALUE) {
		file_frame_offset = (int64_t) rint (framerate * (double) pFormatCtx->start_time / (double) AV_TIME_BASE);
	}

	fidx = malloc (frames * sizeof(struct FrameIndex));
	for (i = 0; i < frames; ++i) {
		fidx[i].pkt_pts = -1;
		fidx[i].pkt_pos = -1;
		fidx[i].key = 0;
	}

	// recalc offset with new framerate
	if (smpte_offset) {
		ts_offset = smptestring_to_frame (smpte_offset);
	}

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	if (!want_quiet) {
		fprintf(stdout, "frame rate: %g\n", framerate);
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
		sample_aspect = av_q2d (av_stream->sample_aspect_ratio);
	else if (av_stream->codec->sample_aspect_ratio.num)
		sample_aspect = av_q2d (av_stream->codec->sample_aspect_ratio);
	else
		sample_aspect = 1.0;

	movie_aspect = sample_aspect * (float)pCodecCtx->width / (float) pCodecCtx->height;

	/* calculate effective width, height */
	ffctv_height = movie_height;
	ffctv_width = ((int)rint (pCodecCtx->height * movie_aspect));
	if (ffctv_width > pCodecCtx->width) {
		ffctv_width = movie_width;
		ffctv_height = ((int)rint (pCodecCtx->width / movie_aspect));
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

#ifdef AVFMT_FLAG_GENPTS
	if (want_genpts)
		pFormatCtx->flags |= AVFMT_FLAG_GENPTS;
#endif

	if (!want_quiet) {
		fprintf(stderr, "display size: %ix%i px\n", movie_width, movie_height);
	}
	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder (pCodecCtx->codec_id);
	if (pCodec==NULL) {
		if (!want_quiet)
			fprintf(stderr, "Cannot find a codec for file: %s\n", file_name);
		avformat_close_input (&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return -1;
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		if (!want_quiet)
			fprintf(stderr, "Cannot open the codec for file %s\n", file_name);
		avformat_close_input (&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return -1;
	}

	pFrame=av_frame_alloc();
	if (pFrame == NULL) {
		if (!want_quiet)
			fprintf(stderr, "Cannot allocate video frame buffer\n");
		avcodec_close (pCodecCtx);
		avformat_close_input (&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return -1;
	}

	pFrameFMT=av_frame_alloc();
	if (pFrameFMT == NULL) {
		if (!want_quiet)
			fprintf(stderr, "Cannot allocate display frame buffer\n");
		av_free (pFrame);
		avcodec_close (pCodecCtx);
		avformat_close_input (&pFormatCtx);
		pFormatCtx = NULL;
		pCodecCtx = NULL;
		return -1;
	}

	char *tmp;
	if (have_dropframes)
		sprintf(OSD_nfo_tme[1], "FPS: %.2f df", framerate);
	else
		sprintf(OSD_nfo_tme[1], "FPS: %.3f", framerate);
	strcat(OSD_nfo_tme[2], "S: ");
	strcat(OSD_nfo_tme[3], "E: ");
	strcat(OSD_nfo_tme[4], "L: ");
	frame_to_smptestring(&OSD_nfo_tme[2][3], file_frame_offset, 1);
	frame_to_smptestring(&OSD_nfo_tme[3][3], file_frame_offset + frames - 1, 1);
	frame_to_smptestring(&OSD_nfo_tme[4][3], frames, 1);

	sprintf(OSD_nfo_geo[1], "PRESCALE: %d x %d", movie_width, movie_height);
	sprintf(OSD_nfo_geo[3], "GEOMETRY: %d x %d", ffctv_width, ffctv_height);

	if (av_stream->sample_aspect_ratio.num)
		sprintf(OSD_nfo_geo[2], "SAR: %d : %d",
				av_stream->sample_aspect_ratio.num, av_stream->sample_aspect_ratio.den);
	else if (av_stream->codec->sample_aspect_ratio.num)
		sprintf(OSD_nfo_geo[2], "SAR: %d : %d",
				av_stream->codec->sample_aspect_ratio.num, av_stream->codec->sample_aspect_ratio.den);
	else
		sprintf(OSD_nfo_geo[2], "SAR: unknown (1 : 1)");

	AVRational dar;
	av_reduce(&dar.num, &dar.den, ffctv_width, ffctv_height, 1024 * 1024);
	sprintf(OSD_nfo_geo[4], "DAR: %d : %d", dar.num, dar.den);

#ifdef PLATFORM_WINDOWS
	if ((tmp = strrchr(file_name, '\\')) && *++tmp)
#else
	if ((tmp = strrchr(file_name, '/')) && *++tmp)
#endif
	{
		strncpy(OSD_nfo_tme[0], tmp, sizeof(OSD_nfo_tme[3]) - 1);
		strncpy(OSD_nfo_geo[0], tmp, sizeof(OSD_nfo_tme[3]) - 1);
	} else {
		strncpy(OSD_nfo_tme[0], file_name, sizeof(OSD_nfo_tme[3]) - 1);
		strncpy(OSD_nfo_geo[0], file_name, sizeof(OSD_nfo_tme[3]) - 1);
	}
	OSD_nfo_tme[0][sizeof(OSD_nfo_tme[0]) - 3] = '.';
	OSD_nfo_tme[0][sizeof(OSD_nfo_tme[0]) - 2] = '.';
	OSD_nfo_tme[0][sizeof(OSD_nfo_tme[0]) - 1] = '\0';
	OSD_nfo_geo[0][sizeof(OSD_nfo_tme[0]) - 3] = '.';
	OSD_nfo_geo[0][sizeof(OSD_nfo_tme[0]) - 2] = '.';
	OSD_nfo_geo[0][sizeof(OSD_nfo_tme[0]) - 1] = '\0';

	current_file = strdup (file_name);
	x_fib_add_recent (current_file, time (NULL));

	start_index_thread();

	return 0;
}

static void render_empty_frame (int blit, int splashagain) {
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
		memset (buffer, 0, Ylen);
		memset (buffer + Ylen, 0x80, Ylen / 2);
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
		for (i = 0; i < movie_width * movie_height * 4; i += 4) {
			buffer[i]   = 0x00;
			buffer[i+1] = 0x00;
			buffer[i+2] = 0x00;
			buffer[i+3] = 0xff;
		}
	} else {
		memset (buffer, 0, avpicture_get_size (render_fmt, movie_width, movie_height));
	}
#ifdef DRAW_CROSS
	int x,y;
	if (render_fmt == PIX_FMT_UYVY422)
		for (x = 0, y = 0;x < movie_width - 1; ++x, y = movie_height * x / movie_width) {
			int off = (2 * x + 2 * movie_width * y);
			buffer[off]=127; buffer[off+1]=127;

			off = (2 * x + 2 * movie_width * (movie_height - y - 1));
			buffer[off]=127; buffer[off+1]=127;
		}
	if (render_fmt == PIX_FMT_YUV420P)
		for (x = 0, y = 0; x < movie_width - 1; ++x, y = movie_height * x / movie_width) {
			int yoff = (x + movie_width * y);
			buffer[yoff]=127; buffer[yoff+1]=127;

			yoff = (x + movie_width * (movie_height - y - 1));
			buffer[yoff]=127; buffer[yoff+1]=127;
		}
	if (render_fmt == PIX_FMT_RGB24)
		for (x = 0, y = 0; x < movie_width - 1; ++x, y = movie_height * x / movie_width) {
			int yoff = 3 * (x + movie_width * y);
			buffer[yoff]=127;
			buffer[yoff+1]=127;
			buffer[yoff+2]=127;
			yoff = 3 * (x + movie_width * (movie_height - y - 1));
			buffer[yoff]=127;
			buffer[yoff+1]=127;
			buffer[yoff+2]=127;
		}
	if (render_fmt == PIX_FMT_RGBA32 || render_fmt == PIX_FMT_BGRA32)
		for (x = 0, y = 0; x < movie_width - 1; ++x, y = movie_height * x / movie_width) {
			int yoff = 4 * (x + movie_width * y);
			buffer[yoff]=127;
			buffer[yoff+1]=127;
			buffer[yoff+2]=127;
			buffer[yoff+3]=255;
			yoff = 4 * (x + movie_width * (movie_height - y - 1));
			buffer[yoff]=127;
			buffer[yoff+1]=127;
			buffer[yoff+2]=127;
			buffer[yoff+3]=255;
		}
#endif
	if (!splashed || splashagain) {
		splash(buffer);
	}
	if (blit)
		render_buffer (buffer);
}

void display_frame (int64_t timestamp, int force_update) {
	static AVPacket packet;

	if (!buffer) {
		osd_smpte_ts = -1;
		osd_vtc_oob  = -1;
		displaying_valid_frame = 0;
		return;
	}
#if (defined DND && defined PLATFORM_LINUX) || (defined WINMENU && defined PLATFORM_WINDOWS)
	if (!current_file && !(OSD_mode & OSD_MSG) && getvidmode() != VO_SDL) {
		sprintf(OSD_msg, "[right-click]");
		OSD_mode |= OSD_MSG | OSD_BOX;
		OSD_mode &= ~OSD_EQ;
		OSD_frame[0] = '\0';
		OSD_smpte[0] = '\0';
		osd_smpte_ts = -1;
		osd_vtc_oob  = -1;
	}
#endif

	if (!scan_complete || !current_file) {
		OSD_frame[0] = '\0';
		int need_redisplay = force_update;
		if (OSD_mode&OSD_SMPTE) {
			if (osd_smpte_ts != timestamp - ts_offset)
				need_redisplay = 1;
			osd_smpte_ts = timestamp - ts_offset;
			strcpy(OSD_smpte, syncname[syncnidx]);
			frame_to_smptestring (&OSD_smpte[4], osd_smpte_ts, 0);
		}
		render_empty_frame (need_redisplay, !current_file && !want_nosplash);
		displaying_valid_frame = 0;
		return;
	}


	// want_ignstart here ??
	if (timestamp < 0 || timestamp >= frames) {
		OSD_frame[0] = '\0';
		int need_redisplay = force_update || displaying_valid_frame;
		if (OSD_mode&OSD_SMPTE) {
			if (osd_smpte_ts != timestamp - ts_offset)
				need_redisplay = 1;
			osd_smpte_ts = timestamp - ts_offset;
			strcpy(OSD_smpte, syncname[syncnidx]);
			frame_to_smptestring (&OSD_smpte[4], osd_smpte_ts, 0);
		}
		if ((OSD_mode & (OSD_VTC | OSD_VTCOOR)) == (OSD_VTC | OSD_VTCOOR)) {
			int64_t oob = timestamp;
			if (want_ignstart) {
				oob += file_frame_offset;
			}
			if (timestamp < 0) {
				strcpy(OSD_frame, "-- ");
				oob -= file_frame_offset;
			} else {
				strcpy(OSD_frame, "++ ");
				oob -= frames;
			}
			if (osd_vtc_oob != oob) {
				need_redisplay = 1;
			}
			osd_vtc_oob = oob;
			frame_to_smptestring (&OSD_frame[3], oob, 1);
			strcat(OSD_frame, " EOF");
		}
		render_empty_frame (need_redisplay, 0);
		displaying_valid_frame = 0;
		return;
	}

	if (!force_update && dispFrame == timestamp) return;

	if (want_verbose)
		fprintf(stdout, "\t\t\t\tdisplay:%07"PRId64"  \r", timestamp);

	dispFrame = timestamp;

	if (OSD_mode & (OSD_FRAME | OSD_VTC)) {
		int64_t dfn;
		if (want_ignstart) {
			dfn = dispFrame + file_frame_offset;
		} else {
			dfn = dispFrame;
		}
		if (OSD_mode&OSD_VTC) {
			frame_to_smptestring (&OSD_frame[0], dfn, 0);
		} else {
			snprintf(OSD_frame, 48, "F:%8"PRId64" ", dfn);
		}
	}
	if (OSD_mode&OSD_SMPTE) {
		strcpy(OSD_smpte, syncname[syncnidx]);
		osd_smpte_ts = dispFrame - ts_offset;
		frame_to_smptestring (&OSD_smpte[4], dispFrame - ts_offset, 0);
	}

	if (fFirstTime) {
		fFirstTime=0;
#ifndef HAVE_AV_INIT_PACKET
		memset (&packet, 0, sizeof(AVPacket));
#else
		av_init_packet (&packet);
		packet.data = NULL;
		packet.size = 0;
#endif
	}

	if (pFrameFMT && !seek_frame (&packet, timestamp)) {
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
		sws_scale (pSWSCtx, (const uint8_t * const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameFMT->data, dstStride);
		displaying_valid_frame = 1;
		if (!splashed) {
			splash(buffer);
		}
		render_buffer (buffer);
	}
	else
	{
		// seek failed of no format
		if (pFrameFMT && want_debug)
			printf("DEBUG: frame seek unsucessful.\n");
		render_empty_frame (force_update || displaying_valid_frame, 0);
		displaying_valid_frame = 0;
		last_decoded_pts = -1;
		last_decoded_frameno = -1;
	}
}

int close_movie () {
	if (current_file)
		free (current_file);
	current_file=NULL;

	cancel_index_thread();
	free (fidx);
	fidx = NULL;

	if (!pFrameFMT) return -1;
	// Free the software scaler
	sws_freeContext (pSWSCtx);

	// Free the formatted image
	if (buffer) free (buffer);
	buffer=NULL;
	if (pFrameFMT)
		av_free (pFrameFMT);
	pFrameFMT=NULL;

	//Free the YUV frame
	if (pFrame)
		av_free (pFrame);
	pFrame=NULL;

	//Close the codec
	avcodec_close (pCodecCtx);

	//Close the video file
	avformat_close_input (&pFormatCtx);
	duration = frames = 1;
	pCodecCtx = NULL;
	pFormatCtx = NULL;
	movie_width  = ffctv_width = 640;
	movie_height = ffctv_height = 320;
	movie_aspect = (float)movie_width / (float) movie_height;
	framerate = 5;
	OSD_frame[0] = '\0';
	OSD_smpte[0] = '\0';
	osd_smpte_ts = -1;
	osd_vtc_oob  = -1;
	clear_info();
	Xletterbox (Xgetletterbox());
	return (0);
}
