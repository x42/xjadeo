
  extern int movie_width, movie_height;
  extern int loop_flag, loop_run;
  extern uint8_t *buffer;

  extern int want_quiet;
  extern int want_verbose;
  extern int remote_en;

  extern char OSD_fontfile[1024];
  extern char OSD_text[128];
  extern char OSD_frame[48];
  extern char OSD_smpte[13];
  extern int OSD_mode;
  extern int OSD_fx, OSD_fy;
  extern int OSD_sx, OSD_sy;
  extern int OSD_tx, OSD_ty;


  inline void stride_memcpy(void * dst, const void * src, int width, int height, int dstStride, int srcStride);

/*******************************************************************************
 * GTK
 */

#define HAVE_MYGTK (HAVE_GTK && HAVE_GDK_PIXBUF )

#if HAVE_MYGTK
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

#define SUP_GTK 1
#else
#define SUP_GTK 0
#endif

void resize_gtk (unsigned int x, unsigned int y);
void getsize_gtk (unsigned int *x, unsigned int *y);
void position_gtk (int x, int y);
int open_window_gtk(int *argc, char ***argv);
void close_window_gtk(void);
void render_gtk (uint8_t *mybuffer);
void handle_X_events_gtk (void);


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
int open_window_sdl (int *argc, char ***argv);
void resize_sdl (unsigned int x, unsigned int y) ;
void getsize_sdl (unsigned int *x, unsigned int *y);
void position_sdl(int x, int y);
void render_sdl (uint8_t *mybuffer);
void newsrc_sdl (void) ;
void handle_X_events_sdl (void) ;

/*******************************************************************************
 * XV !!!
 */

#if HAVE_LIBXV
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#define SUP_LIBXV 1
#else
#define SUP_LIBXV 0
#endif /* HAVE_LIBXV */

void get_window_size_xv (unsigned int *my_Width, unsigned int *my_Height);
void get_window_pos_xv (int *x,  int *y);
void resize_xv (unsigned int x, unsigned int y);
void position_xv (int x, int y);
void render_xv (uint8_t *mybuffer);
void handle_X_events_xv (void);
void newsrc_xv (void); 
int open_window_xv (int *argc, char ***argv); 
void close_window_xv(void);

/*******************************************************************************
 *
 * X11 / ImLib 
 */

#if HAVE_IMLIB

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>

#include <Imlib.h>

#define SUP_IMLIB 1
#else
#define SUP_IMLIB 0
#endif 

void get_window_size_imlib (unsigned int *my_Width, unsigned int *my_Height);
void get_window_pos_imlib (int *x,  int *y);
int open_window_imlib (int *argc, char ***argv);
void close_window_imlib(void);
void render_imlib (uint8_t *mybuffer);
void newsrc_imlib (void) ;
void handle_X_events_imlib (void);
void resize_imlib (unsigned int x, unsigned int y);
void position_imlib (int x, int y);

