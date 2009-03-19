/* shared header file for xjadeo display backends */

  extern int movie_width, movie_height;
  extern int loop_flag, loop_run;
  extern uint8_t *buffer;

  extern int want_quiet;
  extern int want_debug;
  extern int want_verbose;
  extern int start_ontop;
  extern int start_fullscreen;
  extern int want_letterbox;
  extern int remote_en;

  extern char OSD_fontfile[1024];
  extern char OSD_text[128];
  extern char OSD_frame[48];
  extern char OSD_smpte[13];
  extern int OSD_mode;
  extern int OSD_fx, OSD_fy;
  extern int OSD_sx, OSD_sy;
  extern int OSD_tx, OSD_ty;

/* fast memcpy - see mplayer's libvo/aclib_template.c */
inline void * fast_memcpy(void * to, const void * from, size_t len);

/* prototypes in display.c */
inline void stride_memcpy(void * dst, const void * src, int width, int height, int dstStride, int srcStride);
void rgb2argb (uint8_t *rgbabuffer, uint8_t *rgbbuffer, int width, int height);
void rgb2abgr (uint8_t *rgbabuffer, uint8_t *rgbbuffer, int width, int height);

typedef struct {
	int render_fmt; // the format ffmpeg should write to the shared buffer
	int supported; // 1: format compiled in -- 0: not supported 
	const char *name; // 
	void (*render)(uint8_t *mybuffer);
	int (*open)(void);
	void (*close)(void);
	void (*eventhandler)(void);
	void (*newsrc)(void);
	void (*resize)(unsigned int x, unsigned int y);
	void (*getsize)(unsigned int *x, unsigned int *y);
	void (*position)(int x, int y);
	void (*getpos)(int *x, int *y);
	void (*fullscreen)(int action);
	void (*ontop)(int action);
	void (*mousepointer)(int action);
}vidout;

/*******************************************************************************
 * SDL
 */

#if HAVE_SDL
#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#define SUP_SDL 1
void position_sdl(int x, int y);

#else
#define SUP_SDL 0
#endif

void close_window_sdl(void);
int open_window_sdl (void);
void resize_sdl (unsigned int x, unsigned int y) ;
void getsize_sdl (unsigned int *x, unsigned int *y);
void position_sdl(int x, int y);
void render_sdl (uint8_t *mybuffer);
void newsrc_sdl (void) ;
void handle_X_events_sdl (void) ;


/*******************************************************************************
 * Shared X11 functions
 */

#if (HAVE_LIBXV || HAVE_IMLIB || HAVE_IMLIB2)

void xj_set_fullscreen (int action);
void xj_mousepointer (int action);
int xj_get_eq(char *prop, int *value);
void xj_set_ontop (int action);
void xj_position (int x, int y);
void xj_resize (unsigned int x, unsigned int y);
void xj_get_window_size (unsigned int *my_Width, unsigned int *my_Height);
void xj_get_window_pos (int *x,  int *y);

#endif

/*******************************************************************************
 * XV !!!
 */

#if HAVE_LIBXV

# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/keysym.h>
# include <X11/Xatom.h>
# include <X11/extensions/XShm.h>
# include <X11/extensions/Xvlib.h>
# include <sys/ipc.h>
# include <sys/shm.h>

# define SUP_LIBXV 1
#else
# define SUP_LIBXV 0
#endif /* HAVE_LIBXV */

void get_window_size_xv (unsigned int *my_Width, unsigned int *my_Height);
void get_window_pos_xv (int *x,  int *y);
void resize_xv (unsigned int x, unsigned int y);
void position_xv (int x, int y);
void render_xv (uint8_t *mybuffer);
void handle_X_events_xv (void);
void newsrc_xv (void); 
int open_window_xv (void); 
void close_window_xv(void);

/*******************************************************************************
 *
 * X11 / ImLib 
 */

#if HAVE_IMLIB

# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>
# include <X11/Xatom.h>
# include <Imlib.h>

# define SUP_IMLIB 1
#else
# define SUP_IMLIB 0
#endif 

void get_window_size_imlib (unsigned int *my_Width, unsigned int *my_Height);
void get_window_pos_imlib (int *x,  int *y);
int open_window_imlib (void);
void close_window_imlib(void);
void render_imlib (uint8_t *mybuffer);
void newsrc_imlib (void) ;
void handle_X_events_imlib (void);
void resize_imlib (unsigned int x, unsigned int y);
void position_imlib (int x, int y);

/*******************************************************************************
 *
 * X11 / ImLib2 
 */

#if HAVE_IMLIB2

# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xos.h>
# include <X11/Xatom.h>
# include <Imlib2.h>

# define SUP_IMLIB2 1
#else
# define SUP_IMLIB2 0
#endif 

void get_window_size_imlib2 (unsigned int *my_Width, unsigned int *my_Height);
void get_window_pos_imlib2 (int *x,  int *y);
int open_window_imlib2 (void);
void close_window_imlib2(void);
void render_imlib2 (uint8_t *mybuffer);
void newsrc_imlib2 (void) ;
void handle_X_events_imlib2 (void);
void resize_imlib2 (unsigned int x, unsigned int y);
void position_imlib2 (int x, int y);

/*******************************************************************************
 *
 * Max Osx - quartz
 */

#ifdef HAVE_MACOSX

# define SUP_MACOSX 1
#else
# define SUP_MACOSX 0
#endif 

void get_window_size_mac (unsigned int *my_Width, unsigned int *my_Height);
void getpos_mac (int *x,  int *y);
int open_window_mac (void);
void close_window_mac(void);
void render_mac (uint8_t *mybuffer);
void newsrc_mac (void) ;
void handle_X_events_mac (void);
void resize_mac (unsigned int x, unsigned int y);
void position_mac (int x, int y);
void getsize_mac (unsigned int *x, unsigned int *y);

