/* xjadeo - OSC remote control
 *
 * Copyright (C) 2007,2009 Robin Gareus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation;
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

#define OSC_DOC_ALL

/* extract doc for website, doc/pages/osc.html table:
 *
 * xjadeo --osc-doc \
 * | grep -ve '^#' \
 * | sed 's% %</code></td><td><code>%;s% %</code></td><td>%;s%^%<tr><td><code>%;s%$%</td></tr>%'
 *
 */

#ifdef HAVE_LIBLO

#include <unistd.h>
#include <lo/lo_lowlevel.h>
#include "xjadeo.h"

extern int      want_verbose;
extern int      want_quiet;
extern int      loop_flag;
extern int      loop_run;
extern int      force_redraw;
extern int      movie_width;
extern int64_t  userFrame;
extern int64_t  dispFrame;
extern double   delay;
extern int64_t  ts_offset;

#ifdef HAVE_MIDI
extern int midi_clkconvert;
extern int midi_clkadj;
extern char midiid[32];
#endif

#ifdef TIMEMAP
extern int64_t timeoffset;
extern double  timescale;
extern int     wraparound;
#endif

#ifdef CROPIMG
extern int xoffset;
#endif

extern double delay;
extern double framerate;

// OSD
extern char OSD_fontfile[1024];
extern int OSD_mode;

static int oscb_seek (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  userFrame=argv[0]->i;
  return(0);
}

static int oscb_fps (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- f:%f\n", path, argv[0]->f);
  if (argv[0]->f>0) delay= 1.0 / argv[0]->f;
  else delay = -1; // use file-framerate
  return(0);
}

static int oscb_offset (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  ts_offset = argv[0]->i;
  return(0);
}

static int oscb_offsetsmpte (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- s:%s\n", path, &argv[0]->s);
  ts_offset = smptestring_to_frame((char*)&argv[0]->s);
  return(0);
}

static int oscb_jackconnect (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  open_jack();
  return(0);
}

static int oscb_jackdisconnect (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  close_jack();
  return(0);
}

#if defined HAVE_LTC || defined OSC_DOC_ALL
static int oscb_ltcconnect (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  open_ltcjack(NULL);
  return(0);
}

static int oscb_ltcdisconnect (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  close_ltcjack();
  return(0);
}
#endif

#if defined HAVE_MIDI || defined OSC_DOC_ALL
static int oscb_midiconnect (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef HAVE_MIDI
  char *mp;
  if (&argv[0]->s && strlen(&argv[0]->s)>0) mp=&argv[0]->s;
  else mp="-1";
  midi_open(mp);
  if (midi_connected()) {
    strncpy(midiid,mp,32); // XXX we need a better idea for 'xapi_reopen_midi()
    midiid[31]=0;
  }
#endif
  return(0);
}

static int oscb_mididisconnect (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef HAVE_MIDI
  midi_close();
#endif
  return(0);
}
#endif

#if defined TIMEMAP || defined OSC_DOC_ALL
static int oscb_timescale (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef TIMEMAP
  if (want_verbose) fprintf(stderr, "OSC: %s <- f:%f\n", path, argv[0]->f);
  timescale=argv[0]->f;
  force_redraw=1;
#endif
  return(0);
}

static int oscb_timescale2 (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef TIMEMAP
  if (want_verbose) fprintf(stderr, "OSC: %s <- f:%f i:%i\n", path, argv[0]->f, argv[1]->i);
  timescale=argv[0]->f;
  timeoffset=argv[1]->i;
  force_redraw=1;
#endif
  return(0);
}

static int oscb_loop (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef TIMEMAP
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  wraparound = argv[0]->i?1:0;
  return(0);
#endif
}

static int oscb_reverse (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef TIMEMAP
  if (want_verbose) fprintf(stderr, "OSC: %s\n", path);
  timescale *= -1.0;
  if (timescale<0)
    timeoffset = (-2.0*timescale) * dispFrame; // TODO: check file-offset and ts_offset. -> also in remote.c
  else
    timeoffset = 0; // TODO - applt diff dispFrame <> transport src
#endif
  return(0);
}
#endif

#if defined CROPIMG || defined OSC_DOC_ALL
static int oscb_pan (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
#ifdef CROPIMG
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  xoffset=argv[0]->i;
  if (xoffset<0) xoffset=0;
  if (xoffset>movie_width) xoffset=movie_width;
  force_redraw=1;
#endif
  return(0);
}
#endif

static int oscb_load (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- s:%s\n", path, &argv[0]->s);
  open_movie(&argv[0]->s);
  init_moviebuffer();
  newsourcebuffer();
  force_redraw=1;
  return(0);
}

static int oscb_osdfont (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- s:%s\n", path, &argv[0]->s);
  snprintf(OSD_fontfile,1024,"%s",(char*) &argv[0]->s);
  return(0);
}

static int oscb_osdframe (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  if (argv[0]->i)
    OSD_mode|=OSD_FRAME;
  else
    OSD_mode&=~OSD_FRAME;
  force_redraw=1;
  return(0);
}

static int oscb_osdsmtpe (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  if (argv[0]->i)
    OSD_mode|=OSD_SMPTE;
  else
    OSD_mode&=~OSD_SMPTE;
  force_redraw=1;
  return(0);
}

static int oscb_osdbox (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  if (argv[0]->i)
    OSD_mode|=OSD_BOX;
  else
    OSD_mode&=~OSD_BOX;
  force_redraw=1;
  return(0);
}

static int oscb_remotecmd (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- s:%s\n", path, &argv[0]->s);
  exec_remote_cmd (&argv[0]->s);
  return(0);
}

// X11 options
#if 0
static int oscb_fullscreen (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  int action=_NET_WM_STATE_TOGGLE;
  if (!strcmp(d,"on") || atoi(d)==1) action=_NET_WM_STATE_ADD;
  else if (!strcmp(d,"off")) action=_NET_WM_STATE_REMOVE;
  remote_printf(100,"ok.");
  Xfullscreen(action);
  return(0);
}

static int oscb_mousepointer (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  if (want_verbose) fprintf(stderr, "OSC: %s <- i:%i\n", path, argv[0]->i);
  int action=2;
  if (!strcmp(d,"on") || atoi(d)==1) action=1;
  else if (!strcmp(d,"off")) action=0;
  Xmousepointer (action);
  remote_printf(100,"ok.");
  return(0);
}
#endif

// general

static int oscb_quit (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data){
  fprintf(stderr, "OSC 'quit' command recv.\n");
  loop_flag=0;
  return(0);
}

static void oscb_error (int num, const char *m, const char *path) {
  fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
}

//////////////////////////////////////////////////////////////////////////////

struct osc_command {
  const char *path;
  const char *typespec;
  lo_method_handler handler;
  const char *documentation;
};

static struct osc_command OSCC[] = {
  {"/jadeo/quit", "", &oscb_quit, "Terminate xjadeo."},
  {"/jadeo/load", "s", &oscb_load, "Load a video file, replacing any previous one (load <filename>)"},
  {"/jadeo/seek", "i", &oscb_seek, "Seek to given frame-number (seek <frame>) - Xjadeo needs to be disconnected from a sync-source"},
  {"/jadeo/cmd", "s", &oscb_remotecmd, "Call a remote control command"},

  {"/jadeo/fps", "f", &oscb_fps, "Set the screen update frequency (-f, set fps)"},
  {"/jadeo/offset", "i", &oscb_offset, "Set time-offset as frame-number (-o, set offset)"},
  {"/jadeo/offset", "s", &oscb_offsetsmpte, "Set time-offset as timecode (-o, set offset)"},

  {"/jadeo/osd/font", "s", &oscb_osdfont, "Specify a TrueType Font file to be used for rendering On-Screen-Display text (osd font)"},
  {"/jadeo/osd/timecode", "i", &oscb_osdsmtpe, "If set to 1: render timecode on screen; set to 0 to disable (-i, osd smpte)"},
  {"/jadeo/osd/framenumber", "i", &oscb_osdframe, "If set to 1: render frame-number on screen, set to 0 to disable (-i, osd frame)"},
  {"/jadeo/osd/box", "i", &oscb_osdbox, "If set to 1: draw a black backround around OSD elements, set to 0 to disable (osd box, osd nobox)"},

  {"/jadeo/jack/connect", "", &oscb_jackconnect, "Connect to JACK and sync to JACK-transport (jack connect)"},
  {"/jadeo/jack/disconnect", "", &oscb_jackdisconnect, "Stop synchronization with JACK-transport (jack disconnect)"},

#if defined HAVE_LTC || defined OSC_DOC_ALL
  {"/jadeo/ltc/connect", "", &oscb_ltcconnect, "Synchronize to LTC from jack-audio port (ltc connect)"},
  {"/jadeo/ltc/disconnect", "", &oscb_ltcdisconnect, "Close LTC/JACK client (ltc disconnect)"},
#endif

#if defined HAVE_MIDI || defined OSC_DOC_ALL
  // HAVE_MIDI
  {"/jadeo/midi/connect", "s", &oscb_midiconnect, "Get sync from MTC (MIDI Time Code). The parameter specifies the midi-port to connect to. (-m, -d, midi connect)"},
  {"/jadeo/midi/disconnect", "", &oscb_mididisconnect, "Close the MIDI device (midi disconnect)"},
#endif

#if defined CROPIMG || defined OSC_DOC_ALL
  {"/jadeo/art/pan", "i", &oscb_pan, "Set the x-offset to the value given in pixels. 0 ≤ val ≤ movie-width"},
#endif

#if defined TIMEMAP || defined OSC_DOC_ALL
  {"/jadeo/art/timescale", "f", &oscb_timescale, "Set time-multiplier; default value: 1.0"},
  {"/jadeo/art/timescale", "fi", &oscb_timescale2, "Set both time-multiplier and offset. default: 1.0, 0"},
  {"/jadeo/art/loop", "i", &oscb_loop, "Enable wrap-around/loop video. If set to 1, multiples of the movie-length are added/subtracted if the current time-stamp is outside the file duration."},
  {"/jadeo/art/reverse", "", &oscb_reverse, "Trigger reverse. This action multiplies the current time-scale with -1.0 and sets a time-offset so that the currently displayed frame is retained."},
#endif
};


//////////////////////////////////////////////////////////////////////////////

static lo_server osc_server = NULL;

int xjosc_initialize (int osc_port) {
  char tmp[8];
  uint32_t port = (osc_port>100 && osc_port< 60000)?osc_port:7000;

  snprintf(tmp, sizeof(tmp), "%d", port);
  fprintf(stderr, "OSC trying port:%i\n",port);
  osc_server = lo_server_new (tmp, oscb_error);
  //fprintf (stderr,"OSC port %i is in use.\n", port);

  if (!osc_server) {
    if(!want_quiet) fprintf(stderr, "OSC start failed.");
    return(1);
  }

  if(!want_quiet) {
    char *urlstr;
    urlstr = lo_server_get_url (osc_server);
    fprintf(stderr, "OSC server name: %s\n",urlstr);
    free (urlstr);
  }
  int i;
  for (i = 0; i < sizeof(OSCC) / sizeof(struct osc_command); ++i) {
    lo_server_add_method(osc_server, OSCC[i].path, OSCC[i].typespec, OSCC[i].handler, NULL);
  }

  if(want_verbose) fprintf(stderr, "OSC server started on port %i\n",port);
  return (0);
}

int xjosc_process (void) {
  int rv = 0;
  if (!osc_server) return 0;
  while (lo_server_recv_noblock(osc_server, 0) > 0) {
    rv++;
  }
  return rv;
}

void xjosc_shutdown (void) {
  if (!osc_server) return;
  lo_server_free(osc_server);
  if(!want_verbose) fprintf(stderr, "OSC server shut down.\n");
}

void xjosc_documentation (void) {
  printf("# Xjadeo OSC methods. Format:\n");
  printf("# Path <space> Typespec <space> Documentation <newline>\n");
  printf("#######################################################\n");
  int i;
  for (i = 0; i < sizeof(OSCC) / sizeof(struct osc_command); ++i) {
    printf("%s %s %s\n", OSCC[i].path, OSCC[i].typespec, OSCC[i].documentation);
  }
}

#else
int  xjosc_initialize(int osc_port) {return(1);}
void xjosc_shutdown(void) {;}
int  xjosc_process(void) {return(0);}
void xjosc_documentation (void) {
  printf("# This version of xjadeo is compiled without OSC support.\n");
}
#endif

/* vi:set ts=8 sts=2 sw=2: */
