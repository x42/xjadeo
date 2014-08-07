/* xjadeo - MIDI / MTC sync
 *
 * (C) 2006-2014 Robin Gareus <robin@gareus.org>
 * (C) 2006 Luis Garrido <luisgarrido@users.sourceforge.net>
 * (C) 2005 Clemens Ladisch <clemens@ladisch.de>
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

#include <xjadeo.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "weak_libjack.h"

#ifdef HAVE_MIDI

extern int want_quiet;
extern int want_verbose;
extern int want_debug;
extern double framerate;
extern int midi_clkadj;
extern double	delay;

typedef struct {
	int frame;
	int sec;
	int min;
	int hour;

	int day; //  overflow
	int type;
	int tick; // 1/8 of a frame.
} smpte;

/* global Vars */
static smpte tc;
static smpte last_tc;
static int full_tc = 0;

const char MTCTYPE[4][10] = {
	"24fps",
	"25fps",
	"29fps",
	"30fps",
};

/* midi system exclusive start/end byte*/
#define MIDI_SOX 0xf0
#define MIDI_EOX 0xf7

#define SE(ARG) prevtick = tc.tick; tc.tick=ARG; full_tc|=1<<(ARG);
#define SL(ARG) ARG = ( ARG &(~0xf)) | (data&0xf);
#define SH(ARG) ARG = ( ARG &(~0xf0)) | ((data&0xf)<<4);

/* parse MTC 0x71 message data */
static void parse_timecode( int data) {
	static int prevtick =0;
	switch (data>>4) {
		case 0x0: // #0000 frame LSN
			SE(1); SL(tc.frame); break;
		case 0x1: // #0001 frame MSN
			SE(2); SH(tc.frame); break;
		case 0x2: // #0010 sec LSN
			SE(3); SL(tc.sec); break;
		case 0x3: // #0011 sec MSN
			SE(4); SH(tc.sec); break;
		case 0x4: // #0100 min LSN
			SE(5); SL(tc.min); break;
		case 0x5: // #0101 min MSN
			SE(6); SH(tc.min); break;
		case 0x6: // #0110 hour LSN
			SE(7); SL(tc.hour); break;
		case 0x7: // #0111 hour MSN and type
			SE(0);tc.hour= (tc.hour&(~0xf0)) | ((data&1)<<4);
			tc.type = (data>>1)&3;
			if (full_tc!=0xff) break;
			if (want_verbose) {
				printf("\r\t\t\t\t\t\t\t->- %02i:%02i:%02i.%02i[%s]\r",tc.hour,tc.min,tc.sec,tc.frame,MTCTYPE[tc.type]);
				fflush(stdout);
			}
			memcpy(&last_tc,&tc,sizeof(smpte));
			tc.type=tc.min=tc.frame=tc.sec=tc.hour=tc.tick=0;
		default:
			;
	}
	if (full_tc!=0xff) { last_tc.tick=0; return; }
	// count quarterframes
	switch (tc.tick - prevtick) {
		case 7:
		//	assert(tc.tick==7);
		case -1: /*reverse direction */
			last_tc.tick=0-tc.tick; // -7+(7-tc.tick) compensate for latency
			if (want_verbose) { printf("\r\t\t\t\t\t\t\t-<-\r"); fflush(stdout); }
			break;
		case -7:
		//	assert(prevtick==7);
		case 1: /* transport rolling */
			last_tc.tick=tc.tick+7; // compensate for latency
			break;
		default:
			full_tc=last_tc.tick=0;
	}
}

/* parse system exclusive MSGs
 * seek frame - if transport is not rolling */
static int parse_sysex_urtm (int data, int state, int type) {
	/*
	 *	Structure of a System Exclusive Message
	 *
	 *	# Start of Exclusive: F0
	 *	# Manufacturer ID: 00-7D, 7E=Universal non-realtime, 7F=Universal realtime
	 *	# Device number: 00-0F = dev 1 -16, 7F=all
	 *	# Model ID: 00-7F
	 *	# 0 or more data bytes
	 *	# Checksum: 00-7F (all data + checksum = 0 for lowest 7 bits)
	 *	# End of Exclusive: F7
	 *
	 * eg:
	 * F0 7F 7F 01 01 20 00 03 01 F7
	 * F0 7F 7F 06 44 06 01 20 00 03 01 00 F7
	 * timecode 00:00:03:01 (@ 25fps) -- Roland MTC
	 *
	 *  roland MTC sysex real time message - reverse engineered format
	 *         01 01 [fps: bit 6..5 hour: bit4..0] [min] [sec] [frame] F7
	 *         06 xx 06 xx [fps (bit 6..5) hour: bit4..0)] [min] [sec] [frame] Checksum F7
	 *
	 *  fps/hour:	(011HHHHH):  30 fps, 29fps - non drop
	 *		(010HHHHH :  29 fps (drop)
	 *		(001HHHHH):  25 fps
	 *		(000HHHHH):  24 fps
	 *  min,sec,frame are 6bit values
	 */

	int rv=type;
	if (type<0) return (-1);
	if (state<2 && data !=0x7f) return(-1);
	if (state<3 && type>0) return (-1);

	if (state==2 && type==0) {
		if (data==0x01) rv=1;
		if (data==0x06) rv=2;
		last_tc.tick=0;
	}
	if (state>2 && type <=0 ) return (-4);

	if (state==3 && type ==1 && data!=0x01) return (-1);
	if (state==3 && type ==2 && data!=0x44) return (-1);

	if (state==3 && type ==2 && data!=0x44) return (-1);
	if (state==4 && type ==2 && data!=0x06) return (-1);
	if (state==5 && type ==2 && data!=0x01) return (-1);

	// type==1 && state4,5,6,7
	if (state==4 && type ==1 ) { last_tc.hour=(data&0x1f); last_tc.type=(data>>5)&3; } // hour + format
	if (state==5 && type ==1 ) { last_tc.min=(data&0x7f); } // min
	if (state==6 && type ==1 ) { last_tc.sec=(data&0x7f); } // sec
	if (state==7 && type ==1 ) { last_tc.frame=(data&0x7f); } // frame
	if (state>7 && type ==1 ) {
		if (want_verbose) {
			if (want_debug)
				printf("\r\t\t\t\t\t\t\t~-~ %02i:%02i:%02i.%02i[%s]\r",tc.hour,tc.min,tc.sec,tc.frame,MTCTYPE[tc.type]);
			else
				printf("\r\t\t\t\t\t\t\t-~- %02i:%02i:%02i.%02i[%s]\r",last_tc.hour,last_tc.min,last_tc.sec,last_tc.frame,MTCTYPE[last_tc.type]);
			fflush(stdout);
		}
		return (-1);
	}

	// type==2 && state6,7,8,9
	if (state==6 && type ==2 ) { last_tc.hour=(data&0x1f); /*last_tc.type=(data>>5)&3*/; } // hour
	if (state==7 && type ==2 ) { last_tc.min=(data&0x7f); } // min
	if (state==8 && type ==2 ) { last_tc.sec=(data&0x7f); } // sec
	if (state==9 && type ==2 ) { last_tc.frame=(data&0x7f); } // frame
	if (state>9 && type ==2 ) {
		if (want_verbose) {
			if (want_debug)
				printf("\r\t\t\t\t\t\t\t-V- %02i:%02i:%02i.%02i[%s]\r",tc.hour,tc.min,tc.sec,tc.frame,MTCTYPE[tc.type]);
			else
				printf("\r\t\t\t\t\t\t\t-v- %02i:%02i:%02i.%02i[%s]\r",last_tc.hour,last_tc.min,last_tc.sec,last_tc.frame,MTCTYPE[last_tc.type]);
			fflush(stdout);
		}
		return (-1);
	}

	return (rv);
}

static int64_t convert_smpte_to_frame (smpte now) {
	return(smpte_to_frame(
		now.type,
		now.frame,
		now.sec,
		now.min,
		now.hour,
		now.day));
}


/************************************************
 * portmidi
 */

#ifdef HAVE_PORTMIDI

#include <portmidi.h>
#include <porttime.h>

typedef void PmQueue;
PmQueue *Pm_QueueCreate(long num_msgs, long bytes_per_msg);
PmError Pm_QueueDestroy(PmQueue *queue);
PmError Pm_Enqueue(PmQueue *queue, void *msg);
PmError Pm_Dequeue(PmQueue *queue, void *msg);

PmStream * pm_midi = NULL;

/* if INPUT_BUFFER_SIZE is 0, PortMidi uses a default value */
#define INPUT_BUFFER_SIZE 0

static int pm_midi_detectdevices (int print) {
	int midiid=-1;
	int i;

	// id = Pm_GetDefaultInputDeviceID(); <- use this as default ??

	/* list device information */
	for (i = 0; i < Pm_CountDevices(); i++) {
		const PmDeviceInfo *info = Pm_GetDeviceInfo(i);

		if (info->input) {
			if(midiid==-1) { midiid=i; }
		}

		if (print) {
			printf("%d: %s, %s", i, info->interf, info->name);
			if (info->input) printf(" (input)");
			if (info->output) printf(" (output)");
			if(midiid==i)  printf(" (*)");
			printf("\n");
		}
	}
	return (midiid);
}

static int pm_midi_check (int midiid) {
	if (midiid < 0 || midiid >=Pm_CountDevices()) {
		fprintf(stderr,"Error: invalid midi device id.\n");
		return(-1);
	}
	return(0);
}

static int active = FALSE;
static int sysex_state = -1;
static int sysex_type = 0;

/* shared queues */
PmQueue *midi_to_main;
PmQueue *main_to_midi;

/* timer interrupt for processing midi data */
static void process_midi(PtTimestamp timestamp, void *userData) {
	PmError result;
	PmEvent buffer; /* just one message at a time */
	smpte msg;

	if (!active) return;

	/* check for messages */
	do {
		result = Pm_Dequeue(main_to_midi, &msg);
		if (result) {
			if (msg.frame == 0xaffe) {
				// stop thread
				Pm_Enqueue(midi_to_main, &msg);
				active= FALSE;
				return;
			} else if (msg.frame == 0x4711) {
				// transport stopped - reset ticks.
				full_tc=last_tc.tick=0;
				Pm_Enqueue(midi_to_main, &msg);
			} else {
				memcpy(&msg,&last_tc,sizeof(smpte));
				Pm_Enqueue(midi_to_main, &msg);
			}
		}
	} while (result);

	/* see if there is any midi input to process */
	do {
		result = Pm_Poll(pm_midi);
		if (result) {
			int shift, data;
			data = 0;

			if (Pm_Read(pm_midi, &buffer, 1) == pmBufferOverflow) continue;

			/* parse only MTC relevant messages */
			if (Pm_MessageStatus(buffer.message) == 0xf1)
				parse_timecode (Pm_MessageData1(buffer.message));

			for (shift = 0; shift < 32 && (data != MIDI_EOX); shift += 8) {
				data = (buffer.message >> shift) & 0xFF;

				/* if this is a status byte that's not MIDI_EOX, the sysex
				 * message is incomplete and there is no more sysex data */
				if (data & 0x80 && data != MIDI_EOX && data != MIDI_SOX) {
					sysex_state=-1;
					break;
				}
				/* sysex- universal  real time message f0 7f ... f7  */
				if (data == 0xf7) { sysex_state=-1;}
				else if (sysex_state < 0 && data == 0xf0) { sysex_state=0; sysex_type=0; }
				else if (sysex_state>=0) {
					sysex_type = parse_sysex_urtm (data,sysex_state,sysex_type);
					sysex_state++;
				}
			}
		}
	} while (result);
}

static void pm_midi_open(char *midiid) {
	int midi_input;
	if (pm_midi) return;

	midi_input = atoi(midiid);
	if (want_verbose && midi_input < 0) midi_input = pm_midi_detectdevices(1);
	else if (midi_input <0 ) midi_input = pm_midi_detectdevices(0);

	if (pm_midi_check(midi_input)) return ;

	// init smpte
	tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
	last_tc.type=last_tc.min=last_tc.frame=last_tc.sec=last_tc.hour=0;
	sysex_state = -1;

	midi_to_main = Pm_QueueCreate(2, sizeof(smpte));
	main_to_midi = Pm_QueueCreate(2, sizeof(smpte));
	if (!midi_to_main || !main_to_midi ) {
		fprintf(stderr, "Could not create portmidi queues\n");
		return;
	}

	PmEvent buffer[1];
	Pt_Start(1, &process_midi, 0); /* timer started w/millisecond accuracy */

	Pm_Initialize();

	/* open input device */
	Pm_OpenInput(&pm_midi, midi_input, NULL, INPUT_BUFFER_SIZE, NULL, NULL);

	if (!want_quiet) printf("Midi Input opened.\n");

	Pm_SetFilter(pm_midi, PM_FILT_ACTIVE | PM_FILT_CLOCK);
	/* flush the buffer after setting filter, just in case anything got through */
	while (Pm_Poll(pm_midi)) { Pm_Read(pm_midi, buffer, 1); }

	active = TRUE;
}

static void pm_midi_close(void) {
	smpte cmd;

	if (!want_quiet) printf("closing midi...");
	if(!pm_midi) return;

	cmd.frame=0xaffe; // shutdown CMD
	Pm_Enqueue(main_to_midi, &cmd);
	while (Pm_Dequeue(midi_to_main, &cmd)==0) ; // spin

	Pt_Stop(); /* stop the timer */
	Pm_QueueDestroy(midi_to_main);
	Pm_QueueDestroy(main_to_midi);

	Pm_Close(pm_midi);
	pm_midi=NULL;
}

static int pm_midi_connected(void) {
	if (pm_midi) return (1);
	return (0);
}

static int64_t pm_midi_poll_frame (void) {
	int spin;
	int64_t frame;
	static int64_t lastframe = -1 ;
	static int stopcnt = 0;
	smpte now;
	if (!pm_midi) return (0);

	now.frame=0; // CMD request
	Pm_Enqueue(main_to_midi, &now); // request data
	do {
		spin = Pm_Dequeue(midi_to_main, &now);
	} while (spin == 0); /* spin */ ;

	frame = convert_smpte_to_frame(now);

	if(midi_clkadj && (full_tc==0xff)) {
		double dly = delay>0?delay:(1.0/framerate);
		//add time that has passed sice last full MTC frame..
		smpte cmd;
		cmd.frame=0x4711; // reset-full_tc CMD
		double diff= now.tick/4.0; // in smpte frames.
		// check if transport is stuck...
		if (lastframe != frame) {
			stopcnt=0;
			lastframe=frame;
		} else if (stopcnt++ > (int) ceil(4.0*framerate/dly)) {
			// we expect a full midi MTC every (2.0*framerate/delay) polls

			Pm_Enqueue(main_to_midi, &cmd); // request data
			while (Pm_Dequeue(midi_to_main, &cmd)==0) ; // spin
			diff=0.0;
			if (want_verbose)
				printf("\r\t\t\t\t\t\t        -?-\r");
		}
		frame += (int64_t) rint(diff);
		if (want_verbose)
			// subtract 7 quarter frames latency when running..
			printf("\r\t\t\t\t\t\t  |+%g/8\r",diff<0?rint(4.0*(1.75-diff)):diff<2.0?0:rint(4.0*(diff-1.75)));
	}
	return(frame);
}
#endif /* HAVE_PORTMIDI */

#ifdef HAVE_JACKMIDI

/************************************************
 * jack-midi
 *
 */

static jack_client_t *jack_midi_client = NULL;
static jack_port_t   *jack_midi_port;

#define JACK_MIDI_QUEUE_SIZE (1024)
typedef struct my_midi_event {
	jack_nframes_t time;
	size_t size;
	jack_midi_data_t buffer[16];
} my_midi_event_t;

static my_midi_event_t event_queue[JACK_MIDI_QUEUE_SIZE];
static int queued_events_start = 0;
static int queued_events_end = 0;
static int queued_cycle_id = 0;

static void dequeue_jmidi_events(jack_nframes_t until) {
	int ci = queued_cycle_id;
	int new=0; // always process data from prev. jack cycles.
	while (queued_events_start != queued_events_end) {
		if (queued_events_start == ci ) new=1;
		if (new && event_queue[queued_events_start].time > until) {
			break;
		}

		my_midi_event_t *ev = &event_queue[queued_events_start];

		if (ev->size==2 && ev->buffer[0] == 0xf1) {
			parse_timecode(ev->buffer[1]);
		} else if (ev->size >9 && ev->buffer[0] == 0xf0) {
			int i;
			int sysex_type = 0;
			for (i=1; i<ev->size; ++i) {
				sysex_type = parse_sysex_urtm(ev->buffer[i],i-1,sysex_type);
			}
		}
		queued_events_start = (queued_events_start +1 ) % JACK_MIDI_QUEUE_SIZE;
	}
}

static int jack_midi_process(jack_nframes_t nframes, void *arg) {
	void *jack_buf = WJACK_port_get_buffer(jack_midi_port, nframes);
	int nevents = WJACK_midi_get_event_count(jack_buf);
	int n;
	queued_cycle_id = queued_events_end;

	for (n=0; n<nevents; n++) {
		jack_midi_event_t ev;
		WJACK_midi_event_get(&ev, jack_buf, n);

		if (ev.size <1 || ev.size > 15) {
			continue;
		} else {
			event_queue[queued_events_end].time = ev.time;
			event_queue[queued_events_end].size = ev.size;
			memcpy (event_queue[queued_events_end].buffer, ev.buffer, ev.size);
			queued_events_end = (queued_events_end +1 ) % JACK_MIDI_QUEUE_SIZE;
		}
	}
	return 0;
}

static void jack_midi_shutdown(void *arg) {
	jack_midi_client=NULL;
	xj_shutdown_jack();
	if (!want_quiet)
		fprintf (stderr, "jack server shutdown\n");
}

static void jm_midi_close(void) {
	xj_close_jack(&jack_midi_client);
}

static void jm_midi_open(char *midiid) {
	if (midi_connected()) {
		fprintf (stderr, "xjadeo is already connected to jack-midi.\n");
		return;
	}

	if (xj_init_jack(&jack_midi_client, "xjadeo")) {
		return;
	}

#ifndef PLATFORM_WINDOWS
	WJACK_on_shutdown (jack_midi_client, jack_midi_shutdown, 0);
#endif
	WJACK_set_process_callback(jack_midi_client, jack_midi_process, NULL);
	jack_midi_port = WJACK_port_register(jack_midi_client, "MTC in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput , 0);

	if (jack_midi_port == NULL) {
		fprintf(stderr, "can't register jack-midi-port\n");
		midi_close();
		return;
	}

	// init smpte
	tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
	last_tc.type=last_tc.min=last_tc.frame=last_tc.sec=last_tc.hour=0;

	if (WJACK_activate(jack_midi_client)) {
		fprintf(stderr, "can't activate jack-midi-client\n");
		midi_close();
	}

	if (midiid && strlen(midiid)>0) {
		const char **found_ports = WJACK_get_ports(jack_midi_client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
		if (found_ports) {
			int j;
			for (j = 0; found_ports[j]; ++j) {
				if (!strncasecmp(found_ports[j], midiid, strlen(midiid))) {
					if (want_verbose) {
						printf("JACK-connect '%s' -> '%s'\n", found_ports[j], WJACK_port_name(jack_midi_port));
					}
					if (WJACK_connect(jack_midi_client, found_ports[j], WJACK_port_name(jack_midi_port))) {
						if (!want_quiet) fprintf(stderr,"can not auto-connect jack-midi port.\n");
					}
				}
			}
			WJACK_free(found_ports);
		}
	}

}

static int jm_midi_connected(void) {
	if (jack_midi_client) return (1);
	return (0);
}

static int64_t jm_midi_poll_frame (void) {
	int64_t frame =0 ;
	static int64_t lastframe = -1 ;
	static int stopcnt = 0;

	dequeue_jmidi_events(WJACK_frames_since_cycle_start(jack_midi_client));
	frame = convert_smpte_to_frame(last_tc);

	if(midi_clkadj && (full_tc==0xff)) {
		double dly = delay>0?delay:(1.0/framerate);
		//add time that has passed sice last full MTC frame.
		double diff; // unit: smpte-frames.
		// check if transport is stuck...
		if (lastframe != frame) {
			stopcnt=0;
			lastframe=frame;
		} else if (stopcnt++ > (int) ceil(4.0*framerate/dly)) {
			// we expect a full midi MTC every (2.0*framerate/delay) polls
			full_tc=last_tc.tick=0;
			if (want_verbose)
				printf("\r\t\t\t\t\t\t        -?-\r");
		}
		diff= last_tc.tick/4.0;

		if (want_verbose)
			// subtract 7 quarter frames latency when running..
			printf("\r\t\t\t\t\t\t  |+%g/8\r",diff<0?rint(4.0*(1.75-diff)):diff<2.0?0:rint(4.0*(diff-1.75)));
		frame += (int64_t) rint(diff);
	}
	return(frame);
}

#endif  /* HAVE_JACKMIDI */

/************************************************
 * alsamidi
 */

#ifdef ALSA_RAW_MIDI /* old alsa raw midi  */

#include <alsa/asoundlib.h>

static snd_rawmidi_t *amidi= NULL;
static int ar_sysex_state = -1;
static int ar_sysex_type = 0;

static void amidi_open(char *port_name) {
	int err=0;

	if (amidi) return;
	if ((err = snd_rawmidi_open(&amidi, NULL, port_name, 0)) < 0) {
		fprintf(stderr,"cannot open port \"%s\": %s\n", port_name, snd_strerror(err));
		return;
	}

	// init smpte
	tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
	last_tc.type=last_tc.min=last_tc.frame=last_tc.sec=last_tc.hour=0;
	ar_sysex_state = -1;

	snd_rawmidi_nonblock(amidi, 1);
	//	snd_rawmidi_read(amidi, NULL, 0);
}

static void ar_midi_close(void) {
	if (!want_quiet) printf("closing alsa midi...");
	if(!amidi) return;
	snd_rawmidi_close(amidi);
	amidi=NULL;
}

// TODO increase buffer size ( avg: 15Hz * 8 msgs )
// better: standalone thread
static void amidi_event(void) {
	int i,rv;
	int npfds = 0;
	struct pollfd *pfds;
	unsigned char buf[256];
	unsigned short revents;

	npfds = snd_rawmidi_poll_descriptors_count(amidi);
	pfds = alloca(npfds * sizeof(struct pollfd));
	snd_rawmidi_poll_descriptors(amidi, pfds, npfds);

	if (poll(pfds, npfds, 0)<= 0) return;
	if (snd_rawmidi_poll_descriptors_revents(amidi, pfds, npfds, &revents) < 0) return;
	if (!(revents & POLLIN)) return;

	// TODO: loop until buffer is empty... if rv>=256
	if ((rv = snd_rawmidi_read(amidi, buf, sizeof(buf))) <=0 ) return;
	for (i = 0; i < rv; ++i) {
		int data;

		if (buf[i] == 0xf1 && (i+1 < rv) && !(buf[i+1]&0x80)) parse_timecode(buf[i+1]);
#if 1 /* parse sysex msgs */
		data = (buf[i]) & 0xFF;

		/* if this is a status byte that's not MIDI_EOX, the sysex
		 * message is incomplete and there is no more sysex data */
		if (data & 0x80 && data != MIDI_EOX && data != MIDI_SOX) {ar_sysex_state=-1;}

		// sysex- universal  real time message f0 7f ... f7
		if (data == 0xf7) { ar_sysex_state=-1;}
		else if (ar_sysex_state < 0 && data == 0xf0) { ar_sysex_state=0; ar_sysex_type=0; }
		else if (ar_sysex_state>=0) {
			ar_sysex_type = parse_sysex_urtm (data,ar_sysex_state,ar_sysex_type);
			ar_sysex_state++;
		}
#endif
	}
}

static int64_t ar_midi_poll_frame (void) {
	if (!amidi) return (0);
	amidi_event(); // process midi buffers - get most recent timecode
	return(convert_smpte_to_frame(last_tc));
}

static void ar_midi_open(char *midiid) {
	char devicestring[32];
	if (atoi(midiid)<0) {
		if (!want_quiet)
			fprintf(stdout,"AlsaMIDI does not support autodetection. using default hw:0,0,0\n");
		snprintf(devicestring,31,"hw:0,0,0");
	} else if (isdigit(midiid[0])) {
		snprintf(devicestring,31,"hw:%s",midiid);
	} else {
		snprintf(devicestring,31,"%s",midiid);
	}
	if (want_verbose)
		printf("amidi device: '%s'\n",devicestring);

	amidi_open(devicestring);
}

static int ar_midi_connected(void) {
	if (amidi) return (1);
	return (0);
}

#endif /* ALSA RAW   */

#ifdef ALSA_SEQ_MIDI /* alsa sequcer */

/************************************************
 * alsa seq midi interface
 */
	
#include <alsa/asoundlib.h>
#include <pthread.h>

// getpid()
#include <sys/types.h>
#include <unistd.h>


static pthread_t aseq_thread;
static pthread_mutex_t aseq_lock;

static snd_seq_t *seq= NULL;
static int as_sysex_type = 0;
static int aseq_stop=0; // only modify in main thread.

static void aseq_close(void) {
	if(!seq) return;
	if (!want_quiet) printf("closing alsa midi...");
	snd_seq_close(seq);
	seq=NULL;
}

static void aseq_open(char *port_name) {
	int err=0;
	snd_seq_addr_t port;
	char seq_name[32];
	snprintf(seq_name,32,"xjadeo-%i",(int) getpid());

	if (seq) return;

	/* open sequencer */ // SND_SEQ_OPEN_INPUT
	if ((err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0)) <0 ) {
		fprintf(stderr,"cannot open alsa sequencer: %s\n", snd_strerror(err));
		seq=NULL;
		return;
	}

	if ((err = snd_seq_set_client_name(seq, seq_name)) <0 ) {
		fprintf(stderr,"cannot set client name: %s\n", snd_strerror(err));
		aseq_close();
		return;
	}


	if ((err = snd_seq_create_simple_port(seq, "MTC in",
					SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
					SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
		fprintf(stderr,"cannot create port: %s\n", snd_strerror(err));
		aseq_close();
		return;
	}

	if (port_name) {
		err = snd_seq_parse_address(seq, &port, port_name);
		if (err < 0) {
			fprintf(stderr,"Cannot find port %s - %s\n", port_name, snd_strerror(err));
		}
		err = snd_seq_connect_from(seq, 0, port.client, port.port);
		if (err < 0) {
			fprintf(stderr,"Cannot connect from port %d:%d - %s\n", port.client, port.port, snd_strerror(err));
		}
	}

	snd_seq_nonblock(seq, 1);

	// init smpte
	tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
	last_tc.type=last_tc.min=last_tc.frame=last_tc.sec=last_tc.hour=0;

}

static void process_seq_event(const snd_seq_event_t *ev) {
	if (ev->type == SND_SEQ_EVENT_QFRAME) parse_timecode(ev->data.control.value);
	else if (ev->type == SND_SEQ_EVENT_SYSEX) {
		unsigned int i;
		as_sysex_type = 0;
		for (i = 1; i < ev->data.ext.len; ++i) {
			as_sysex_type = parse_sysex_urtm(((unsigned char*)ev->data.ext.ptr)[i],i-1,as_sysex_type);
		}
	}
}

static int64_t as_midi_poll_frame (void) {
	int64_t frame =0 ;
	static int64_t lastframe = -1 ;
	static int stopcnt = 0;
	if (!seq) return (0);

	pthread_mutex_lock(&aseq_lock);
	frame = convert_smpte_to_frame(last_tc);
	pthread_mutex_unlock(&aseq_lock);

	if(midi_clkadj && (full_tc==0xff)) {
		double dly = delay>0?delay:(1.0/framerate);
		//add time that has passed sice last full MTC frame.
		double diff; // unit: smpte-frames.
		// check if transport is stuck...
		if (lastframe != frame) {
			stopcnt=0;
			lastframe=frame;
		} else if (stopcnt++ > (int) ceil(4.0*framerate/dly)) {
			// we expect a full midi MTC every (2.0*framerate/delay) polls
			pthread_mutex_lock(&aseq_lock);
			full_tc=last_tc.tick=0;
			pthread_mutex_unlock(&aseq_lock);
			if (want_verbose)
				printf("\r\t\t\t\t\t\t        -?-\r");
		}
		diff= last_tc.tick/4.0;

		if (want_verbose)
			// subtract 7 quarter frames latency when running..
			printf("\r\t\t\t\t\t\t  |+%g/8\r",diff<0?rint(4.0*(1.75-diff)):diff<2.0?0:rint(4.0*(diff-1.75)));
		frame += (int64_t) rint(diff);
	}
	return(frame);
}

static void as_midi_close(void) {
	if(!seq) return;
	aseq_stop =1;
	pthread_join(aseq_thread,NULL);
	pthread_mutex_destroy(&aseq_lock);
	aseq_close();
}

static void *aseq_run(void *arg) {
	int err;
	int npfds = 0;
	struct pollfd *pfds;

	npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
	pfds = alloca(sizeof(*pfds) * npfds);
	for (;;) {
		snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);
		if (poll(pfds, npfds, 1) < 0) break;
		do {
			snd_seq_event_t *event;
			err = snd_seq_event_input(seq, &event);
			if (err < 0) break;
			if (event) {
				// TODO: lock only when actually modifying last_tc
				pthread_mutex_lock(&aseq_lock);
				process_seq_event(event);
				pthread_mutex_unlock(&aseq_lock);
			}
		} while (err > 0);
		if (aseq_stop) break;
	}
	pthread_exit(NULL);
	return (NULL);
}

/* list devices...
 * borrowed from aseqdump.c
 * Copyright (c) 2005 Clemens Ladisch <clemens@ladisch.de>
 * GPL
 */
static void as_midi_detectdevices (int print) {
	if (print) {
		snd_seq_client_info_t *cinfo;
		snd_seq_port_info_t *pinfo;

		snd_seq_client_info_alloca(&cinfo);
		snd_seq_port_info_alloca(&pinfo);

		printf(" Dumping midi seq ports: (not connecting to any)\n");
		printf("  Port    Client name                      Port name\n");

		snd_seq_client_info_set_client(cinfo, -1);
		while (snd_seq_query_next_client(seq, cinfo) >= 0) {
			int client = snd_seq_client_info_get_client(cinfo);

			snd_seq_port_info_set_client(pinfo, client);
			snd_seq_port_info_set_port(pinfo, -1);
			while (snd_seq_query_next_port(seq, pinfo) >= 0) {
				/* we need both READ and SUBS_READ */
				if ((snd_seq_port_info_get_capability(pinfo)
							& (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
						!= (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
					continue;
				printf(" %3d:%-3d  %-32.32s %s\n",
						snd_seq_port_info_get_client(pinfo),
						snd_seq_port_info_get_port(pinfo),
						snd_seq_client_info_get_name(cinfo),
						snd_seq_port_info_get_name(pinfo));
			}
		}
	}
}

static void as_midi_open(char *midiid) {
	if (atoi(midiid)<0) {
		aseq_open(NULL);
		if (want_verbose) as_midi_detectdevices(1);
	} else {
		aseq_open(midiid);
	}

	if (!seq) return;
	aseq_stop =0;
	pthread_mutex_init(&aseq_lock, NULL);
	if(pthread_create(&aseq_thread, NULL, aseq_run, NULL)) {
		fprintf(stderr,"could not start midi seq. thread\n");
		pthread_mutex_destroy(&aseq_lock);
		aseq_close();
	}
}

static int as_midi_connected(void) {
	if (seq) return (1);
	return (0);
}

#endif /*  alsa seq midi  */

static int  null_midi_connected(void) { return 0;}
static void null_midi_open(char *midiid) {;}
static void null_midi_close(void) {;}
static int64_t null_midi_poll_frame (void) { return 0L;}

#define NULLMIDI 0, &null_midi_open, &null_midi_close, &null_midi_connected, &null_midi_poll_frame

typedef struct {
	const char *name;
	int supported; // 1: format compiled in -- 0: not supported
	void (*midi_open)(char *);
	void (*midi_close)(void);
	int (*midi_connected)(void);
	int64_t (*midi_poll_frame) (void);
}midiapi;

const midiapi MA[] = {
	{ "JACK-MIDI",
#ifdef  HAVE_JACKMIDI
		1, &jm_midi_open, &jm_midi_close, &jm_midi_connected, &jm_midi_poll_frame
#else
			NULLMIDI
#endif
	},
	{ "ALSA-Sequencer",
#ifdef ALSA_SEQ_MIDI /* alsa sequcer */
		1, &as_midi_open, &as_midi_close, &as_midi_connected, &as_midi_poll_frame
#else
			NULLMIDI
#endif
	},
	{ "PORTMIDI",
#ifdef HAVE_PORTMIDI
		1, &pm_midi_open, &pm_midi_close, &pm_midi_connected, &pm_midi_poll_frame
#else
			NULLMIDI
#endif
	},
	{ "ALSA-RAW-MIDI",
#ifdef ALSA_RAW_MIDI
		1, &ar_midi_open, &ar_midi_close, &ar_midi_connected, &ar_midi_poll_frame
#else
			NULLMIDI
#endif
	},
	{NULL, NULLMIDI}  // the end.
};

int current_midi_driver = 0;

int midi_choose_driver(const char *id) {
	if (midi_connected()) return -1;
	int i=0;
	while (MA[i].name) {
		if ((id && !strncasecmp(MA[i].name, id, strlen(id))) || (!id && MA[i].supported) ) {
			current_midi_driver = i;
			break;
		}
		++i;
	}
	if (!want_quiet && MA[current_midi_driver].supported) {
		printf("selected MIDI driver: %s\n", MA[current_midi_driver].name);
	}
	return MA[current_midi_driver].supported;
}

const char *midi_driver_name() {
	return MA[current_midi_driver].name;
}

int  midi_connected(void) { return (MA[current_midi_driver].midi_connected());}
void midi_open(char *midiid) {MA[current_midi_driver].midi_open(midiid);}
void midi_close(void) {MA[current_midi_driver].midi_close();}
int64_t midi_poll_frame (void) { return (MA[current_midi_driver].midi_poll_frame());}

#else /* HAVE_MIDI */

int midi_connected(void) {
	return (0);
}

const char *midi_driver_name() {
	return "none";
}
#endif /* HAVE_MIDI */
