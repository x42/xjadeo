/* 
 * xjadeo remote-master
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
 * (C) 2006,2008 Robin Gareus <robin@gareus.org>
 *
 * This is a quick hack to generate xjadeo remote commands
 * to stdout. this version also reads from stdin and echos
 * commands to stdout.
 *
 * the default "remote command header" is hardcoded.
 *
 *
 * examples:
 *  ./osceo | xjadeo -R &>/dev/null
 *  ./osceo | xjadeo -R --vo ImLib2/x11 <file> 
 *  ./osceo | rsh <host> xjadeo -q -R <remote-file>  &>/dev/null
 *  ./osceo | ssh <[user@]host> rjadeo.sh -R <remote-file> 
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <signal.h>

int xjosc_initialize(int osc_port);
void xjosc_shutdown(void);

char *program_name;
int c_run = 1; //< keep running

/* options */
long port = 2525;
double framerate;  // =  frames / duration;
int readfromstdin =1; // pass thru remote control command
int want_verbose = 0; // verbose output 


void usage(int status) {
	printf("usage %s [[<osc port>] video-filename]\n",program_name);
	exit(status);
}


void catchsig (int sig) {
  c_run=0;
  signal(SIGHUP, catchsig); /* reset signal */
  signal(SIGINT, catchsig);
  signal(SIGPIPE, catchsig);
  if (want_verbose)
    fprintf(stderr,"\n CAUGHT signal - shutting down.\n");
  printf("quit\n");
  fflush(stdout);
  exit(1);
}


int main (int argc, char **argv) {
	char *filename = NULL;
	program_name = argv[0];
// TODO : proper command line args and usage..
	if (argc>3) usage(1);
	if (argc==3) {
		filename=argv[2];
		port=atol(argv[1]);
	}
	else if (argc>1) filename=argv[1];
	//if (!filename) usage(1);

	framerate = 25.0;

	signal (SIGHUP, catchsig);
	signal (SIGINT, catchsig);
	signal (SIGPIPE, catchsig);

	printf ("jack disconnect\n");
	if (filename)
		printf ("load %s\n",filename);
//	printf ("window mouse off\n");
	printf ("window ontop on\n");
	printf ("window letterbox on\n");
//	printf ("window fullscreen on\n");
//	printf ("window resize 640x480\n");
//	printf ("window resize 880x545\n");
	printf ("osd font /usr/share/fonts/truetype/freefont/FreeMonoBold.ttf\n");
	printf ("osd off\n");
	printf ("osd smpte 100\n");
//	printf ("set fps %.0f\n",framerate);
	fflush(stdout);

	xjosc_initialize(port);

	struct timeval tv;
	fd_set set;

	while (c_run) {

		tv.tv_sec = 0;
		tv.tv_usec = 1000000/framerate;
		FD_ZERO(&set);
		if (readfromstdin) FD_SET(fileno(stdin), &set);

		if (select(readfromstdin?(1+fileno(stdin)):0, &set, NULL, NULL, &tv) ) if (readfromstdin) {
			size_t rv;
			char buf[BUFSIZ];
			if ((rv=read(fileno(stdin),buf,BUFSIZ)) > 0) {
				if (!strncmp(buf,"exit",3)) { 
					printf ("quit\n");
					c_run=0;
				}else {
					write(fileno(stdout),buf,rv);
				}
			}
			fflush(stdout);
		}

	}

	xjosc_shutdown();
	return (0);
}
/* vim:set ts=8 sts=8 sw=8: */
