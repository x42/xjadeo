/* xjadeo - video output abstraction and common display functions */

#ifndef XJADEO_DISPLAY_H

extern int movie_width, movie_height;
extern int ffctv_width, ffctv_height;
extern float movie_aspect;
extern int loop_flag, loop_run;
extern uint8_t *buffer;

extern int want_quiet;
extern int want_debug;
extern int want_verbose;
extern int start_ontop;
extern int start_fullscreen;
extern int want_letterbox;
extern int hide_mouse;
extern int remote_en;
extern int force_redraw;
extern int interaction_override;

extern char OSD_fontfile[1024];
extern char OSD_text[128];
extern char OSD_frame[48];
extern char OSD_smpte[20];
extern char OSD_msg[128];
extern char OSD_nfo_tme[5][48];
extern char OSD_nfo_geo[5][48];
extern int OSD_mode;
extern int OSD_fx, OSD_fy;
extern int OSD_sx, OSD_sy;
extern int OSD_tx, OSD_ty;
extern uint8_t osd_seeking;

#define PB_H (20)
#define PB_X (10)
#define BAR_Y ((OSD_mode & OSD_SMPTE && OSD_sy > 80) || (OSD_mode & (OSD_FRAME | OSD_VTC) && OSD_fy > 80) ? .89 : .95)
#define OSD_MIN_NFO_HEIGHT (160)

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
	int  (*getfullscreen)(void);
	int  (*getontop)(void);
	void (*letterbox_change)(void);
} vidout;


/*******************************************************************************
 * SDL
 */

#ifdef HAVE_SDL
#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#define SUP_SDL 1
void get_window_pos_sdl (int *rx, int *ry);
int sdl_get_fullscreen ();
void sdl_toggle_fullscreen(int action);
void mousecursor_sdl(int action);
void sdl_set_ontop (int action);
int sdl_get_ontop ();

void close_window_sdl(void);
int open_window_sdl (void);
void resize_sdl (unsigned int x, unsigned int y) ;
void getsize_sdl (unsigned int *x, unsigned int *y);
void position_sdl(int x, int y);
void render_sdl (uint8_t *mybuffer);
void newsrc_sdl (void) ;
void handle_X_events_sdl (void) ;
void sdl_letterbox_change (void);

#else
#define SUP_SDL 0
#endif


/*******************************************************************************
 * Shared X11 functions
 */

#if (defined HAVE_LIBXV || defined HAVE_IMLIB2)

void xj_set_fullscreen (int action);
void xj_mousepointer (int action);
void xj_set_ontop (int action);
void xj_position (int x, int y);
void xj_resize (unsigned int x, unsigned int y);
void xj_get_window_size (unsigned int *my_Width, unsigned int *my_Height);
void xj_get_window_pos (int *x,  int *y);
int  xj_get_ontop ();
int  xj_get_fullscreen ();
void xj_letterbox();
int  xj_get_eq(char *prop, int *value);

#endif

#if (defined HAVE_LIBXV || defined HAVE_IMLIB2 || (defined HAVE_GL && !defined PLATFORM_WINDOWS && !defined PLATFORM_OSX))

#ifdef DND
#include <X11/Xlib.h>
#include <X11/Xatom.h>

void init_dnd (Display *dpy, Window win);
void disable_dnd (Display *dpy, Window win);
int handle_dnd_event (Display *dpy, Window win, XEvent *event);
#endif // DND

#ifdef XDLG
#include <X11/Xlib.h>
int show_x_dialog(Display *dpy, Window parent, int x, int y);
void close_x_dialog(Display *dpy);
int handle_xdlg_event (Display *dpy, XEvent *event);
#endif

#ifdef XFIB
int   x_fib_show (Display *dpy, Window parent, int x, int y);
void  x_fib_close (Display *dpy);
int   x_fib_handle_events (Display *dpy, XEvent *event);
int   x_fib_status ();
char *x_fib_filename ();
int   x_fib_cfg_filter_callback (int (*cb)(const char*));
int fib_filter_movie_filename (const char *name);
#endif


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

#if defined PLATFORM_OSX && (defined __i386 || defined __ppc__)

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
void fullscreen_mac (int a);
void ontop_mac (int a);
int  get_fullscreen_mac();
int  get_ontop_mac();
void window_resized_mac();
void mac_letterbox_change();


#ifdef HAVE_GL
# define SUP_OPENGL 1
void gl_render (uint8_t *mybuffer);
int  gl_open_window ();
void gl_close_window();
void gl_handle_events ();
void gl_newsrc ();

void gl_resize (unsigned int x, unsigned int y);
void gl_get_window_size (unsigned int *w, unsigned int *h);
void gl_position (int x, int y);
void gl_get_window_pos (int *x,  int *y);

void gl_set_ontop (int action);
void gl_set_fullscreen (int action);
void gl_mousepointer (int action);
int  gl_get_ontop ();
int  gl_get_fullscreen ();
void gl_letterbox_change ();
#else
# define SUP_OPENGL 0
#endif

#endif // XJADEO_DISPLAY_H
