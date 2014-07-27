/* 
   jplay2 - JACK audio player for multimedia files

   Copyright (C) 2007 Robin Gareus

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 
   as published by the Free Software Foundation;

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <lo/lo.h>
#include <fcntl.h>
#include <pthread.h>

extern int want_verbose;
extern int c_run;

int oscb_fps (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "%s <- f:%f\n", path, argv[0]->f);
  printf("set fps %f\n", argv[0]->f);
  fflush(stdout);
  return(0);
}

int oscb_seek (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "%s <- i:%i\n", path, argv[0]->i);
  printf("seek %i\n", argv[0]->i);
  fflush(stdout);
  return(0);
}

int oscb_load (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "%s <- s:%s\n", path, &argv[0]->s);
  printf("load %s\n", &argv[0]->s);
  fflush(stdout);
  return(0);
}

int oscb_quit (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  fprintf(stderr, "OSC 'quit' command recv.\n");
  printf("quit\n");
  fflush(stdout);
  return(0);
}

static void oscb_error(int num, const char *m, const char *path) {
  fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
}

lo_server_thread osc_server = NULL;

#define PORTAINC 1

int xjosc_initialize(int osc_port) {
  char tmp[255];
  int j;
  uint32_t port = (osc_port>100 && osc_port< 60000)?osc_port:7000;
  for (j=0; j < PORTAINC; ++j) {
    snprintf(tmp, sizeof(tmp), "%d", port);
    fprintf(stderr, "OSC trying port:%i\n",port);
    if ((osc_server = lo_server_thread_new (tmp, oscb_error))) break;
    fprintf (stderr,"OSC port %i is in use.\n", port);
    port++;
  }

  if (osc_server) {
    char *urlstr;
    urlstr = lo_server_thread_get_url (osc_server);
    fprintf(stderr, "OSC server name: %s\n",urlstr);
    free (urlstr);

    lo_server_thread_add_method(osc_server, "/jadeo/fps", "f",  &oscb_fps, NULL);
    lo_server_thread_add_method(osc_server, "/jadeo/seek", "i", &oscb_seek, NULL);
    lo_server_thread_add_method(osc_server, "/jadeo/load", "S", &oscb_load, NULL);
    lo_server_thread_add_method(osc_server, "/jadeo/quit", "", &oscb_quit, NULL);

    lo_server_thread_start(osc_server);
    if(want_verbose) fprintf(stderr, "OSC server started on port %i\n",port);
    return (0);
  } 

  if(want_verbose) fprintf(stderr, "OSC start failed.");
  return(1);
}

void xjosc_shutdown(void) {
  if (!osc_server) return;
  lo_server_thread_stop(osc_server);
  if(!want_verbose) fprintf(stderr, "OSC server shut down.\n");
}

/* vi:set ts=8 sts=2 sw=2: */
