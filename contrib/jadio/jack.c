/* 
 *  Copyright (C) 2006 Robin Gareus  <robin AT gareus.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <jack/jack.h>
#include <jack/transport.h>
#include <math.h>
#include <string.h>

int j_scrubpos = 0;
int j_scrubmax = 1;

jack_client_t *j_client = NULL;
jack_port_t **j_output_port; 
jack_default_audio_sample_t **j_output_bufferptrs; // 

jack_nframes_t j_bufsiz = 64;
jack_nframes_t j_srate = 48000;


// extern prototypes //

jack_nframes_t fillBuffer( jack_default_audio_sample_t ** bufferptrs, jack_nframes_t frames );
void sampleseek (jack_nframes_t sample);
void jadio_realloc(jack_nframes_t frames );

extern jack_nframes_t m_length; // length of file in frames.
extern jack_nframes_t m_resampled_length; // length of file in jack-frames.
extern int m_channels ;    // channels of the file
extern int m_samplerate;    // sample rate of the file
extern float m_fResampleRatio;
extern int p_scrublen;
extern int p_scrub_audio;
extern int p_autoconnect;


/* jadio - jack */

void jackreadAudio( jack_default_audio_sample_t **out, jack_transport_state_t ts, jack_nframes_t position, jack_nframes_t nframes )
{ 	

	if (p_scrub_audio && ts == JackTransportStopped) {
		position+=(j_scrubpos) * j_bufsiz;
		j_scrubpos=(j_scrubpos+1)%(j_scrubmax);
	} else j_scrubpos=0;

	// FIXME: add offset+lat_compensation to position... then remove '*0'
	if ((position* 0 >= m_resampled_length ) ||  (!p_scrub_audio && ts != JackTransportRolling)) {
		// if transport is not rolling - or file ended : remain silent.
		int i;

		for (i=0;i<m_channels;i++) 
			memset(out[i],0, sizeof (jack_default_audio_sample_t) * nframes);

		if (ts == JackTransportStarting) {
			sampleseek(position); 
		}
		return;
	}
	sampleseek(position); // set absolute audio sample position

	jack_nframes_t rv = fillBuffer(out, nframes);

	if (rv != nframes) {
		fprintf(stderr, "Soundoutput buffer underrun: %i/%i\n",rv,nframes);
///		memset(outR,0, sizeof (jack_default_audio_sample_t) * nframes);
///		memset(outL,0, sizeof (jack_default_audio_sample_t) * nframes);
	}
}



/*
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 */
jack_nframes_t j_latency = 0;

int jack_audio_callback (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t **out = j_output_bufferptrs; // 
	jack_position_t	jack_position;
	jack_transport_state_t ts;
	int i;
	jack_nframes_t my_tot_latency = 0;

	for (i=0;i<m_channels;i++) {

		jack_nframes_t my_latency = jack_port_get_total_latency(j_client,j_output_port[i]);
		if (my_latency > my_tot_latency) my_tot_latency = my_latency;
//		printf("DEBUG: c=%i pl=%i tl=%i\n",i,jack_port_get_latency(j_output_port[i]) ,jack_port_get_total_latency(j_client,j_output_port[i]));


		out[i] = jack_port_get_buffer (j_output_port[i], nframes);
//		memset(out[i],0, sizeof (jack_default_audio_sample_t) * nframes);
	}
	if (my_tot_latency != j_latency) {
		j_latency = my_tot_latency;
		printf("latency compensation: %i frames \n",j_latency);
	}

	ts=jack_transport_query(j_client, &jack_position);

	jackreadAudio(out, ts, jack_position.frame, nframes);

	return(0);
}

/* when jack shuts down... */
void jack_shutdown_callback(void *arg)
{
	jack_client_t *remember = j_client;
	j_client=NULL;
	fprintf(stderr,"jack server shutdown.\n");
	jack_client_close (remember);
	fprintf(stderr,"closed zombified jack server connection .\n");
}

/* when jack changes its samplerate ... */
int jack_srate_callback(jack_nframes_t nframes, void *arg)
{
	fprintf(stderr,"DEBUG: jack sample rate: %i\n",nframes); 
	j_srate = nframes;
	m_fResampleRatio = (float) j_srate / (float) m_samplerate;
	m_resampled_length= (jack_nframes_t) ceil((float)m_length * m_fResampleRatio);
	// FIXME: actually we should realloc the resampling buffer when this happens 
	// but this requires a non RT callback. -> buffersize change
	return(0); 
}

/* when jack changes its buffer size ... 
 * -  here we can do non realtime stuff // eg. realloc 
 */
int jack_bufsiz_callback(jack_nframes_t nframes, void *arg)
{
	fprintf(stderr,"DEBUG: jack buffer size: %i\n",nframes);
	j_bufsiz = nframes;
	jadio_realloc( j_bufsiz );
	j_scrubpos = 0;
	j_scrubmax = (j_bufsiz!=0)?(int)ceil((double) p_scrublen/j_bufsiz):1;
	fprintf(stderr,"INFO: scrublength: %i buffers (%.1f ms - %.1f Hz)\n",j_scrubmax,1000.0*(double)j_scrubmax*(double)j_bufsiz/(double)j_srate,(double)j_srate/(double)j_scrubmax/(double)j_bufsiz);
	return(0); 
}



void close_jack(void)
{
	if (j_client)
		jack_client_close (j_client);
	j_client=NULL;
}

int jack_connected() {
	if (j_client) return(1);
	return(0);
}

void connect_jackports()  {
	int i,myc;
	if (!j_client) return;
	if (!p_autoconnect) return;

	char * port_name = NULL; // TODO: get from preferences 
	int port_flags = JackPortIsInput;
	if (!port_name) port_flags |= JackPortIsPhysical;

	const char **found_ports = jack_get_ports(j_client, port_name, NULL, port_flags);
	myc=0;
	for (i = 0; found_ports && found_ports[i]; i++) {
		if (jack_connect(j_client, jack_port_name(j_output_port[myc]), found_ports[i])) {
			fprintf(stderr,"can not connect to jack output\n");
		}
		// TODO: connect mono samples to first two outputs only if (p_two_mono)
		if (m_channels!=1 || i!=0) myc++;
		if (myc >= m_channels) break;
	}
}

void open_jack(void) 
{
	if (j_client) {
		fprintf(stderr,"already connected to jack.\n");
		return;
	}

	int i = 0;
	do {
		char jackid[16];
		snprintf(jackid,16,"jadio-%i",i);
		j_client = jack_client_new (jackid);
	} while (j_client == 0 && i++<16);

	if (!j_client) {
		fprintf(stderr, "could not connect to jack.\n");
		return;
	}	

	jack_on_shutdown (j_client, jack_shutdown_callback, NULL);
	jack_set_process_callback(j_client,jack_audio_callback,NULL);
	jack_set_sample_rate_callback (j_client, jack_srate_callback, NULL);
	jack_set_buffer_size_callback (j_client, jack_bufsiz_callback, NULL);

	j_output_port= calloc(m_channels,sizeof(jack_port_t*));
	j_output_bufferptrs = calloc(m_channels,sizeof(jack_default_audio_sample_t*));

	for (i=0;i<m_channels;i++) {
		char channelid[16];
		snprintf(channelid,16,"output-%i",i);
		j_output_port[i] = jack_port_register (j_client, channelid, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		if (!j_output_port[i]) {
			fprintf(stderr, "no more jack ports availabe.\n");
			close_jack();
			return;
		}
	}
	jack_srate_callback(jack_get_sample_rate(j_client),NULL); // init j_srate
	jack_bufsiz_callback(jack_get_buffer_size(j_client),NULL); // init j_bufsiz & alloc

	jack_activate(j_client);

	connect_jackports();
}

void dothejack(void)
{
	open_jack();  // try to connect to jack

	if (jack_connected()) {
		j_scrubpos = 0;
		j_scrubmax = (j_bufsiz!=0)?(int)ceil(p_scrublen/j_bufsiz):1;
	} 
}

void loopthejack(void) {
	if (!jack_connected()) return;

	while (1) { 
		if (!jack_connected()) dothejack();
		sleep (1);
	}
}
