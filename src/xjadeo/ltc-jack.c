/* xjadeo - jack video monitor
 *
 * (c) 2010  Robin Gareus <robin@gareus.org>
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

#ifdef HAVE_LTCSMPTE
extern double framerate;

#include <math.h>
#include <jack/jack.h>
#include <ltcsmpte.h>

void close_ltcjack(void);

SMPTEDecoder *ltc_decoder = NULL;
long int monotonic_fcnt = 0;
#define LTCFPS (25)

jack_nframes_t j_samplerate = 48000;
jack_port_t *j_input_port = NULL;
jack_default_audio_sample_t *j_in;
jack_nframes_t j_latency = 0;
jack_client_t *j_client = NULL;

long int ltc_position = 0;

/**
 * 
 */
int myProcess(SMPTEDecoder *d, long int *jt)  {
  SMPTEFrameExt frame;
  int errors;
  int i=0;
  int rv=0;
  int flush = 0; // XXX process only last LTC (0: all in decoder queue)
  while (flush && SMPTEDecoderRead(d,&frame)) {i++;}
  while (i || SMPTEDecoderRead(d,&frame)) {
    SMPTETime stime;
    i=0;

    
    // TODO: use frame.posinfo for latency compensation..
    SMPTEFrameToTime(&frame.base,&stime);
    SMPTEDecoderErrors(d,&errors);

    if (jt) {
      *jt=((stime.hours*60+stime.mins)*60 +stime.secs)*j_samplerate 
	  + (int)floor(stime.frame*(double)j_samplerate/LTCFPS);
    //printf("debug %i %lu \n",*jt,frame.posinfo);
		//TODO: use LTCsmpte's framerate or xjadeo convertor!
    }
    /* TODO use frame.startpos or frame.endpos
		 * if (jf) {
      *jf= frame.posinfo;
    }*/

#ifdef DEBUG
    int ms;
    SMPTEDecoderFrameToMillisecs(d,&frame,&ms);
    printf("Ch%i: %02d:%02d:%02d:%02d %8d %d \n",chn+1,
      stime.hours,stime.mins,
      stime.secs,stime.frame,
      ms,
      errors);		
#endif
    ++rv;
  }
  return rv;
}


/**
 * jack audio process callback
 */
int process (jack_nframes_t nframes, void *arg) {
	j_in = jack_port_get_buffer (j_input_port, nframes);
	j_latency = jack_port_get_total_latency(j_client,j_input_port);

  //assert(nframes <= 4096); // XXX
	
  unsigned char sound[4096];
  jack_default_audio_sample_t val;
  size_t i;
	for (i = 0; i < nframes; i++) {
		int snd;
		val=*(j_in+i);
		snd=(int)rint((127.0*val)+128.0);
		sound[i] = (unsigned char) (snd&0xff);
	}
	SMPTEDecoderWrite(ltc_decoder,sound,nframes,monotonic_fcnt-j_latency);
#if 1 
	myProcess(ltc_decoder,&ltc_position);
#endif

  monotonic_fcnt += nframes; // XXX we don't need that here - TODO simply use latency as posinfo and subtract it later..
  return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void ltcjack_shutdown (void *arg) {
  fprintf(stderr,"recv. shutdown request from jackd.\n");
  close_ltcjack();
}

/**
 * open a client connection to the JACK server 
 */
int init_jack(const char *client_name, const char *server_name, jack_options_t options) {
  jack_status_t status;
  j_client = jack_client_open (client_name, options, &status, server_name);
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
#ifndef HAVE_WINDOWS
  jack_on_shutdown (j_client, ltcjack_shutdown, 0);
#endif
  j_samplerate=jack_get_sample_rate (j_client);
	return 0;
}

/**
 *
 */
int jack_portsetup(void) {
	int fps_num = 25; // XXX -> extern from main.c
	int fps_den = 1;
	FrameRate *fps;
	ltc_decoder = SMPTEDecoderCreate(j_samplerate,(fps=FR_create(fps_num,fps_den,FRF_NONE)),8,1);
	FR_free(fps);
  
	char name[64];
	sprintf (name, "ltc-input");
	if ((j_input_port = jack_port_register (j_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) {
		fprintf (stderr, "cannot register input port \"%s\"!\n", name);
		jack_client_close (j_client);
		j_client=NULL;
		return -1;
  }
	return 0;
}


/* API */

long ltc_poll_frame (void) {
	//myProcess(ltc_decoder, NULL, NULL);
	return ((ltc_position)*framerate/j_samplerate);
	return 0;
}

void open_ltcjack(char *autoconnect) { 
	// TODO: jack-client name ID?!
	char * client_name = "xjadeo-ltc";
	const char * server_name = NULL;
  jack_options_t options = JackNullOption;
  if (init_jack(client_name, server_name, options)) {
		close_ltcjack();
		return;
	}
  if (jack_portsetup()) {
		close_ltcjack();
		return;
	}
// TODO:  autoconnect
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
		SMPTEFreeDecoder(ltc_decoder);
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
