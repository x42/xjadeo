#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h> 	/* uint8_t */
#include <string.h> 	/* memcpy */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* prototypes of fn's in  display.c */
void handle_X_events (void);
void newsourcebuffer (void);
void handle_X_events (void);
void close_window(void);
void open_window(int *argc, char ***argv);
void render_buffer (uint8_t *mybuffer);
int vidoutmode(int user_req);
int getvidmode (void);
void Xresize (unsigned int x, unsigned int y);
void Xgetsize (unsigned int *x, unsigned int *y);
void Xposition (int x, int y);

/* remote.c */
void close_remote_ctrl (void) ;
void open_remote_ctrl (void);
int remote_fd_set(fd_set *fd);
int remote_read(void);
void remote_printf(int val, const char *format, ...);

/* xjadeo.c */
void display_frame(int64_t timestamp, int force_update);
int open_movie(char* file_name);
int close_movie();
void avinit (void);
void init_moviebuffer(void);
void event_loop(void);
void do_try_this_file_and_exit(char *movie);

/* jack.c function prototype */
long jack_poll_frame (void);
void open_jack(void );
void close_jack(void);

/* smpte.c prototypes */

long int smptestring_to_frame (char *str);
void frame_to_smptestring(char *smptestring, long int frame);

/* midi.c function prototype */
#ifdef HAVE_MIDI
long midi_poll_frame (void);
int midi_detectdevices (int print);
void midi_open(int midi_input);
void midi_close(void);
#endif


/* freetype - On screen display */
enum { OSD_LEFT=0, OSD_CENTER, OSD_RIGHT };

#define OSD_BOX (256)
#define OSD_FRAME (1)
#define OSD_SMPTE (2)
#define OSD_TEXT (64)

#define ST_PADDING (10)

//#define FONT_FILE       "arial.ttf"
#define FONT_FILE       "/usr/share/fonts/truetype/msttcorefonts/arial.ttf"

int render_font (char *fontfile, char *text);
#ifdef HAVE_FT
  #define ST_WIDTH   1024
  #define ST_HEIGHT  30
#else  /* Have_FT */
  #define ST_WIDTH   0
  #define ST_HEIGHT  0
#endif /* Have_FT */

