#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MIDI

#include <portmidi.h>
#include <porttime.h>

extern int want_quiet;
extern int want_verbose;

// TODO: increase buffer size on a slow system or few --fps
// better: spawn midi_event as thread.
//
// TODO: (later) parse midi sysex messages: F0 ... F7 for timecode
//

#define INPUT_BUFFER_SIZE 100

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
PmStream * midi = NULL;

const char MTCTYPE[4][10] = {
	"24fps",
	"25fps",
	"30fps ND",
	"30fps",
};

#if 0


#endif


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
			  printf("                                         --- %02i:%02i:%02i.%02i  [%s]        \r",tc.hour,tc.min,tc.sec,tc.frame,MTCTYPE[tc.type]);
#ifdef MIDI_DEBUG
			  printf("\n");
#endif
			  fflush(stdout);
			}
			memcpy(&last_tc,&tc,sizeof(smpte));
			tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
	}
}






int midi_detectdevices (int print) {
	int midiid=-1;
	int i;
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



void midi_open(int midi_input) {

    if (midi || midi_check(midi_input)) return ;

    // init smpte
    tc.type=tc.min=tc.frame=tc.sec=tc.hour=0;
    last_tc.type=last_tc.min=last_tc.frame=last_tc.sec=last_tc.hour=0;

    PmEvent buffer[1];
/* It is recommended to start timer before Midi; otherwise, PortMidi may
 * start the timer with its (default) parameters
 */
    Pt_Start(1, 0, 0); /* timer started w/millisecond accuracy */

    /* open input device */
    Pm_OpenInput(&midi, midi_input, NULL, INPUT_BUFFER_SIZE, ((long (*)(void *)) Pt_Time), NULL);

    if (!want_quiet) printf("Midi Input opened.\n");

    Pm_SetFilter(midi, PM_FILT_ACTIVE | PM_FILT_CLOCK);
    /* flush the buffer after setting filter, just in case anything got through */
    while (Pm_Poll(midi)) { Pm_Read(midi, buffer, 1); }

}

void midi_close(void) {
    if (!want_quiet) printf("closing midi...");
    if(!midi) return;
    Pm_Close(midi);
    midi=NULL;
}

int midi_conected(void) {
	if (midi) return (1);
	return (0);
}

void midi_event(void) {
    PmError status, length;
    PmEvent buffer[INPUT_BUFFER_SIZE];
    int i;
    /* now start paying attention to messages */
    while ((status = Pm_Poll(midi)) == TRUE ) {
            length = Pm_Read(midi,buffer, INPUT_BUFFER_SIZE);
	    for (i=0;i<length;i++)  {
#ifdef MIDI_DEBUG
                printf("MIDI rcv: %d/%d time %lu, %2lx %2lx %2lx\n",
                       i+1,length, buffer[i].timestamp,
                       Pm_MessageStatus(buffer[i].message),
                       Pm_MessageData1(buffer[i].message),
                       Pm_MessageData2(buffer[i].message));
#endif
		// parse only MTC relevant messages
		if (Pm_MessageStatus(buffer[i].message) == 0xf1) parse_timecode (Pm_MessageData1(buffer[i].message));
	    }
    }

}


long midi_poll_frame (void) {
	long frame =0 ;

	if (midi)  {
		int fps= 25; 
		midi_event(); // process midi buffers - get most recent timecode

		switch(last_tc.type) {
			case 0: fps=24; break;
			case 1: fps=25; break;
			case 2: fps=29; break;
			case 3: fps=30; break;
		}
// TODO: allow the user to choose the conversion
// in either case there is no resampling code here yet, so 
// the video file fps must match the MTC-fps !!
// (unless you know what you're doing :)
#if 1
// calc frame from SMPTE and MTC-fps
		frame = last_tc.frame + 
			fps * ( last_tc.sec + 60*last_tc.min + 3600*last_tc.hour);
#else
// the video file fps is used for converting the SMPTE
		frame = floor(frames * videoFPS / midiFPS);
#endif
	}
	return(frame);
}


#endif
