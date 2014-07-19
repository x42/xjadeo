/* xjadeo - Linear Time Code Sync - libltc interface
 *
 * (C) 2010-2014 Robin Gareus <robin@gareus.org>
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

#ifdef HAVE_LTC

#include <stdio.h>
#include <ltc.h>
#include <math.h>

#include "weak_libjack.h"

#ifdef JACK_SESSION
extern char *jack_uuid;
void jack_session_cb( jack_session_event_t *event, void *arg );
#endif

extern double framerate;
extern int jack_autostart;

void close_ltcjack(void);

static jack_nframes_t j_samplerate = 48000;
static jack_port_t *j_input_port = NULL;
static jack_default_audio_sample_t *j_in;
static jack_nframes_t j_latency = 0;
static jack_client_t *j_client = NULL;

static double ltc_position = 0;
static long long int monotonic_fcnt = 0;
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

#ifdef NEW_JACK_LATENCY_API
static int jack_latency_cb(void *arg) {
	jack_latency_range_t jlty;
	WJACK_port_get_latency_range(j_input_port, JackCaptureLatency, &jlty);
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
	j_in = WJACK_port_get_buffer (j_input_port, nframes);

#ifndef NEW_JACK_LATENCY_API
	j_latency = WJACK_port_get_total_latency(j_client,j_input_port);
#endif
	if (nframes > 8192) return 0;

	for (i = 0; i < nframes; i++) {
	  const int snd=(int)rint((127.0*j_in[i])+128.0);
	  sound[i] = (unsigned char) (snd&0xff);
	}

	ltc_decoder_write(ltc_decoder, sound, nframes, monotonic_fcnt-j_latency);
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
	jack_options_t options = jack_autostart ? JackNullOption : JackNoStartServer;
#ifdef JACK_SESSION
	if (jack_uuid)
		j_client = WJACK_client_open2 (client_name, options|JackSessionID, &status, jack_uuid);
	else
#endif
		j_client = WJACK_client_open1 (client_name, options, &status);
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
		client_name = WJACK_get_client_name(j_client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	WJACK_set_process_callback (j_client, process, 0);
#ifdef JACK_SESSION
	WJACK_set_session_callback (j_client, jack_session_cb, NULL);
#endif
#ifndef PLATFORM_WINDOWS
	WJACK_on_shutdown (j_client, ltcjack_shutdown, 0);
#endif
	j_samplerate=WJACK_get_sample_rate (j_client);
	return 0;
}

static int jack_portsetup(void) {
	ltc_decoder = ltc_decoder_create(j_samplerate * 25, 8);
	if (!ltc_decoder)
		return -1;

	if ((j_input_port = WJACK_port_register (j_client, "ltc-input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) {
		fprintf (stderr, "cannot register input port \"ltc-input\"!\n");
		return -1;
	}
#ifdef NEW_JACK_LATENCY_API
	WJACK_set_graph_order_callback (j_client, jack_latency_cb, NULL);
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
	if (WJACK_activate (j_client)) {
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
		WJACK_deactivate(j_client);
		WJACK_client_close (j_client);
	}
	if (ltc_decoder) {
		ltc_decoder_free(ltc_decoder);
	}
	j_client=NULL;
	ltc_decoder=NULL;
	return;
}

const char *ltc_jack_client_name() {
	return WJACK_get_client_name(j_client);
}

#else

long ltc_poll_frame (void) { return 0;}
void open_ltcjack(char *autoconnect) { ; }
void close_ltcjack(void) { ; }
int ltcjack_connected(void) { return 0;}
const char *ltc_jack_client_name() { return "N/A";}

#endif
