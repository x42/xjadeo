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
 * (c) 2006 
 *  Robin Gareus <robin@gareus.org>
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <jack/jack.h>
#include <jack/transport.h>

#include "xjadeo.h"


//extern double duration;
//extern long frames;
extern double framerate;
extern int want_quiet;
extern int jack_clkconvert;
extern int interaction_override;
#ifdef HAVE_LASH
extern lash_client_t *lash_client;
#endif

jack_client_t *jack_client = NULL;
char jackid[16];

#ifdef JACK_SESSION
#include <jack/session.h>
extern char *jack_uuid;
extern int	loop_flag;
void jack_session_cb(jack_session_event_t *event, void *arg) {
	char filename[256];
	char command[256];
	if (interaction_override&OVR_JSESSION) {
		/* DO NOT SAVE SESSION 
		 * e.g. if xjadeo will be restored by wrapper-program
		 * f.i. ardour3+videotimeline
		 */
	  jack_session_reply(jack_client, event);
	  jack_session_event_free(event);
		return;
	}

	snprintf(filename, sizeof(filename), "%sxjadeo.state", event->session_dir );
	snprintf(command,  sizeof(command),  "xjadeo -U %s --rc ${SESSION_DIR}xjadeo.state", event->client_uuid );

	//TODO save-state in filename
	saveconfig(filename);

	event->command_line = strdup(command);
	jack_session_reply( jack_client, event );
  if(event->type == JackSessionSaveAndQuit)
		loop_flag=0;
	jack_session_event_free(event);
}
#endif

/* when jack shuts down... */
void jack_shutdown(void *arg)
{
	jack_client=NULL;
	fprintf (stderr, "jack server shutdown\n");
}

int jack_connected(void)
{
	if (jack_client) return (1);
	return (0);
}

void open_jack(void ) 
{
	if (jack_client) {
		fprintf (stderr, "xjadeo is alredy connected to jack.\n");
		return;
	}

	int i = 0;
	do {
		snprintf(jackid,16,"xjadeo-%i",i);
#ifdef JACK_SESSION
		if (jack_uuid) 
			jack_client = jack_client_open (jackid, JackUseExactName|JackSessionID, NULL, jack_uuid);
		else
#endif
		  jack_client = jack_client_open (jackid, JackUseExactName, NULL);
	} while (jack_client == 0 && i++<16);

	if (!jack_client) {
		fprintf(stderr, "could not connect to jack server.\n");
	} else { 
#ifdef JACK_SESSION
		jack_set_session_callback (jack_client, jack_session_cb, NULL);
#endif
#ifndef HAVE_WINDOWS
		jack_on_shutdown (jack_client, jack_shutdown, 0);
		jack_activate(jack_client);
#endif
		if (!want_quiet) 
			fprintf(stdout, "connected as jack client '%s'\n",jackid);
#ifdef HAVE_LASH
		lash_jack_client_name(lash_client, jackid);
#endif
	}
}

void jackt_rewind() {
	if (jack_client) {
		jack_transport_locate (jack_client,0);
	}
}

void jackt_start() {
	if (jack_client) {
		jack_transport_start (jack_client);
	}
}

void jackt_stop() {
	if (jack_client) {
		jack_transport_stop (jack_client);
	}
}

void jackt_toggle() {
	if (jack_client) {
		switch (jack_transport_query(jack_client, NULL)) { 
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

void close_jack(void)
{
	if (jack_client) {
		jack_client_t *b = jack_client;
		jack_client=NULL;
		jack_deactivate (b);
		jack_client_close (b);
	}
	jack_client=NULL;
}

long jack_poll_frame (void) {
	jack_position_t	jack_position;
	long 		frame = 0;

	if (!jack_client) return (-1);
	jack_transport_query(jack_client, &jack_position);

#ifdef JACK_DEBUG
	fprintf(stdout, "jack position: %lu %lu/ \n", (long unsigned) jack_position.frame, (long unsigned) jack_position.frame_rate);
	fprintf(stdout, "jack frame position time: %g sec %g sec\n", jack_position.frame_time , jack_position.next_time);
#endif

#ifdef HAVE_JACK_VIDEO
	if ((jack_position.valid & JackAudioVideoRatio) && jack_clkconvert == 0 ) {
		frame = (long ) floor(jack_position.audio_frames_per_video_frame  * jack_position.frame / (double) jack_position.frame_rate);
# ifdef JACK_DEBUG
		fprintf(stdout, "jack calculated frame: %li\n", frame);
# endif
	} else
#endif /* HAVE_JACK_VIDEO */
	{
		double jack_time = 0;
		jack_time = jack_position.frame / (double) jack_position.frame_rate;
	//	frame = (long) floor((double) frames * (double) jack_time / (double) duration);
	//	frame = sec_to_frame(jack_time);
		frame = floor(framerate * jack_time);
#ifdef JACK_DEBUG
		fprintf(stdout, "jack calculated time: %lf sec - frame: %li\n", jack_time, frame);
#endif
	}

	return(frame);
}
