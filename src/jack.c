#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <jack/jack.h>
#include <jack/transport.h>


extern double            duration;
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
	//fprintf(stdout, "jack calculated time: %lf\n", jack_time);
	frame = floor(frames * jack_time / duration);
	return(frame);
}
