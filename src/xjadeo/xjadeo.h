#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h> 	/* uint8_t */
#include <string.h> 	/* memcpy */
#include <sys/select.h> 


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_LASH
# include <lash/lash.h>
#endif

#include <avutil.h>
#if LIBAVUTIL_BUILD >= 0x320000
#define PIX_FMT_RGBA32 PIX_FMT_RGBA
#endif

/* xjadeo seek modes */
enum { 	SEEK_ANY, ///< directly seek to givenvideo frame 
	SEEK_KEY, ///< seek to next keyframe after given frame.
	SEEK_CONTINUOUS }; ///< seek to keframe before this frame and advance to current frame.

/* freetype - On screen display */
enum { OSD_LEFT=-1, OSD_CENTER=-2, OSD_RIGHT=-3 }; ///< use positive values as percent or pixel.

#define OSD_FRAME (1)
#define OSD_SMPTE (2)

#define OSD_EQ    (8)
#define OSD_OFFS (16)
#define OSD_OFFF (32)
#define OSD_TEXT (64)

#define OSD_BOX (256)

#ifdef TTFFONTFILE
# define FONT_FILE TTFFONTFILE
#else 
# define FONT_FILE       "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf"
#endif

/* define maximum size for OSD in pixel */
#ifdef HAVE_FT
# define ST_WIDTH   (1024)
# define ST_HEIGHT  (30)
# define ST_PX      (24)
#else 
# define ST_WIDTH   (0)
# define ST_HEIGHT  (0)
#endif 

#define ST_PADDING  (10)


/* X11 only - but defined here since needed in remote.c and display.c
 * EWMH state actions, see
	 http://freedesktop.org/Standards/wm-spec/index.html#id2768769 */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

/* prototypes in lash.c */
void lash_process();
void lcs_str(char *key, char *value);
void lcs_int(char *key, int value);
void lcs_long(char *key, long int  value);
void lcs_dbl(char *key, double value);

#ifdef HAVE_LASH
  void lash_setup();
#endif

/* prototypes of fn's in  display.c */
void newsourcebuffer (void);
void close_window(void);
void open_window(void);

int vidoutmode(int user_req);
int parsevidoutname (char *arg);
int vidoutsupported (int i);
int getvidmode (void);
const char *vidoutname (int i);
int try_next_vidoutmode(int user_req);
void splash (uint8_t *mybuffer);

void render_buffer (uint8_t *mybuffer);
void handle_X_events (void);
void Xresize (unsigned int x, unsigned int y);
void Xfullscreen (int a);
void Xmousepointer (int a);
void Xletterbox (int a);
void Xontop (int a);
void Xgetsize (unsigned int *x, unsigned int *y);
void Xposition (int x, int y);
void Xgetpos (int *x, int *y);

/* remote.c */
void close_remote_ctrl (void) ;
void open_remote_ctrl (void);
void close_mq_ctrl (void) ;
void open_mq_ctrl (void);
int remote_fd_set(fd_set *fd);
int remote_read_mq(void);
int remote_read_io(void);
void remote_printf(int val, const char *format, ...);

/* xjadeo.c */
void display_frame(int64_t timestamp, int force_update);
int open_movie(char* file_name);
int close_movie();
void avinit (void);
void override_fps (double fps);
void init_moviebuffer(void);
void event_loop(void);
void do_try_this_file_and_exit(char *movie);

/* jack.c function prototypes */
long jack_poll_frame (void);
void open_jack(void );
void close_jack(void);
int jack_connected(void);

/* smpte.c prototypes */
long int smptestring_to_frame (char *str, int autodrop);
void frame_to_smptestring(char *smptestring, long int frame, int autodrop);
long int smpte_to_frame(int type, int f, int s, int m, int h, int overflow);

/* midi.c function prototype */
int midi_connected(void);
#ifdef HAVE_MIDI
long midi_poll_frame (void);
void midi_open(char *midiid);
void midi_close(void);
#endif

/* configfile.c */
void xjadeorc (void);

/* freetype - On screen display */
int render_font (char *fontfile, char *text);

