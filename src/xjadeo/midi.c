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
 * many kudos to the portmidi developers and their
 * example code...
 *
 * the alsa midi code was inspired by the alsa-tools
 * amidi.c written by Clemens Ladisch <clemens@ladisch.de>
 */

#include <xjadeo.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_MIDI
extern int want_quiet;
extern int want_verbose;
extern double framerate;
extern int midi_clkconvert;


/*
 * common xjadeo MTC 
 * SMPTE / parse raw midi data...
 */
typedef struct {
	int frame;
	int sec;
	int min;
	int hour;

	int day; //  overflow
	int type;
} smpte;


/* global Vars */
smpte tc;
smpte last_tc;

const char MTCTYPE[4][10] = {
	"24fps",
	"25fps",
	"29fps",
	"30fps",
};

/* parse MTC 0x71 message data */
void parse_timecode( int data) {
	switch (data>>4) {
		case 0x0: // #0000 frame LSN
			tc.frame= ( tc.frame&(~0xf)) | (data&0xf); break;
		case 0x1: // #0001 frame MSN
			tc.frame= (tc.frame&(~0xf0)) | ((data&0xf)<<4); break;
		case 0x2: // #0010 sec LSN
			tc.sec= ( tc.sec&(~0xf)) | (data&0xf); break;
		case 0x3: // #0011 sec MSN
			tc.sec= (tc.sec&(~0xf0)) | ((data&0xf)<<4); break;
		case 0x4: // #0100 min LSN
			tc.min= ( tc.min&(~0xf)) | (data&0xf); break;
		case 0x5: // #0101 min MSN
			tc.min= (tc.min&(~0xf0)) | ((data&0xf)<<4); break;
		case 0x6: // #0110 hour LSN
			tc.hour= ( tc.hour&(~0xf)) | (data&0xf); break;
		case 0x7: // #0111 hour MSN and type
			tc.hour= (tc.hour&(~0xf0)) | ((data&1)<<4);
			tc.type = (data>>1)&3;
			if (want_verbose) {
			  printf("\t\t\t\t\t---  %02i:%02i:%02i.%02i [%s]       \r",tc.hour,tc.min,tc.sec,tc.frame,MTCTYPE[tc.type]);
			  fflush(stdout);
			}
			memcpy(&last_tc,&tc,sizeof(smpte));
			tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
		default: 
			;
	}
}

/* parse system exclusive MSGs  
 * seek frame - if transport is not rolling */
int parse_sysex_urtm (int data, int state, int type) {

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
 *  roland sysex real time message - reverse engineered format
 *         06 xx 06 xx: [fps (bit 6..5) hour: bit4..0)] [min] [sec] [frame] Checksum F7
 *         01 01: [fps (bit 6..5) hour: bit4..0)] [min] [sec] [frame] F7
 *
 *  fps:  (011HHHHH): 30fps, 29fps (non drop)
 *        (010HHHHH :  29 fps (drop)
 *	  (001HHHHH):  25 fps
 *	  (000HHHHH):  24 fps
 */

	int rv=type;
	if (type<0) return (-1); 
	if (state<2 && data !=0x7f) return(-1);
	if (state<3 && type>0) return (-1);

	if (state==2 && type==0) {
		if (data==0x01) rv=1;
		if (data==0x06) rv=2;
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
	if (state>7 && type ==2 ) return (-1);

	// type==2 && state6,7,8,9
	if (state==6 && type ==2 ) { last_tc.hour=(data&0x1f); last_tc.type=(data>>5)&3; } // hour + format
	if (state==7 && type ==2 ) { last_tc.min=(data&0x7f); } // min
	if (state==8 && type ==2 ) { last_tc.sec=(data&0x7f); } // sec
	if (state==9 && type ==2 ) { last_tc.frame=(data&0x7f); } // frame
	if (state>9 && type ==2 ) return (-1);

	return (rv);
}



/*
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

PmStream * midi = NULL;

/* if INPUT_BUFFER_SIZE is 0, PortMidi uses a default value */
#define INPUT_BUFFER_SIZE 0


int midi_detectdevices (int print) {
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

int midi_check (int midiid) {
	if (midiid < 0 || midiid >=Pm_CountDevices()) {
		fprintf(stderr,"Error: invalid midi device id.\n");
		return(-1);
	}
	return(0);
}


int active = FALSE;

int sysex_state = -1;
int sysex_type = 0; 

#define MIDI_SOX 0xf0
#define MIDI_EOX 0xf7

/* shared queues */
PmQueue *midi_to_main;
PmQueue *main_to_midi;

/* timer interrupt for processing midi data */
void process_midi(PtTimestamp timestamp, void *userData)
{
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
		}
		memcpy(&msg,&last_tc,sizeof(smpte));
                Pm_Enqueue(midi_to_main, &msg);
        }
    } while (result);
     
    /* see if there is any midi input to process */
    do {
	result = Pm_Poll(midi);
        if (result) {
	    int shift,data;
	    shift=data=0;

            if (Pm_Read(midi, &buffer, 1) == pmBufferOverflow) continue;

	    /* parse only MTC relevant messages */
	    if (Pm_MessageStatus(buffer.message) == 0xf1)
		parse_timecode (Pm_MessageData1(buffer.message));

	    for (shift = 0; shift < 32 && (data != MIDI_EOX); shift += 8) {
		data = (buffer.message >> shift) & 0xFF;

		/* if this is a status byte that's not MIDI_EOX, the sysex
		 * message is incomplete and there is no more sysex data */
		if (data & 0x80 && data != MIDI_EOX && data != MIDI_SOX) break;

		// sysex- universal  real time message f0 7f ... f7 
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


void midi_open(char *midiid) {
    int midi_input;
    if (midi) return;

    midi_input = atoi(midiid);
    if (want_verbose && midi_input < 0) midi_input = midi_detectdevices(1);
    else if (midi_input <0 ) midi_input = midi_detectdevices(0);

    if (midi_check(midi_input)) return ;

    // init smpte
    tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
    last_tc.type=last_tc.min=last_tc.frame=last_tc.sec=last_tc.hour=0;

    midi_to_main = Pm_QueueCreate(2, sizeof(smpte));
    main_to_midi = Pm_QueueCreate(2, sizeof(smpte));
    if (!midi_to_main || !main_to_midi ) {
	    fprintf(stderr, "Could not create portmidi queues\n");
	    return;
    }
    
    PmEvent buffer[1];
/* It is recommended to start timer before Midi; otherwise, PortMidi may
 * start the timer with its (default) parameters
 */
    Pt_Start(1, &process_midi, 0); /* timer started w/millisecond accuracy */

    Pm_Initialize();

    /* open input device */
//  Pm_OpenInput(&midi, midi_input, NULL, INPUT_BUFFER_SIZE, ((long (*)(void *)) Pt_Time), NULL);
    Pm_OpenInput(&midi, midi_input, NULL, INPUT_BUFFER_SIZE, NULL, NULL);

    if (!want_quiet) printf("Midi Input opened.\n");

    Pm_SetFilter(midi, PM_FILT_ACTIVE | PM_FILT_CLOCK);
    /* flush the buffer after setting filter, just in case anything got through */
    while (Pm_Poll(midi)) { Pm_Read(midi, buffer, 1); }

    active = TRUE; 

}

void midi_close(void) {
    smpte cmd;

    if (!want_quiet) printf("closing midi...");
    if(!midi) return;

    cmd.frame=0xaffe; // shutdown CMD 
    Pm_Enqueue(main_to_midi, &cmd); 
    while (Pm_Dequeue(midi_to_main, &cmd)==0) ; // spin 

    Pt_Stop(); /* stop the timer */
    Pm_QueueDestroy(midi_to_main);
    Pm_QueueDestroy(main_to_midi);

    Pm_Close(midi);
    midi=NULL;
}

int midi_connected(void) {
	if (midi) return (1);
	return (0);
}

long midi_poll_frame (void) {
	long frame =0 ;
	int fps= 25; 
	int spin;
    	smpte now;
	if (!midi) return (0);

	now.frame=0; // CMD request
	Pm_Enqueue(main_to_midi, &now); // request data
	do {
		spin = Pm_Dequeue(midi_to_main, &now);
	} while (spin == 0); /* spin */ ;


	switch(now.type) {
		case 0: fps=24; break;
		case 1: fps=25; break;
		case 2: fps=29; break;
		case 3: fps=30; break;
	}
	switch (midi_clkconvert) {
		case 2: // force video fps
			frame = now.frame +  (int)
				floor(framerate * ( now.sec + 60*now.min + 3600*now.hour));
		break;
		case 3: // 'convert' FPS.
			frame = now.frame + 
				fps * ( now.sec + 60*now.min + 3600*now.hour);
			frame = (int) rint(frame * framerate / fps);
		break;
		default: // use MTC fps info
			frame = now.frame + 
				fps * ( now.sec + 60*now.min + 3600*now.hour);
	}
	return(frame);
}

#else  /* endif HAVE_PORTMIDI */

/*
 * alsamidi 
 */

#include <alsa/asoundlib.h>

static snd_rawmidi_t *amidi= NULL;

void amidi_open(char *port_name) {
	int err=0;
	if (amidi) return;
	if ((err = snd_rawmidi_open(&amidi, NULL, port_name, 0)) < 0) {
		fprintf(stderr,"cannot open port \"%s\": %s", port_name, snd_strerror(err));
		return;
	}
	snd_rawmidi_read(amidi, NULL, 0); 
	snd_rawmidi_nonblock(amidi, 1);
}

void amidi_close(void) {
    if (!want_quiet) printf("closing alsa midi...");
    if(!amidi) return;
    snd_rawmidi_close(amidi);
    amidi=NULL;
}

void amidi_event(void) {
	int i,rv;
	int npfds = 0;
	struct pollfd *pfds;
	unsigned char buf[256];
	unsigned short revents;

	snd_rawmidi_nonblock(amidi, 1);
	npfds = snd_rawmidi_poll_descriptors_count(amidi);
	pfds = alloca(npfds * sizeof(struct pollfd));
	snd_rawmidi_poll_descriptors(amidi, pfds, npfds);

	if (poll(pfds, npfds, 0)<= 0) return;
	if (snd_rawmidi_poll_descriptors_revents(amidi, pfds, npfds, &revents) < 0) return;
	if (!(revents & POLLIN)) return;

	if ((rv = snd_rawmidi_read(amidi, buf, sizeof(buf))) <=0 ) return;
	for (i = 0; i < rv; ++i)
		if (buf[i] == 0xf1 && (i+1 < rv) && !(buf[i+1]&0x80)) parse_timecode(buf[i+1]);
	// TODO: parse sysex messages.
}

long amidi_poll_frame (void) {
	long frame =0 ;
	int fps= 25; 
	if (!amidi) return (0);

	amidi_event(); // process midi buffers - get most recent timecode

	switch(last_tc.type) {
		case 0: fps=24; break;
		case 1: fps=25; break;
		case 2: fps=29; break;
		case 3: fps=30; break;
	}
	switch (midi_clkconvert) {
		case 2: // force video fps
			frame = last_tc.frame +  (int)
				floor(framerate * ( last_tc.sec + 60*last_tc.min + 3600*last_tc.hour));
		break;
		case 3: // 'convert' FPS.
			frame = last_tc.frame + 
				fps * ( last_tc.sec + 60*last_tc.min + 3600*last_tc.hour);
			frame = (int) rint(frame * framerate / fps);
		break;
		default: // use MTC fps info
			frame = last_tc.frame + 
				fps * ( last_tc.sec + 60*last_tc.min + 3600*last_tc.hour);
	}
	return(frame);
}

inline long midi_poll_frame (void) { return (amidi_poll_frame() ); }
inline void midi_close(void) {amidi_close();}

void midi_open(char *midiid) {
	char devicestring[32];
	if (atoi(midiid)<0) {
		fprintf(stderr,"AlsaMIDI does not support autodetection. using default hw:2,0,0\n");
		snprintf(devicestring,31,"hw:2,0,0");
	} else if (isdigit(midiid[0])) {
		snprintf(devicestring,31,"hw:%s",midiid);
	} else {
		snprintf(devicestring,31,"%s",midiid);
	}
	if (want_verbose) 
		printf("amidi device: '%s'\n",devicestring);
	amidi_open(devicestring); 
}

int midi_detectdevices (int print) { 
	if (print) printf("use 'amidi -l' to list Midi ports\n");
	return(0);
}

int midi_connected(void) {
	if (amidi) return (1);
	return (0);
}

#endif /* no HAVE_PORTMIDI -> alsamidi */

#endif /* HAVE_MIDI */
