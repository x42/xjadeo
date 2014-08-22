/* xjadeo - JACK transport sync interface
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "weak_libjack.h"

#include "xjadeo.h"

extern double framerate;
extern int want_quiet;
extern int jack_clkconvert;
extern int interaction_override;
extern int jack_autostart;

static jack_client_t *jack_client = NULL;

/* when jack shuts down... */
static void jack_shutdown(void *arg) {
	jack_client=NULL;
	xj_shutdown_jack();
	if (!want_quiet)
		fprintf (stderr, "jack server shutdown\n");
}

int jack_connected(void) {
	if (jack_client) return (1);
	return (0);
}

void open_jack(void ) {
	if (jack_client) {
		fprintf (stderr, "xjadeo is alredy connected to jack.\n");
		return;
	}
	if (xj_init_jack(&jack_client, "xjadeo")) {
		return;
	}
	WJACK_on_shutdown (jack_client, jack_shutdown, 0);
}

void jackt_rewind() {
	if (jack_client) {
		WJACK_transport_locate (jack_client,0);
	}
}

void jackt_start() {
	if (jack_client) {
		WJACK_transport_start (jack_client);
	}
}

void jackt_stop() {
	if (jack_client) {
		WJACK_transport_stop (jack_client);
	}
}

void jackt_toggle() {
	if (jack_client) {
		switch (WJACK_transport_query(jack_client, NULL)) {
			case JackTransportRolling:
				jackt_stop();
				break;
			case JackTransportStopped:
				jackt_start();
				break;
			default:
				break;
		}
	}
}

void close_jack(void) {
	if (jack_client) {
		jack_client_t *b = jack_client;
		// prevent any timecode query while we're closing
		jack_client=NULL;
		xj_close_jack(&b);
	}
	jack_client=NULL;
}

int64_t jack_poll_frame (uint8_t *rolling) {
	jack_position_t	jack_position;
	int64_t frame = 0;

	if (!jack_client) return (-1);
	memset(&jack_position, 0, sizeof(jack_position));
	jack_transport_state_t jts = WJACK_transport_query(jack_client, &jack_position);

#ifdef JACK_DEBUG
	fprintf(stdout, "jack position: %u %u/ \n", (unsigned int) jack_position.frame, (unsigned int) jack_position.frame_rate);
	fprintf(stdout, "jack frame position time: %g sec %g sec\n", jack_position.frame_time , jack_position.next_time);
#endif

#ifdef HAVE_JACK_VIDEO
	if ((jack_position.valid & JackAudioVideoRatio) && jack_clkconvert == 0 ) {
		frame = (int64_t)floor (jack_position.audio_frames_per_video_frame * jack_position.frame / (double) jack_position.frame_rate);
# ifdef JACK_DEBUG
		fprintf(stdout, "jack calculated frame: %li\n", frame);
# endif
	} else
#endif /* HAVE_JACK_VIDEO */
	{
		double jack_time = 0;
		jack_time = jack_position.frame / (double) jack_position.frame_rate;
		frame = (int64_t)floor (framerate * jack_time);
#ifdef JACK_DEBUG
		fprintf(stdout, "jack calculated time: %lf sec - frame: %li\n", jack_time, frame);
#endif
	}
	if (rolling) {
		*rolling = (jts != JackTransportStopped) ? 1 : 0;
	}
	return(frame);
}
