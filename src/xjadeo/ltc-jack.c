/* xjadeo - jack video monitor
 *
 * (c) 2010, 2011  Robin Gareus <robin@gareus.org>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#ifdef JACK_SESSION
#include <jack/session.h>
extern char *jack_uuid;
void jack_session_cb( jack_session_event_t *event, void *arg );
#endif

#if (defined HAVE_LTCSMPTE || defined HAVE_LTC)
extern double framerate;

#include <math.h>
#include <jack/jack.h>

void close_ltcjack(void);

jack_nframes_t j_samplerate = 48000;
jack_port_t *j_input_port = NULL;
jack_default_audio_sample_t *j_in;
jack_nframes_t j_latency = 0;
jack_client_t *j_client = NULL;

static double ltc_position = 0;
static long long int monotonic_fcnt = 0;

#ifdef HAVE_LTCSMPTE
#warning using deprecated libltcsmpte - get https://github.com/x42/libltc
#include <ltcsmpte/ltcsmpte.h>

static SMPTEDecoder *ltc_decoder = NULL;

static int myProcess(SMPTEDecoder *d, double *jt)  {
	SMPTEFrameExt frame;
#ifdef DEBUG
	int errors;
#endif
	int i=0; /* marker - if queue is flushed - don't read the last */
	int rv=0;
#if 0 /* process only last LTC (0: all in decoder queue) */
	while (SMPTEDecoderRead(d,&frame)) {i++;}
#endif
	while (i || SMPTEDecoderRead(d,&frame)) {
		SMPTETime stime;
		i=0;

		SMPTEFrameToTime(&frame.base,&stime);
#ifdef DEBUG
		SMPTEDecoderErrors(d,&errors);
#endif

		if (jt) {
			*jt=(double) (
					((stime.hours*60+stime.mins)*60 +stime.secs)*j_samplerate
					+ ((double)stime.frame*(double)j_samplerate/framerate)
					+ frame.startpos - monotonic_fcnt
					);
			//printf("LTC-debug %f %li %li\n",*jt,frame.startpos, frame.endpos);
		}

#ifdef DEBUG
		int ms;
		SMPTEDecoderFrameToMillisecs(d,&frame,&ms);
		printf("LTC: %02d:%02d:%02d:%02d %8d %d \n",
				stime.hours,stime.mins,
				stime.secs,stime.frame,
				ms,
				errors);
#endif
		++rv;
	}
	return rv;
}

#else // HAVE_LTC -- new libltc

#include <ltc.h>

static LTCDecoder *ltc_decoder = NULL;

static int myProcess(LTCDecoder *d, double *jt)  {
	LTCFrameExt frame;
	int rv=0;
	while (ltc_decoder_read(d,&frame)) {
		SMPTETimecode stime;
		ltc_frame_to_time(&stime, &frame.ltc, 0);

#ifdef DEBUG
		printf( "%02d:%02d:%02d%c%02d | %8lld %8lld%s \n",
				stime.hours,
				stime.mins,
				stime.secs,
				(frame.ltc.dfbit) ? '.' : ':',
				stime.frame,
				frame.off_start,
				frame.off_end,
				frame.reverse ? " R" : "  "
				);
#endif

		if (jt) {
			*jt = (double) (
					((stime.hours*60+stime.mins)*60 +stime.secs) * j_samplerate
					+ ((double)stime.frame*(double)j_samplerate / framerate)
					+ frame.off_end - monotonic_fcnt
					);
		}
		++rv;
	}
	return rv;
}
#endif

#ifdef NEW_JACK_LATENCY_API
static int jack_latency_cb(void *arg) {
	jack_latency_range_t jlty;
	jack_port_get_latency_range(j_input_port, JackCaptureLatency, &jlty);
	j_latency = jlty.max;
	return 0;
}
#endif

/**
 * jack audio process callback
 */
static int process (jack_nframes_t nframes, void *arg) {
	unsigned char sound[8192];
	size_t i;
	j_in = jack_port_get_buffer (j_input_port, nframes);

#ifndef NEW_JACK_LATENCY_API
	j_latency = jack_port_get_total_latency(j_client,j_input_port);
#endif
	if (nframes > 8192) return 0;

	for (i = 0; i < nframes; i++) {
	  const int snd=(int)rint((127.0*j_in[i])+128.0);
	  sound[i] = (unsigned char) (snd&0xff);
	}

#ifdef HAVE_LTCSMPTE
	SMPTEDecoderWrite(ltc_decoder, sound,nframes, monotonic_fcnt-j_latency);
#else // HAVE_LTC
	ltc_decoder_write(ltc_decoder, sound, nframes, monotonic_fcnt-j_latency);
#endif
	myProcess(ltc_decoder, &ltc_position);
	monotonic_fcnt += nframes;
	return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
static void ltcjack_shutdown (void *arg) {
	fprintf(stderr,"recv. shutdown request from jackd.\n");
	close_ltcjack();
}

/**
 * open a client connection to the JACK server
 */
static int init_jack(const char *client_name) {
	jack_status_t status;
	jack_options_t options = JackNullOption;
#ifdef JACK_SESSION
	if (jack_uuid)
		j_client = jack_client_open (client_name, options|JackSessionID, &status, jack_uuid);
	else
#endif
		j_client = jack_client_open (client_name, options, &status);
	if (j_client == NULL) {
		fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		return -1;
	}

	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(j_client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	jack_set_process_callback (j_client, process, 0);
#ifdef JACK_SESSION
	jack_set_session_callback (j_client, jack_session_cb, NULL);
#endif
#ifndef HAVE_WINDOWS
	jack_on_shutdown (j_client, ltcjack_shutdown, 0);
#endif
	j_samplerate=jack_get_sample_rate (j_client);
	return 0;
}

static int jack_portsetup(void) {
#ifdef HAVE_LTCSMPTE
	FrameRate *fps;
	fps = FR_create(1, 1, FRF_NONE);
	FR_setdbl(fps, framerate, 1); // auto-detect drop-frames
	ltc_decoder = SMPTEDecoderCreate(j_samplerate,fps,8,1);
	FR_free(fps);
#else
	ltc_decoder = ltc_decoder_create(j_samplerate * 25, 8);
#endif
	if (!ltc_decoder)
		return -1;

	if ((j_input_port = jack_port_register (j_client, "ltc-input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) {
		fprintf (stderr, "cannot register input port \"ltc-input\"!\n");
		return -1;
	}
#ifdef NEW_JACK_LATENCY_API
	jack_set_graph_order_callback (j_client, jack_latency_cb, NULL);
	jack_latency_cb(NULL);
#endif
	return 0;
}

/* API */

long ltc_poll_frame (void) {
	return (long) floor(ltc_position * framerate / (double)j_samplerate);
}

void open_ltcjack(char *autoconnect) {
	char * client_name = "xjadeo-ltc";
	if (init_jack(client_name)) {
		close_ltcjack();
		return;
	}
	if (jack_portsetup()) {
		close_ltcjack();
		return;
	}
	// TODO: autoconnect jack port ?!
	if (jack_activate (j_client)) {
		close_ltcjack();
		return;
	}
}

int ltcjack_connected(void) {
	if (j_client) return 1;
	return 0;
}

void close_ltcjack(void) {
	if (j_client) {
		jack_deactivate(j_client);
		jack_client_close (j_client);
	}
	if (ltc_decoder) {
#ifdef HAVE_LTCSMPTE
		SMPTEFreeDecoder(ltc_decoder);
#else
		ltc_decoder_free(ltc_decoder);
#endif
	}
	j_client=NULL;
	ltc_decoder=NULL;
	return;
}

const char *ltc_jack_client_name() {
	return jack_get_client_name(j_client);
}

#else

long ltc_poll_frame (void) { return 0;}
void open_ltcjack(char *autoconnect) { ; }
void close_ltcjack(void) { ; }
int ltcjack_connected(void) { return 0;}
const char *ltc_jack_client_name() { return "N/A";}

#endif
