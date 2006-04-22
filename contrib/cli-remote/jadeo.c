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
 *  ./xjadeo-rcli | xjadeo -R &>/dev/null
 *  ./xjadeo-rcli | rsh <host> xjadeo -R <file> 
 *  ./xjadeo-rcli | ssh <[user@]host> rjadeo.sh -R <file> 
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <xjadeo.h>

/* values for jack.c time conversion:
 *   = x * frames / duration
 */ 
long frames = 25;
double duration =1;

/* value for midi.c time conversion 
 *   = x * framerate 
 * this is also used as timebase for the internal loop, 
 */ 
double framerate;  // =  frames / duration;

/* hardcoded settings */
int want_quiet = 1;
int want_verbose = 0;
int midi_clkconvert = 0;
char *midiid = NULL;

/* mode of operation */
int jack = 1;
int readfromstdin = 1; // set to 0 or 1!


int main (int argc, char **argv) {
	int run;
	long frame, pframe;

	framerate = (double) frames / (double) duration;

	struct timeval tv;
	fd_set set;

	//TODO: trap some signals.. -> run=0;

	if (jack) {
		open_jack();
		run= jack_connected();
	} else{
		midi_open(midiid);
		run= midi_connected();
	}

	printf ("jack disconnect\n");
//	printf ("window resize 880x545\n");
//	printf ("osd font /var/lib/defoma/gs.d/dirs/fonts/FreeMonoBold.ttf\n");
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
				if (!strncmp(buf,"die",3)) { 
					printf ("quit\n");
					run=0;
				}else 
					write(1,buf,rv);
			}
		}

		if (jack) frame= jack_poll_frame();
		else frame= midi_poll_frame();
		if (pframe!=frame)
			printf ("seek %li\n",frame);
		pframe=frame;

		fflush(stdout);
	}

	//fprintf(stderr,"Live long and prosper.\n");

	if (jack) close_jack();
	else midi_close();

	return (0);
}
/* vim:set ts=8 sts=8 sw=8: */
