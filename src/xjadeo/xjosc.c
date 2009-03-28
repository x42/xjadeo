/* 
   xjadeo - OSC remote control

   Copyright (C) 2007,2009 Robin Gareus

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


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_LIBLO

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <lo/lo.h>
#include "xjadeo.h"

extern int	want_verbose;
extern int	want_quiet;
extern int	loop_flag;
extern int	loop_run;
extern int 	force_redraw;
extern int 	movie_width;
extern long	userFrame;

#ifdef CROPIMG
  extern int xoffset;
#endif

extern double	delay;
extern double	framerate;

int oscb_seek (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "%s <- i:%i\n", path, argv[0]->i);
  userFrame=argv[0]->i;
  return(0);
}

#ifdef CROPIMG
int oscb_pan (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "%s <- i:%i\n", path, argv[0]->i);
  xoffset=argv[0]->i;
  if (xoffset<0) xoffset=0;
  if (xoffset>movie_width) xoffset=movie_width; 
  force_redraw=1;
  return(0);
}
#endif

int oscb_load (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (1 || want_verbose) fprintf(stderr, "%s <- s:%s\n", path, &argv[0]->s);
  loop_run=0; // TODO lock !!
  double dly = delay>0?delay:(1.0/framerate);
  usleep(ceil(2e6*dly)); 
  open_movie(&argv[0]->s);
  init_moviebuffer();
  newsourcebuffer();
  force_redraw=1;
  loop_run=1;
  return(0);
}

int oscb_quit (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  fprintf(stderr, "OSC 'quit' command recv.\n");
  loop_flag=0;
  return(0);
}

static void oscb_error(int num, const char *m, const char *path) {
  fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
}

//////////////////////////////////////////////////////////////////////////////

lo_server_thread osc_server = NULL;

int initialize_osc(int osc_port) {
  char tmp[8];
  uint32_t port = (osc_port>100 && osc_port< 60000)?osc_port:7000;

  snprintf(tmp, sizeof(tmp), "%d", port);
  fprintf(stderr, "OSC trying port:%i\n",port);
  osc_server = lo_server_thread_new (tmp, oscb_error);
  fprintf (stderr,"OSC port %i is in use.\n", port);

  if (!osc_server) {
    if(want_verbose) fprintf(stderr, "OSC start failed.");
    return(1);
  }

  if(!want_quiet) {
    char *urlstr;
    urlstr = lo_server_thread_get_url (osc_server);
    fprintf(stderr, "OSC server name: %s\n",urlstr);
    free (urlstr);
  }

  lo_server_thread_add_method(osc_server, "/jadeo/seek", "i", &oscb_seek, NULL);
  lo_server_thread_add_method(osc_server, "/jadeo/load", "S", &oscb_load, NULL);
#ifdef CROPIMG
  lo_server_thread_add_method(osc_server, "/jadeo/pan", "i", &oscb_pan, NULL);
#endif
  lo_server_thread_add_method(osc_server, "/jadeo/quit", "", &oscb_quit, NULL);

  lo_server_thread_start(osc_server);
  if(want_verbose) fprintf(stderr, "OSC server started on port %i\n",port);
  return (0);
}

void shutdown_osc(void) {
  if (!osc_server) return;
  lo_server_thread_stop(osc_server);
  if(!want_verbose) fprintf(stderr, "OSC server shut down.\n");
}

#else
int initialize_osc(int osc_port) {return(1);}
void shutdown_osc(void) {;}
#endif

/* vi:set ts=8 sts=2 sw=2: */
