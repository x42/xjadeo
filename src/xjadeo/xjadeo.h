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

