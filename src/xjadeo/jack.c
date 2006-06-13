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


extern double            duration;
extern double            framerate;
extern long              frames;
extern int want_quiet;

jack_client_t *jack_client = NULL;
char jackid[16];

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
		jack_client = jack_client_new (jackid);
	} while (jack_client == 0 && i++<16);

	if (!jack_client) {
		fprintf(stderr, "could not connect to jack server.\n");
	} else { 
		jack_on_shutdown (jack_client, jack_shutdown, 0);
		if (!want_quiet) 
			fprintf(stdout, "connected as jack client '%s'\n",jackid);
	}
}

void close_jack(void)
{
	if (jack_client)
		jack_client_close (jack_client);
	jack_client=NULL;
}

long jack_poll_frame (void) {
	jack_position_t	jack_position;
	double		jack_time;
	long 		frame;

	if (!jack_client) return (-1);

	/* Calculate frame. */
	jack_transport_query(jack_client, &jack_position);
	jack_time = jack_position.frame / (double) jack_position.frame_rate;
//	fprintf(stdout, "jack calculated time: %lf sec\n", jack_time);
	frame = (long) rint((double) frames * (double) jack_time / (double) duration);
//	if (frame!= rint(framerate * jack_time) ) printf("\nDAMN %i %f\n",frame, rint(framerate * jack_time));
//	frame = rint(framerate * jack_time);
	return(frame);
}
