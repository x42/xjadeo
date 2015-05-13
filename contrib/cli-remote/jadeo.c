/* 
 * xjadeo windowless remote-master
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
 * (C) 2006 Robin Gareus <robin@gareus.org>
 *
 * This is a quick hack to generate xjadeo remote commands
 * to stdout. this version also reads from stdin and echos
 * commands to stdout.
 *
 * the default "remote command header" is hardcoded.
 *
 *
 * examples:
 *  ./xjadeo-rcli | xjadeo -R <file> &>/dev/null
 *  ./xjadeo-rcli | rsh <host> xjadeo -R <remote-file> 
 *  ./xjadeo-rcli | ssh <[user@]host> rjadeo.sh -R <remote-file> 
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <xjadeo.h>

double framerate;  // =  frames / duration;
char *program_name;

long frames = 25;
double duration =1;
int jack_autostart = 1;

/* hardcoded settings */
int want_quiet = 1;
int want_verbose = 0;
int want_debug = 0;
#ifdef HAVE_MIDI
int midi_clkconvert = 0;
int midi_clkadj = 0;
char *midiid = NULL;
#endif
int want_autodrop =1;   /* --nodropframes -n (hidden option) */
int want_dropframes =0; /* --dropframes -N  BEWARE! */
int have_dropframes =0;
double 	delay = 0.04; // HERE: for MTC timeout only 

#ifdef JACK_SESSION
// XXX jadeo does not actually support JACK-SESSION
// but it uses xjadeo's jack.c
char *jack_uuid = NULL;                                                                                                                        
int loop_flag = 1;                                                  
int interaction_override =0;
#endif  

/* mode of operation */
int jack = 1;
int readfromstdin = 1; // set to 0 or 1!

void usage(int status) {
	printf("usage %s [fps-num:=25] [fps-den:=1]\n",program_name);
	exit(status);
}

int main (int argc, char **argv) {
	int run;
	char *filename = NULL;
	long frame, pframe;
	program_name = argv[0];

	if (argc>3) usage(1);
	if (argc==3) {
		filename=argv[2];
		frames=atol(argv[1]);
	}
	else if (argc>1) filename=argv[1];

	if (frames < 1 || duration < 1) usage(1);
	if (!filename) usage(1);

	framerate = (double) frames / (double) duration;

	struct timeval tv;
	fd_set set;

	//TODO: trap some signals.. -> run=0;

	if (jack) {
		open_jack();
		run= jack_connected();
	} else{
#ifdef HAVE_MIDI
		midi_open(midiid);
#endif
		run= midi_connected();
		
	}

	printf ("jack disconnect\n");
	printf ("load %s\n",filename);
	printf ("window mouse off\n");
	printf ("window letterbox on\n");
	printf ("window fullscreen on\n");
//	printf ("window resize 880x545\n");
	printf ("osd font /usr/share/fonts/truetype/freefont/FreeMonoBold.ttf\n");
	printf ("osd off\n");
	printf ("osd smpte 100\n");
	printf ("set fps %.0f\n",framerate);

	pframe=-1;
	while (run) {
		tv.tv_sec = 0;
		tv.tv_usec = 1000000/framerate;
		FD_ZERO(&set);
		if (readfromstdin) FD_SET(0, &set);

		if (select(0+readfromstdin, &set, NULL, NULL, &tv) ) if (readfromstdin) {
			size_t rv;
			char buf[BUFSIZ];
			if ((rv=read(0,buf,BUFSIZ)) > 0) {
				if (!strncmp(buf,"exit",3)) { 
					printf ("quit\n");
					run=0;
				}else 
					write(1,buf,rv);
			}
		}

		if (jack) frame= jack_poll_frame(NULL);
#ifdef HAVE_MIDI
		else frame= midi_poll_frame();
#else
		else exit (1);
#endif
		if (pframe!=frame)
			printf ("seek %li\n",frame);
		pframe=frame;

		fflush(stdout);
	}

	if (jack) close_jack();
#ifdef HAVE_MIDI
	else midi_close();
#endif
	return (0);
}

int saveconfig (const char *filename) {
	return 0;
}
/* vim:set ts=8 sts=8 sw=8: */
