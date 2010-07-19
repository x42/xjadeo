/* xjadeo - jack video monitor
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 *
 * (c) 2008 Robin Gareus <robin@gareus.org>
 *
 * this code was inspired by mplayer's libvo/vo_quartz.c 
 * (c) 2004 Nicolas Plourde <nicolasplourde@gmail.com>
 * (C) 2004 Romain Dolbeau <romain@dolbeau.org>
 *
 *
 */
#include "xjadeo.h"

void xapi_open(void *d);
void jackt_stop();
void jackt_start();
void jackt_toggle();
void jackt_rewind();

int midi_connected(void);
int jack_connected(void);

extern int loop_flag;
extern int VOutput;
extern int OSD_mode; // change via keystroke
extern long ts_offset; 
extern char *smpte_offset;
extern int force_redraw; // tell the main event loop that some cfg has changed
extern int want_letterbox;
extern int seekflags;
extern int interaction_override; // disable some options.

extern int movie_width;
extern int movie_height;
extern double delay;
extern double framerate;
extern int start_fullscreen;
extern int start_ontop;

int keep_aspect = 0 ; // don't allow resizing window other than in aspect.

#ifdef CROPIMG
  extern int xoffset;
#endif

#define memcpy_pic(d, s, b, h, ds, ss) memcpy_pic2(d, s, b, h, ds, ss, 0)
static inline void * memcpy_pic2(void * dst, const void * src, 
                                 int bytesPerLine, int height, 
                                 int dstStride, int srcStride, int limit2width) 
{ 
  int i; 
  void *retval=dst; 

  if(!limit2width && dstStride == srcStride) { 
    if (srcStride < 0) { 
      src = (uint8_t*)src + (height-1)*srcStride; 
      dst = (uint8_t*)dst + (height-1)*dstStride; 
      srcStride = -srcStride; 
    } 
    memcpy(dst, src, srcStride*height); 
  } else { 
    for(i=0; i<height; i++) 
    { 
      memcpy(dst, src, bytesPerLine); 
      src = (uint8_t*)src + srcStride; 
      dst = (uint8_t*)dst + dstStride; 
    } 
  } 
  return retval; 
}


/*******************************************************************************
 * Mac OSX - QuartzCore/QuickTime
 */

#ifdef HAVE_MACOSX

#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>
//#include <QuickTime/ImageCodec.h>
//

#ifdef WORDS_BIGENDIAN           
#define be2me_16(x) (x)
#define be2me_32(x) (x)
#define be2me_64(x) (x)
#define le2me_16(x) bswap_16(x)
#define le2me_32(x) bswap_32(x)
#define le2me_64(x) bswap_64(x)
#else
#define be2me_16(x) bswap_16(x) 
#define be2me_32(x) bswap_32(x)
#define be2me_64(x) bswap_64(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)
#endif


int levelList[] = {
    kCGDesktopWindowLevelKey,
    kCGNormalWindowLevelKey,
    kCGScreenSaverWindowLevelKey
};

/// prototypes
static void flip_page(void);
static OSStatus KeyEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
static OSStatus MouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
static OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
void mac_put_key(UInt32 key, UInt32 charcode);
OSStatus mac_menu_cmd(OSStatus result, HICommand *acmd);

///
static int vo_fs = 0; // enter fullscreen - user setting
static int vo_mac_fs = 0; // we are in fullscreen

///
static int mouseHide = 0;
static int winLevel = 1; // always on top 
static Rect imgRect; // size of the original image (unscaled)
static Rect dstRect; // size of the displayed image (after scaling)
static Rect winRect; // size of the window containg the displayed image (include padding)
static Rect oldWinRect; // size of the window containg the displayed image (include padding) when NOT in fullscreen mode
static Rect oldWinBounds; // last size before entering full-screen 

static Rect deviceRect; // size of the display device
static int device_width;
static int device_height;
static int device_id;

static short fs_res_x=0; // full screen res
static short fs_res_y=0;

static MenuRef windMenu;
static MenuRef movMenu;
static MenuRef osdMenu;
static MenuRef scrnMenu;
static MenuRef zoomMenu;
static MenuRef seekMenu;
static MenuRef jackMenu;
static MenuRef syncMenu;
static MenuRef osdoMenu;
#if 0
static MenuRef fpsMenu;
static MenuRef fileMenu;
#endif

enum // menubar
{
        mQuit                = 1,
        mOpen,
        mSettings,
        mHalfScreen,
        mNormalScreen,
        mDoubleScreen,
        mFullScreen,
        mKeepAspect, // letterbox
        mOSDOffset, 
        mOSDFrame, 
        mOSDSmpte, 
        mOSDBox, 
    //  mOSDFont, 
        mOSDOffO, 
        mOSDOffS, 
        mOSDOffF, 
        mSyncJack,
        mSyncJackMidi,
        mSyncPortMidi,
        mSyncNone,
        mJackPlay,
        mJackStop,
        mJackRewind,
        mSeekAny, 
        mSeekKeyFrame, 
        mSeekContinuous
};

static WindowRef theWindow = NULL;
static WindowGroupRef winGroup = NULL;
static CGContextRef context;
static GDHandle deviceHdl; // main display device

static CGRect bounds; // RGB[A] image boundaries
static CGDataProviderRef dataProviderRef; // RGB[A] ref
static CGImageRef image; // RGB[A] image

// YUV & CoDec
static uint32_t image_depth;
static uint32_t image_format;
static uint32_t image_size;
static uint32_t image_buffer_size;
static char *image_data = NULL;

static ImageSequence seqId;
//static CodecType image_qtcodec;
static PlanarPixmapInfoYUV420 *P = NULL;
static uint8_t *yuvbuf = NULL;
static struct
{
  ImageDescriptionHandle desc;
  Handle extension_colr;
  Handle extension_fiel;
  Handle extension_clap;
  Handle extension_pasp;
} yuv_qt_stuff;

static MatrixRecord matrix;


////////////////////////////////////////////////////////////////////

// update checked menu items
static void checkMyMenu(void) {
  CheckMenuItem (seekMenu, 1, seekflags==SEEK_ANY);
  CheckMenuItem (seekMenu, 2, seekflags==SEEK_KEY);
  CheckMenuItem (seekMenu, 3, seekflags==SEEK_CONTINUOUS);

  CheckMenuItem (osdMenu, 1, (OSD_mode&OSD_FRAME)!=0);
  CheckMenuItem (osdMenu, 2, (OSD_mode&OSD_SMPTE)!=0);
  CheckMenuItem (osdMenu, 5, (OSD_mode&OSD_BOX)!=0);

  CheckMenuItem (osdoMenu, 1, (OSD_mode&(OSD_OFFS|OSD_OFFF))==0);
  CheckMenuItem (osdoMenu, 2, (OSD_mode&OSD_OFFS)!=0);
  CheckMenuItem (osdoMenu, 3, (OSD_mode&OSD_OFFF)!=0);

  CheckMenuItem (zoomMenu, 6, want_letterbox);

  CheckMenuItem (syncMenu, 1, jack_connected());
  CheckMenuItem (syncMenu, 4, midi_connected());
  CheckMenuItem (syncMenu, 5, (!jack_connected() && !midi_connected()));
}

// main window setup and painting..
static void mac_CreateWindow(uint32_t d_width, uint32_t d_height, WindowAttributes windowAttrs) 
{
  CFStringRef    titleKey;
  CFStringRef    windowTitle; 
  OSStatus       result;
  
  MenuItemIndex index;
  // FIXME : these may leak memory when re-opening the window.
  CFStringRef   movMenuTitle;
  CFStringRef   zoomMenuTitle;
  CFStringRef   seekMenuTitle;
  CFStringRef   jackMenuTitle;
  CFStringRef   osdMenuTitle;
  CFStringRef   osdoMenuTitle;
  CFStringRef   scrnMenuTitle;
  CFStringRef   syncMenuTitle;
#if 0
  CFStringRef   fpsMenuTitle;
  CFStringRef   fileMenuTitle;
#endif
  const EventTypeSpec win_events[] = {
    { kEventClassWindow, kEventWindowClosed },
    { kEventClassWindow, kEventWindowBoundsChanged },
    { kEventClassCommand, kEventCommandProcess }
  };

  const EventTypeSpec key_events[] = {
    { kEventClassKeyboard, kEventRawKeyDown },
    { kEventClassKeyboard, kEventRawKeyRepeat }
  };

  const EventTypeSpec mouse_events[] = {
    { kEventClassMouse, kEventMouseMoved },
  //{ kEventClassMouse, kEventMouseWheelMoved },
    { kEventClassMouse, kEventMouseDown },
    { kEventClassMouse, kEventMouseUp },
  //{ kEventClassMouse, kEventMouseDragged }
  };

  SetRect(&winRect, 0, 0, d_width, d_height);
  SetRect(&oldWinRect, 0, 0, d_width, d_height);
  SetRect(&dstRect, 0, 0, d_width, d_height);
  
  //Clear Menu Bar
  ClearMenuBar();
  
  //Create Window Menu
  CreateStandardWindowMenu(0, &windMenu);
  InsertMenu(windMenu, 0);

#if 1
  EnableMenuCommand(0, kHICommandPreferences);
//DisableMenuCommand(0, kHICommandServices);
//AEInstallEventHandler(kCoreEventClass, kAEShowPreferences, NewAEEventHandlerUPP(WindowEventHandler), NULL, false);
#endif
  
////Create Movie Menu
  CreateNewMenu (1004, 0, &movMenu);
  movMenuTitle = CFSTR("Movie");
  SetMenuTitleWithCFString(movMenu, movMenuTitle);
  
  AppendMenuItemTextWithCFString(movMenu, CFSTR("Load File"), 0, mOpen, &index);
  AppendMenuItemTextWithCFString(movMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  AppendMenuItemTextWithCFString(movMenu, CFSTR("Seek"), 0, 0, &index);

#if 0
  AppendMenuItemTextWithCFString(movMenu, CFSTR("Attributes"), 0, 0, &index);

  //Create File Attribute Menu
  CreateNewMenu (0, 0, &fileMenu);
  fileMenuTitle = CFSTR("View");
  SetMenuTitleWithCFString(fileMenu, fileMenuTitle);
  SetMenuItemHierarchicalMenu(movMenu, 4, fileMenu);
  AppendMenuItemTextWithCFString(fileMenu, CFSTR("use file's FPS"), 1, 0, &index); //XXX
  AppendMenuItemTextWithCFString(fileMenu, CFSTR("override FPS"), 1, 0, &index); //XXX
#endif
#if 0
  AppendMenuItemTextWithCFString(movMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  AppendMenuItemTextWithCFString(movMenu, CFSTR("About"), 0, kHICommandAbout, &index);
#endif
  //Create seek Menu
  CreateNewMenu (0, 0, &seekMenu);
  seekMenuTitle = CFSTR("Seek");
  SetMenuTitleWithCFString(seekMenu, seekMenuTitle);
  SetMenuItemHierarchicalMenu(movMenu, 3, seekMenu);
  
  AppendMenuItemTextWithCFString(seekMenu, CFSTR("to any frame"), 0, mSeekAny, &index);
  AppendMenuItemTextWithCFString(seekMenu, CFSTR("keyframes only"), 0, mSeekKeyFrame, &index);
  AppendMenuItemTextWithCFString(seekMenu, CFSTR("Continuously"), 0, mSeekContinuous, &index);

////Create Screen Menu
  CreateNewMenu (0, 0, &scrnMenu);
  scrnMenuTitle = CFSTR("Screen");
  SetMenuTitleWithCFString(scrnMenu, scrnMenuTitle);
  AppendMenuItemTextWithCFString(scrnMenu, CFSTR("OSD"), 0, 0, &index); 
  AppendMenuItemTextWithCFString(scrnMenu, CFSTR("Zoom"), 0, 0, &index); 
#if 0
  AppendMenuItemTextWithCFString(scrnMenu, CFSTR("Monitor Speed"), 0, 0, &index);

  //Create FPS Menu
  CreateNewMenu (0, 0, &fpsMenu);
  fpsMenuTitle = CFSTR("Monitor Speed");
  SetMenuTitleWithCFString(fpsMenu, fpsMenuTitle);
  SetMenuItemHierarchicalMenu(scrnMenu, 3, fpsMenu);

  AppendMenuItemTextWithCFString(fpsMenu, CFSTR("at screen fps"), 1, 0, &index); //XXX
  AppendMenuItemTextWithCFString(fpsMenu, CFSTR("at files fps"), 1, 0, &index); //XXX
//CheckMenuItem (fpsMenu, 2, (delay==-1));
  AppendMenuItemTextWithCFString(fpsMenu, CFSTR("custom"), 1, 0, &index); //XXX
#endif
   
  //Create OSD Menu
  CreateNewMenu (0, 0, &osdMenu);
  osdMenuTitle = CFSTR("OSD");
  SetMenuTitleWithCFString(osdMenu, osdMenuTitle);
  SetMenuItemHierarchicalMenu(scrnMenu, 1, osdMenu);

  AppendMenuItemTextWithCFString(osdMenu, CFSTR("Frameno."), 0, mOSDFrame, &index);
  AppendMenuItemTextWithCFString(osdMenu, CFSTR("SMPTE"), 0, mOSDSmpte, &index);
  AppendMenuItemTextWithCFString(osdMenu, CFSTR("SMPTE-Offset"), 0, 0, &index);
  AppendMenuItemTextWithCFString(osdMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  AppendMenuItemTextWithCFString(osdMenu, CFSTR("Box"), 0, mOSDBox, &index);
//AppendMenuItemTextWithCFString(osdMenu, CFSTR("Font"), 0, mOSDFont, &index);

  CreateNewMenu (0, 0, &osdoMenu);
  osdoMenuTitle = CFSTR("SMPTE-Offset");
  SetMenuTitleWithCFString(osdoMenu, osdoMenuTitle);
  SetMenuItemHierarchicalMenu(osdMenu, 3, osdoMenu);
  AppendMenuItemTextWithCFString(osdoMenu, CFSTR("off"), 0, mOSDOffO, &index);
  AppendMenuItemTextWithCFString(osdoMenu, CFSTR("as SMPTE"), 0, mOSDOffS, &index);
  AppendMenuItemTextWithCFString(osdoMenu, CFSTR("as Frameno"), 0, mOSDOffF, &index);

  //Create Zoom Menu
  CreateNewMenu (0, 0, &zoomMenu);
  zoomMenuTitle = CFSTR("Zoom");
  SetMenuTitleWithCFString(zoomMenu, zoomMenuTitle);
  SetMenuItemHierarchicalMenu(scrnMenu, 2, zoomMenu);
  
  AppendMenuItemTextWithCFString(zoomMenu, CFSTR("Half Size"), 0, mHalfScreen, &index);
  AppendMenuItemTextWithCFString(zoomMenu, CFSTR("Normal Size"), 0, mNormalScreen, &index);
  AppendMenuItemTextWithCFString(zoomMenu, CFSTR("Double Size"), 0, mDoubleScreen, &index);
  AppendMenuItemTextWithCFString(zoomMenu, CFSTR("Full Screen"), 0, mFullScreen, &index);
  AppendMenuItemTextWithCFString(zoomMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  AppendMenuItemTextWithCFString(zoomMenu, CFSTR("Letterbox"), 0, mKeepAspect, &index);

  //Create Sync Menu
  CreateNewMenu (0, 0, &syncMenu);
  syncMenuTitle = CFSTR("Sync");
  SetMenuTitleWithCFString(syncMenu, syncMenuTitle);
  
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("JACK"), 0, mSyncJack, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("Jack Transport"), 0, 0, &index);
  AppendMenuItemTextWithCFString(syncMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("MTC (portmidi)"), 0, mSyncPortMidi, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("MTC (jackmidi)"), 0, mSyncJackMidi, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("none"), 0, mSyncNone, &index);

  //Create Jack Menu
  CreateNewMenu (0, 0, &jackMenu);
  jackMenuTitle = CFSTR("Jack Transport");
  SetMenuTitleWithCFString(jackMenu, jackMenuTitle);
  SetMenuItemHierarchicalMenu(syncMenu, 2, jackMenu);
  
  AppendMenuItemTextWithCFString(jackMenu, CFSTR("Play"), 0, mJackPlay, &index);
  AppendMenuItemTextWithCFString(jackMenu, CFSTR("Stop"), 0, mJackStop, &index);
  AppendMenuItemTextWithCFString(jackMenu, CFSTR("Rewind"), 0, mJackRewind, &index);

  checkMyMenu();
  InsertMenu(movMenu, GetMenuID(windMenu)); 
  InsertMenu(scrnMenu, GetMenuID(windMenu)); 
  InsertMenu(syncMenu, GetMenuID(windMenu)); //insert before Window menu
  DrawMenuBar();
  
  //create window
  CreateNewWindow(kDocumentWindowClass, windowAttrs, &winRect, &theWindow);
  
  CreateWindowGroup(0, &winGroup);
  SetWindowGroup(theWindow, winGroup);

  //Set window title
  titleKey  = CFSTR("jadeo");
  windowTitle = CFCopyLocalizedString(titleKey, NULL);
  result    = SetWindowTitleWithCFString(theWindow, windowTitle);
  CFRelease(titleKey);
  CFRelease(windowTitle);
  
  //Install event handler
  InstallApplicationEventHandler (NewEventHandlerUPP (KeyEventHandler), GetEventTypeCount(key_events), key_events, NULL, NULL);
  InstallApplicationEventHandler (NewEventHandlerUPP (MouseEventHandler), GetEventTypeCount(mouse_events), mouse_events, NULL, NULL);
  InstallWindowEventHandler (theWindow, NewEventHandlerUPP (WindowEventHandler), GetEventTypeCount(win_events), win_events, theWindow, NULL);
}
////////////////////////////////////////////////////////

// TODO 
static OSStatus NavOpenSettings(void) {
  OSStatus status = 0;
#if 0
  Rect setRect;
  WindowRef theSettings = NULL;
  SetRect(&setRect, 0, 0, 300, 200);
  WindowAttributes windowAttrs;
  windowAttrs = kWindowNoShadowAttribute | kWindowOpaqueForEventsAttribute;
  CreateNewWindow(kDocumentWindowClass, kWindowStandardDocumentAttributes, &setRect, &theSettings);
#else
  DialogRef settingsDialog;
  CreateStandardAlert(kAlertNoteAlert, CFSTR("settings dialog"), CFSTR("not yet implemented. use .xjadeorc file in your $HOME to make settings persist."),NULL, &settingsDialog);
  RunStandardAlert(settingsDialog, NULL, NULL);
#endif
  return(status);
}

////////////////////////////////////////////////////////

static OSStatus OpenDocuments( AEDescList docList ) {
  long         index;
  FSRef        fsRef;
  //CFStringRef  fileName;
  long         count = 0;
  OSStatus status = AECountItems( &docList, &count );
  require_noerr( status, CantGetCount );

  for( index = 1; index <= count; index++ ) {
    if ( (status = AEGetNthPtr( &docList, index, typeFSRef, NULL, NULL, &fsRef, sizeof(FSRef), NULL) ) == noErr ) {
      char revpath[PATH_MAX];
      OSErr error; 
      error = FSRefMakePath(&fsRef, revpath, sizeof(revpath));
      if (error !=noErr) continue;
      printf("%s\n", revpath); 
      xapi_open(revpath);
      break;
    }
  }
CantGetCount:
  return(status);
}

int ask_close() {
  DialogRef noticeDialog;
  DialogItemIndex itemIndex;
  AlertStdCFStringAlertParamRec inAlertParam;
  GetStandardAlertDefaultParams(&inAlertParam, kStdCFStringAlertVersionOne);
  inAlertParam.defaultButton = kAlertStdAlertCancelButton;
  inAlertParam.defaultText = CFSTR("Quit");
  inAlertParam.cancelText = CFSTR("Don't Quit");

  CreateStandardAlert(kAlertCautionAlert, CFSTR("Really Quit?"), CFSTR("Application terminaton is beeing blocked by remote control.\nPlease use the controlling application to quit Jadeo.\nQuit anyway?"), &inAlertParam, &noticeDialog);
  RunStandardAlert(noticeDialog, NULL, &itemIndex);
  if (itemIndex == 1) return 1;
  return 0;
}


static OSStatus NavOpenDocument(void) {
  printf("NavOpenDocument\n");
  OSStatus status;
  NavDialogCreationOptions options;
  NavReplyRecord navReply;
  static  NavObjectFilterUPP navFilterUPP = NULL;
  NavDialogRef navDialog = NULL;

  status = NavGetDefaultDialogCreationOptions( &options );
  require_noerr( status, CantGetDefaultOptions );

//if ( navFilterUPP == NULL ) navFilterUPP = NewNavObjectFilterUPP( NavOpenFilterProc );  

  status  = NavCreateChooseFileDialog( &options, NULL, NULL, NULL, navFilterUPP, NULL, &navDialog );
  require_noerr( status, CantCreateDialog );

  status  = NavDialogRun( navDialog );
  require_noerr( status, CantRunDialog );

  status  = NavDialogGetReply( navDialog, &navReply );
  require( (status == noErr) || (status == userCanceledErr), CantGetReply );

  if ( navReply.validRecord )
        status  = OpenDocuments( navReply.selection );
  else 
        status  = userCanceledErr;

  NavDisposeReply( &navReply );

CantGetReply:
CantRunDialog:
  if ( navDialog != NULL ) NavDialogDispose( navDialog );
CantCreateDialog:
CantGetDefaultOptions:
  return(status);
}

////////////////////////////////////////////////////////
// osx helpers
void window_resized_mac() {
  //printf("resized\n");
  
  int padding = 0;
  
  
  CGRect tmpBounds;

  GetPortBounds( GetWindowPort(theWindow), &winRect );

  if(want_letterbox) {
    float movie_aspect  =  (float)movie_width / (float)movie_height; // XXX todo pixel aspect ?
    float window_aspect = (float)((float)(winRect.right)/(float)winRect.bottom);
	
    if (window_aspect > movie_aspect ) {
      padding = (winRect.right - winRect.bottom*movie_aspect)/2;
      SetRect(&dstRect, padding, 0, winRect.bottom*movie_aspect+padding, winRect.bottom);
    } else {
      padding = (winRect.bottom - winRect.right/movie_aspect)/2;
      SetRect(&dstRect, 0, padding, winRect.right, winRect.right/movie_aspect+padding);
    }
  } else {
    SetRect(&dstRect, 0, 0, winRect.right, winRect.bottom);
  }

  switch (image_format) {
    case PIX_FMT_RGB24:
    case PIX_FMT_RGBA32:
    {
      bounds = CGRectMake(dstRect.left, dstRect.top, dstRect.right-dstRect.left, dstRect.bottom-dstRect.top);
      CreateCGContextForPort (GetWindowPort (theWindow), &context);
      break;
    }
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUYV422:
    case PIX_FMT_UYVY422:
    {
      long scale_X = FixDiv(Long2Fix(dstRect.right - dstRect.left),Long2Fix(imgRect.right));
      long scale_Y = FixDiv(Long2Fix(dstRect.bottom - dstRect.top),Long2Fix(imgRect.bottom));

      SetIdentityMatrix(&matrix);
      if (((dstRect.right - dstRect.left)   != imgRect.right) || ((dstRect.bottom - dstRect.right) != imgRect.bottom)) {
        ScaleMatrix(&matrix, scale_X, scale_Y, 0, 0);
        if (padding > 0) {
          TranslateMatrix(&matrix, Long2Fix(dstRect.left), Long2Fix(dstRect.top));
        }
      }
      SetDSequenceMatrix(seqId, &matrix);
      break;
    }
    default:
      break;
  }
  //Clear Background
  tmpBounds = CGRectMake( 0, 0, winRect.right, winRect.bottom);
  CreateCGContextForPort(GetWindowPort(theWindow),&context);
  CGContextFillRect(context, tmpBounds);
}

void window_ontop() {
  SetWindowGroupLevel(winGroup, CGWindowLevelForKey(levelList[winLevel]));
}

void window_fullscreen() {
  static Ptr restoreState = NULL;
  if(vo_fs) {
    if(winLevel != 0) {
      if(device_id == 0) {
        SetSystemUIMode(kUIModeAllHidden, kUIOptionAutoShowMenuBar);
       // CGDisplayHideCursor(kCGDirectMainDisplay);
       // mouseHide = TRUE;
      }
			
      if(fs_res_x != 0 || fs_res_y != 0) {
        BeginFullScreen( &restoreState, deviceHdl, &fs_res_x, &fs_res_y, NULL, NULL, 0);
        //Get Main device info///////////////////////////////////////////////////
        deviceRect = (*deviceHdl)->gdRect; 
        device_width = deviceRect.right;
        device_height = deviceRect.bottom;
      }
    } // end if winLevel !=0

    //save old window size
    if (!vo_mac_fs) {
      GetWindowPortBounds(theWindow, &oldWinRect);
      GetWindowBounds(theWindow, kWindowContentRgn, &oldWinBounds);
    }
		
    //go fullscreen
    int panscan_x = 0; // offset window->image
    int panscan_y = 0;
    if(want_letterbox) {
      float movie_aspect  =  (float)movie_width / (float)movie_height; // XXX todo pixel aspect ?
      float window_aspect = (float)((float)(device_width)/(float)device_height);
	
      if (window_aspect > movie_aspect)
        panscan_x = (device_width - device_height*movie_aspect);
      else 
        panscan_y = (device_height - device_width/movie_aspect);
    }
    ChangeWindowAttributes(theWindow, kWindowNoShadowAttribute, 0);
    MoveWindow(theWindow, deviceRect.left-(panscan_x >> 1), deviceRect.top-(panscan_y >> 1), 1);
    SizeWindow(theWindow, device_width+panscan_x, device_height+panscan_y,1);

    vo_mac_fs = 1;

  } else {
    //go back to windowed mode
    vo_mac_fs = 0;
    if(restoreState != NULL) {
      EndFullScreen(restoreState, 0);
      //Get Main device info///////////////////////////////////////////////////
      deviceRect = (*deviceHdl)->gdRect;

      device_width = deviceRect.right;
      device_height = deviceRect.bottom;
      restoreState = NULL;
    }

    SetSystemUIMode( kUIModeNormal, 0);
#if 0 //show mouse cursor
    CGDisplayShowCursor(kCGDirectMainDisplay);
    mouseHide = FALSE;
#endif
		
    //revert window to previous setting
    ChangeWindowAttributes(theWindow, 0, kWindowNoShadowAttribute);
    SizeWindow(theWindow, oldWinRect.right, oldWinRect.bottom,1);
    MoveWindow(theWindow, oldWinBounds.left, oldWinBounds.top, 1);
  }
  window_resized_mac();
}

//default keyboard event handler
static OSStatus KeyEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
  printf ("key-event\n");
  OSStatus result = noErr;
  UInt32 class = GetEventClass (event);
  UInt32 kind = GetEventKind (event); 
  result = CallNextEventHandler(nextHandler, event);
  
  if(class == kEventClassKeyboard) {
    char macCharCodes;
    UInt32 macKeyCode;
    UInt32 macKeyModifiers;

    GetEventParameter(event, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(macCharCodes), NULL, &macCharCodes);
    GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(macKeyCode), NULL, &macKeyCode);
    GetEventParameter(event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(macKeyModifiers), NULL, &macKeyModifiers);
    if(macKeyModifiers != 256)
    {
#if 1
      if (kind == kEventRawKeyRepeat || kind == kEventRawKeyDown) {
        mac_put_key(macKeyCode, macCharCodes);
      }
#endif
    }
    else if(macKeyModifiers == 256)
    {
#if 0    
      switch(macCharCodes) {
        case '[': SetWindowAlpha(theWindow, winAlpha-=0.05); break;
        case ']': SetWindowAlpha(theWindow, winAlpha+=0.05); break;		
      }	
#endif
    }
    else
    result = eventNotHandledErr;
  }
  return result;
}

//default mouse event handler
static OSStatus MouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
  //printf ("mouse-event\n");
  OSStatus result = noErr;
  UInt32 class = GetEventClass (event);
  UInt32 kind = GetEventKind (event); 
  result = CallNextEventHandler(nextHandler, event);
  
  if(class == kEventClassMouse) {
    WindowPtr tmpWin;
    Point mousePos;
    Point winMousePos;

    GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, 0, sizeof(Point), 0, &mousePos);
    GetEventParameter(event, kEventParamWindowMouseLocation, typeQDPoint, 0, sizeof(Point), 0, &winMousePos);
    switch (kind) {
      case kEventMouseMoved:
      {
#if 0 // auto - show mouse whem moving
        if(vo_mac_fs) {
          CGDisplayShowCursor(kCGDirectMainDisplay);
          mouseHide = FALSE;
        }
#endif
      }
      break;
      case kEventMouseWheelMoved:
      {
#if 0
        int wheel;
        short part;
        GetEventParameter(event, kEventParamMouseWheelDelta, typeSInt32, 0, sizeof(int), 0, &wheel);
        part = FindWindow(mousePos,&tmpWin);
        if(part == inContent) {
          if(wheel > 0) mac_put_key(0, '>');
          else mac_put_key(0, '<');
        }
#endif
      }
      break;
      case kEventMouseDown:
      case kEventMouseUp:
      {
        EventMouseButton button;
        short part;
        Rect bounds; // XXX bounds is also a static/global
        
        GetWindowPortBounds(theWindow, &bounds);
        GetEventParameter(event, kEventParamMouseButton, typeMouseButton, 0, sizeof(EventMouseButton), 0, &button);
        
        part = FindWindow(mousePos,&tmpWin);
        if(kind == kEventMouseUp)
        {
#if 0
          if (part != inContent) break;
          switch(button) { 
            case kEventMouseButtonPrimary:
              mac_put_key(0, '.');
              break;
            case kEventMouseButtonSecondary:
              mac_put_key(0, ',');
              break;
            case kEventMouseButtonTertiary:
              ;
              break;
            default:result = eventNotHandledErr;break;
          }
#endif
          break;
        }
        if( (winMousePos.h > (bounds.right - 15)) && (winMousePos.v > (bounds.bottom)) ) {
          if(!vo_mac_fs)
                  GrowWindow(theWindow, mousePos, NULL);
        }
        else if(part == inMenuBar) {
          MenuSelect(mousePos);
          HiliteMenu(0);
        }
        else if(part == inContent) {
#if 0
          switch(button) { 
            case kEventMouseButtonPrimary:
              break;
            case kEventMouseButtonSecondary:
              break;
            case kEventMouseButtonTertiary:
              break;
            default:
              result = eventNotHandledErr;
              break;
          }
#else
          result = eventNotHandledErr;
#endif
        }
      }		
      break;

      case kEventMouseDragged:
      break;
	      
      default:result = eventNotHandledErr;break;
    }
  }

  return result;
}

//default window event handler
static OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
  //printf ("window-event\n");
  OSStatus result = noErr;
//uint32_t d_width;
//uint32_t d_height;
  UInt32 class = GetEventClass (event);
  UInt32 kind = GetEventKind (event); 

  if(class == kEventClassCommand) {
    //printf("kEventClassCommand\n");
    HICommand theHICommand;
    result = CallNextEventHandler(nextHandler, event);
    GetEventParameter(event, kEventParamDirectObject, typeHICommand, NULL, sizeof( HICommand ), NULL, &theHICommand);
    result = mac_menu_cmd(result, &theHICommand);

    // or slider 
    // if (kind == kEventCommandProcess) {;}
    
  } else if(class == kEventClassWindow) {
    //printf("kEventClassWindow\n");
    WindowRef     window;
    Rect          rectPort = {0,0,0,0};
    
    GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &window);

    if(window) {
            GetPortBounds(GetWindowPort(window), &rectPort);
    }   
    switch (kind) {
      case kEventWindowClose:
        if ((interaction_override&0x2) == 0 || ask_close()) {
          result = CallNextEventHandler(nextHandler, event);
        } 
        break;
      case kEventWindowClosed:
        result = CallNextEventHandler(nextHandler, event);
        // TODO: catch window close
        theWindow = NULL;
        loop_flag=0;
        //exit(0); // XXX
        //ExitToShell();
        break;
      //resize window
      case kEventWindowZoomed:
      case kEventWindowBoundsChanged:
        result = CallNextEventHandler(nextHandler, event);
        window_resized_mac();
        flip_page();
        window_resized_mac();
        break;
      default:
        result = CallNextEventHandler(nextHandler, event);
        //result = eventNotHandledErr;
        break;
    }
  } 
  return result;
}

static void flip_page(void) {
  int curTime;
  static int lastTime = 0;

  if(theWindow == NULL) return;
//CGContextFlush (context); // XXX not needed 


  switch (image_format) {
    case PIX_FMT_RGB24:
    case PIX_FMT_RGBA32:
    {
      CGContextDrawImage (context, bounds, image);
    }
    break;
	    
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUYV422:
    case PIX_FMT_UYVY422:
    {
      OSErr qterr;
      CodecFlags flags = 0;
      qterr = DecompressSequenceFrameWhen(seqId, (char *)yuvbuf,
          image_buffer_size,
          0, //codecFlagUseImageBuffer,
          &flags, NULL, NULL);
      if (qterr) {
        fprintf(stderr, "mac error: DecompressSequenceFrameWhen in flip_page (%d) flags:0x%08x\n", qterr, flags);
      }
    }
    break;
  }
#if 1
  if(!vo_mac_fs) {
    //render resize box
    CGContextBeginPath(context);
    CGContextSetAllowsAntialiasing(context, false);
    //CGContextSaveGState(context);

    //line white
    CGContextSetRGBStrokeColor (context, 0.2, 0.2, 0.2, 0.5);
    CGContextMoveToPoint( context, winRect.right-1, 1); CGContextAddLineToPoint( context, winRect.right-1, 1);	
    CGContextMoveToPoint( context, winRect.right-1, 5); CGContextAddLineToPoint( context, winRect.right-5, 1);
    CGContextMoveToPoint( context, winRect.right-1, 9); CGContextAddLineToPoint( context, winRect.right-9, 1);
    CGContextStrokePath( context );

    //line gray
    CGContextSetRGBStrokeColor (context, 0.4, 0.4, 0.4, 0.5);
    CGContextMoveToPoint( context, winRect.right-1, 2); CGContextAddLineToPoint( context, winRect.right-2, 1);	
    CGContextMoveToPoint( context, winRect.right-1, 6); CGContextAddLineToPoint( context, winRect.right-6, 1);
    CGContextMoveToPoint( context, winRect.right-1, 10); CGContextAddLineToPoint( context, winRect.right-10, 1);
    CGContextStrokePath( context );

    //line black
    CGContextSetRGBStrokeColor (context, 0.6, 0.6, 0.6, 0.5);
    CGContextMoveToPoint( context, winRect.right-1, 3); CGContextAddLineToPoint( context, winRect.right-3, 1);	
    CGContextMoveToPoint( context, winRect.right-1, 7); CGContextAddLineToPoint( context, winRect.right-7, 1);
    CGContextMoveToPoint( context, winRect.right-1, 11); CGContextAddLineToPoint( context, winRect.right-11, 1);
    CGContextStrokePath( context );
    
    //CGContextRestoreGState( context );
    CGContextFlush (context);
}
#endif

#if 0 //auto hide mouse cursor and futur on-screen control?
	if(vo_mac_fs && !mouseHide) {
		int curTime = TickCount()/60;
		static int lastTime = 0;
		
		if( ((curTime - lastTime) >= 5) || (lastTime == 0) )
		{
			CGDisplayHideCursor(kCGDirectMainDisplay);
			mouseHide = TRUE;
			lastTime = curTime;
		}
	}
#endif
#if 1
  //update activity every 30 seconds to prevent
  //screensaver from starting up.
  curTime  = TickCount()/60;
  if( ((curTime - lastTime) >= 30) || (lastTime == 0) ) {
    UpdateSystemActivity(UsrActivity);
    lastTime = curTime;
  }
#endif
}

static int draw_frame(uint8_t *src) {
  //printf("draw_frame\n");
  switch (image_format) {
    case PIX_FMT_RGB24:
    case PIX_FMT_RGBA32:
      memcpy(image_data,src,image_size);
      return 0;
            
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUYV422:
    case PIX_FMT_UYVY422:
#ifdef CROPIMG
{
      stride_memcpy(yuvbuf, src+(xoffset*2),
          movie_width*2,movie_height, movie_width*2, movie_width*4);
}
#else 
      memcpy_pic(((char*)yuvbuf), src, 
      imgRect.right * 2, imgRect.bottom, 
      imgRect.right * 2, imgRect.right * 2);
#endif
      return 0;
  }
  return -1;
}

/*******************************************************************************
 * xjadeo API
 */

int open_window_mac (void) { 
  uint32_t width   = movie_width;
  uint32_t height  = movie_height; 
  uint32_t d_width = movie_width;
  uint32_t d_height= movie_height;

  device_id = 0;
  image_format = PIX_FMT_YUV420P;
  //image_format = PIX_FMT_RGBA32;

  WindowAttributes windowAttrs;
  OSErr qterr = 0;
  int i;
  CGRect tmpBounds;

  deviceHdl = GetMainDevice();
  for(i=0; i<device_id; i++) {
    deviceHdl = GetNextDevice(deviceHdl);
    if(deviceHdl == NULL) {
      fprintf(stderr, "mac error: Device ID %d do not exist, falling back to main device.\n", device_id);
      deviceHdl = GetMainDevice();
      device_id = 0;
      break;
    }
  }
  
  deviceRect = (*deviceHdl)->gdRect;
  device_width = deviceRect.right-deviceRect.left;
  device_height = deviceRect.bottom-deviceRect.top;
  
  //misc setup/////////////////////////////////////////////////////
  SetRect(&imgRect, 0, 0, width, height);

  switch (image_format) 
  {
    case PIX_FMT_RGBA32:
      image_depth = 32;
      break;
    case PIX_FMT_RGB24:
      image_depth = 24;
      break;
  case PIX_FMT_YUV420P:
  case PIX_FMT_YUYV422:
  case PIX_FMT_UYVY422:
      image_depth = 16;
      break;
  }
  image_size = ((imgRect.right*imgRect.bottom*image_depth)+7)/8;
  vo_fs = start_fullscreen;
  
  if(image_data) free(image_data);
  image_data = malloc(image_size);

  //Create player window//////////////////////////////////////////////////
  windowAttrs =   kWindowStandardDocumentAttributes
                | kWindowStandardHandlerAttribute
                | kWindowInWindowMenuAttribute
                | kWindowAsyncDragAttribute
                | kWindowLiveResizeAttribute;
  windowAttrs &= (~kWindowResizableAttribute);
				  
  if (theWindow == NULL) {
    mac_CreateWindow(d_width, d_height, windowAttrs);
    if (theWindow == NULL) {
      fprintf(stderr, "mac error: Couldn't create window !!!!!\n");
      return -1;
    }

#if 1
    tmpBounds = CGRectMake( 0, 0, winRect.right, winRect.bottom);
    CreateCGContextForPort(GetWindowPort(theWindow),&context);
    CGContextFillRect(context, tmpBounds);
#else
    SetWindowGroupLevel(winGroup, kCGDesktopIconWindowLevel - 1);
    SetWindowGroup(theWindow, winGroup);
    RGBColor black = { 0, 0, 0 };
    SetWindowContentColor(theWindow, &black);
#endif
  } else {
    //printf("re-set window\n");
#if 1
    HideWindow(theWindow);
    ChangeWindowAttributes(theWindow, ~windowAttrs, windowAttrs);
    SetRect(&winRect, 0, 0, d_width, d_height);
    SetRect(&oldWinRect, 0, 0, d_width, d_height);
    SizeWindow (theWindow, d_width, d_height, 1);
#endif
  }
  
  switch (image_format) {
    case PIX_FMT_RGB24:
    case PIX_FMT_RGBA32:
    {
      CreateCGContextForPort (GetWindowPort (theWindow), &context);
      
      dataProviderRef = CGDataProviderCreateWithData (0, image_data, imgRect.right * imgRect.bottom * 4, 0);
      
      image = CGImageCreate   (imgRect.right,
                               imgRect.bottom,
                               8,
                               image_depth,
                               ((imgRect.right*32)+7)/8,
                               CGColorSpaceCreateDeviceRGB(),
                               kCGImageAlphaNoneSkipFirst,
                               dataProviderRef, 0, 1, kCGRenderingIntentDefault);
        break;
    }
    case PIX_FMT_YUYV422:
    case PIX_FMT_UYVY422:
    case PIX_FMT_YUV420P:
    {
      SetIdentityMatrix(&matrix);
      if ((d_width != width) || (d_height != height)) {
        ScaleMatrix(&matrix, FixDiv(Long2Fix(d_width),Long2Fix(width)), FixDiv(Long2Fix(d_height),Long2Fix(height)), 0, 0);
      }		

      yuv_qt_stuff.desc = (ImageDescriptionHandle)NewHandleClear( sizeof(ImageDescription) );

      yuv_qt_stuff.extension_colr = NewHandleClear(sizeof(NCLCColorInfoImageDescriptionExtension));
      ((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->colorParamType = kVideoColorInfoImageDescriptionExtensionType;
      ((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->primaries = 2;
      ((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->transferFunction = 2;
      ((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->matrix = 2;

      yuv_qt_stuff.extension_fiel = NewHandleClear(sizeof(FieldInfoImageDescriptionExtension));
      ((FieldInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_fiel))->fieldCount = 1;
      ((FieldInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_fiel))->fieldOrderings = 0;

      yuv_qt_stuff.extension_clap = NewHandleClear(sizeof(CleanApertureImageDescriptionExtension));
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureWidthN = imgRect.right;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureWidthD = 1;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureHeightN = imgRect.bottom;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureHeightD = 1;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->horizOffN = 0;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->horizOffD = 1;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->vertOffN = 0;
      ((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->vertOffD = 1;

      yuv_qt_stuff.extension_pasp = NewHandleClear(sizeof(PixelAspectRatioImageDescriptionExtension));
      ((PixelAspectRatioImageDescriptionExtension*)(*yuv_qt_stuff.extension_pasp))->hSpacing = 1;
      ((PixelAspectRatioImageDescriptionExtension*)(*yuv_qt_stuff.extension_pasp))->vSpacing = 1;

      (*yuv_qt_stuff.desc)->idSize = sizeof(ImageDescription);
      (*yuv_qt_stuff.desc)->cType = k422YpCbCr8CodecType;
      (*yuv_qt_stuff.desc)->version = 2;
      (*yuv_qt_stuff.desc)->revisionLevel = 0;
      (*yuv_qt_stuff.desc)->vendor = 'xj-5';
      (*yuv_qt_stuff.desc)->width = imgRect.right;
      (*yuv_qt_stuff.desc)->height = imgRect.bottom;
      (*yuv_qt_stuff.desc)->hRes = Long2Fix(72);
      (*yuv_qt_stuff.desc)->vRes = Long2Fix(72);
      (*yuv_qt_stuff.desc)->temporalQuality = 0;
      (*yuv_qt_stuff.desc)->spatialQuality = codecLosslessQuality;
      (*yuv_qt_stuff.desc)->frameCount = 1;
      (*yuv_qt_stuff.desc)->dataSize = 0;
      (*yuv_qt_stuff.desc)->depth = 24; //
      (*yuv_qt_stuff.desc)->clutID = -1;

      qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_colr, kColorInfoImageDescriptionExtension);
      if (qterr) {
        fprintf(stderr, "mac error: AddImageDescriptionExtension [colr] (%d)\n", qterr);
      }
		  
      qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_fiel, kFieldInfoImageDescriptionExtension);
      if (qterr)
      {
              fprintf(stderr, "mac error: AddImageDescriptionExtension [fiel] (%d)\n", qterr);
      }

      qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_clap, kCleanApertureImageDescriptionExtension);
      if (qterr)
      {
              fprintf(stderr, "mac error: AddImageDescriptionExtension [clap] (%d)\n", qterr);
      }

      qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_pasp, kCleanApertureImageDescriptionExtension);
      if (qterr)
      {
              fprintf(stderr, "mac error: AddImageDescriptionExtension [pasp] (%d)\n", qterr);
      }

      if (P != NULL) { // second or subsequent movie
        free(P);
      }
      P = calloc(1, sizeof(PlanarPixmapInfoYUV420));
      if (yuvbuf != NULL) { // second or subsequent movie
        free(yuvbuf);
      }
      yuvbuf = calloc(1, image_size);
#if 0
      switch (image_format) {
        default:
      /*
        case IMGFMT_YV12:
        case IMGFMT_IYUV:
        case IMGFMT_I420:
      */
        case PIX_FMT_YUV420P: //XXX
          P->componentInfoY.offset  = be2me_32(sizeof(PlanarPixmapInfoYUV420));
          P->componentInfoCb.offset = be2me_32(be2me_32(P->componentInfoY.offset) + image_size / 2);
          P->componentInfoCr.offset = be2me_32(be2me_32(P->componentInfoCb.offset) + image_size / 4);
          P->componentInfoY.rowBytes  = be2me_32(imgRect.right);
          P->componentInfoCb.rowBytes =  be2me_32(imgRect.right / 2);
          P->componentInfoCr.rowBytes =  be2me_32(imgRect.right / 2);
          image_buffer_size = image_size + sizeof(PlanarPixmapInfoYUV420);
          break;
        /*
        case IMGFMT_UYVY:
        case IMGFMT_YUY2:
        */
        case PIX_FMT_YUYV422 : //XXX
        case PIX_FMT_UYVY422:
          image_buffer_size = image_size;
          break;
      }
#else
      image_buffer_size = image_size;
#endif
      qterr = DecompressSequenceBeginS(
                &seqId,
                yuv_qt_stuff.desc,
                (char *) yuvbuf, // XXX use moviebuffer here?
                image_buffer_size,
                GetWindowPort(theWindow),
                NULL,
                NULL,
                ((d_width != width) || (d_height != height)) ? 
                &matrix : NULL,
                srcCopy,
                NULL,
                0,
                codecMinQuality, // codecLosslessQuality,
                bestSpeedCodec);
      if (qterr) {
        fprintf(stderr, "mac error: DecompressSequenceBeginS (%d)\n", qterr);
        return -1;
      }
      // Turn off gamma correction (TODO: unless requested)
      if (1)
          QTSetPixMapHandleRequestedGammaLevel(GetPortPixMap(GetWindowPort(theWindow)), kQTUseSourceGammaLevel);
      break;
    } // end case YUV

    default:
        fprintf(stderr, "unsupported Image format (%d).\n", qterr);
        return -1;
  }

  //Show window
  RepositionWindow(theWindow, NULL, kWindowCenterOnMainScreen);
  ShowWindow (theWindow);
//SelectWindow (theWindow);
  SetWindowGroupLevel(winGroup, CGWindowLevelForKey(levelList[winLevel]));

  if(vo_fs)
    window_fullscreen();

  if(start_ontop) {
    winLevel=2;
    window_ontop();
  }
#if 0
  if(vo_rootwin) {
    vo_fs = 1;
    winLevel = 0;
    SetWindowGroupLevel(winGroup, CGWindowLevelForKey(levelList[winLevel]));
    window_fullscreen();
  }
#endif
  window_resized_mac();
  return 0;
}

void close_window_mac (void) {
  printf("close window\n");
  if (1) { // XX ONLY YUV ...
    OSErr qterr;
    qterr = CDSequenceEnd(seqId);
    if (qterr) {;}
  }
  ShowMenuBar();
}

void newsrc_mac (void) {
  close_window_mac();
  open_window_mac();
}

void resize_mac (unsigned int x, unsigned int y) {
  //printf("resize %i %i\n",x,y);
  if(vo_mac_fs) {
          vo_fs = (!(vo_fs)); window_fullscreen();
  }
  SizeWindow(theWindow, x, y ,1);
  window_resized_mac();
}

void getsize_mac (unsigned int *x, unsigned int *y) { 
  static Rect rect; 
  GetWindowPortBounds(theWindow, &rect);
  //GetPortBounds( GetWindowPort(theWindow), &rect );
  //GetWindowBounds(theWindow, kWindowContentRgn, &rect);
  if(x)*x=rect.right; if(y)*y=rect.bottom; 
  printf("getsize %i %i\n", *x, *y);
}

void position_mac (int x, int y) {
  printf("set position %i %i\n",x,y);
  MoveWindow(theWindow, x, y, 1);
}

void getpos_mac (int *x, int *y) { 
  static Rect rect; 
  //GetWindowPortBounds(theWindow, &rect); // OSX < 10.4/5 ?!
  GetWindowBounds(theWindow, kWindowContentRgn, &rect); // OSX 10.6
  if(x)*x=rect.left; if(y)*y=rect.top; 
  printf("get window pos %i %i\n", *x, *y);
}

void fullscreen_mac (int a) {
  vo_fs=a;
  window_fullscreen();
}

int get_fullscreen_mac() {
  return (vo_fs);
}

void mousepointer_mac (int a) {
  if (a>1) a=!mouseHide;
  printf("mouse cursor: %s\n",a?"hide":"show");
  if (a) {
    CGDisplayHideCursor(kCGDirectMainDisplay);
    mouseHide = 1;
  } else {
    CGDisplayShowCursor(kCGDirectMainDisplay);
    mouseHide = 0;
  }
}

void ontop_mac (int a) {
  //printf("ontop %i\n",a?1:0);
  if (a) winLevel=2; else winLevel=1;
  window_ontop();
}

int get_ontop_mac() {
  return(winLevel==2?1:0);
}

void render_mac (uint8_t *mybuffer) {
//printf("render\n");
//imgRect.right= movie_width;
//imgRect.bottom= movie_height;

#if 1
  draw_frame(mybuffer);
#elif 0 // testing
  size_t Ylen= movie_width * movie_height;
  size_t UVlen= movie_width/2 * movie_height/2; 
  memcpy(yuvbuf,mybuffer,Ylen+UVlen+UVlen); 
#else /* http://www.fourcc.org/indexyuv.htm */
  uint8_t *Yptr=mybuffer;
  uint8_t *Uptr=Yptr + Ylen;
  uint8_t *Vptr=Uptr + UVlen;
  stride_memcpy(mac_overlay->pixels[0],Yptr,movie_width,movie_height,mac_overlay->pitches[0],movie_width);//Y
  stride_memcpy(mac_overlay->pixels[1],Vptr,movie_width/2,movie_height/2,mac_overlay->pitches[1],movie_width/2);//V
  stride_memcpy(mac_overlay->pixels[2],Uptr,movie_width/2,movie_height/2,mac_overlay->pitches[2],movie_width/2);//U
#endif

  flip_page();
//printf("render done.\n");
}


void handle_X_events_mac (void) {
  EventRef theEvent;
  EventTargetRef theTarget;
  OSStatus        theErr;

  theTarget = GetEventDispatcherTarget();
  theErr = ReceiveNextEvent(0, 0, kEventDurationNoWait,true, &theEvent);
  if(theErr == noErr && theEvent != NULL) {
	SendEventToEventTarget (theEvent, theTarget);
	ReleaseEvent(theEvent);
  }
}

void mac_put_key(UInt32 key, UInt32 charcode) {
  char c;
  switch (key) {
    case 0x35: if ((interaction_override&0x1) == 0) loop_flag=0; return;  // ESCAPE
    default: c= (char) charcode;
  }
  switch (c) {
    case 'q': if ((interaction_override&0x1) == 0) loop_flag=0; return; 
    case 'a': ontop_mac(winLevel==2?0:1); break;
    case 'f': fullscreen_mac(!vo_fs); break;
    case 'l':  
      want_letterbox = !want_letterbox;
      force_redraw=1;
      window_resized_mac();
      break;
    case 'o': { //'o' // OSD - offset in frames
      if (OSD_mode&OSD_OFFF) {
              OSD_mode&=~OSD_OFFF;
              OSD_mode|=OSD_OFFS;
      } else if (OSD_mode&OSD_OFFS) {
              OSD_mode^=OSD_OFFS;
      } else {
              OSD_mode^=OSD_OFFF;
      }
      force_redraw=1;
    } break;
    case 's': { 
      OSD_mode^=OSD_SMPTE;
      force_redraw=1;
    } break;
    case 'v': { //'v' // OSD - current video frame
      OSD_mode^=OSD_FRAME; 
      force_redraw=1;
    } break;
    case 'b': { //'b' // OSD - black box
      OSD_mode^=OSD_BOX;
      force_redraw=1;
    } break;
    case 'C': { //'C' // OSD - clear all
      OSD_mode=0; 
      force_redraw=1;
    } break;
    case '+': {
      if ((interaction_override&0x10) != 0 ) break;
      ts_offset++;
      force_redraw=1;
      if (smpte_offset) free(smpte_offset);
      smpte_offset= calloc(15,sizeof(char));
      frame_to_smptestring(smpte_offset,ts_offset);
    } break;
    case '-': {
      if ((interaction_override&0x10) != 0 ) break;
      ts_offset--;
      force_redraw=1;
      if (smpte_offset) free(smpte_offset);
      smpte_offset= calloc(15,sizeof(char));
      frame_to_smptestring(smpte_offset,ts_offset);
    } break;
    case '{': {
      if ((interaction_override&0x10) != 0 ) break;
      if (framerate > 0) {
        ts_offset-= framerate *60;
      } else {
        ts_offset-= 25*60;
      }
      force_redraw=1;
      if (smpte_offset) free(smpte_offset);
      smpte_offset= calloc(15,sizeof(char));
      frame_to_smptestring(smpte_offset,ts_offset);
    } break;
    case '}': {
      if ((interaction_override&0x10) != 0 ) break;
      if (framerate > 0) {
        ts_offset+= framerate *60;
      } else {
        ts_offset+= 25*60;
      }
      force_redraw=1;
      if (smpte_offset) free(smpte_offset);
      smpte_offset= calloc(15,sizeof(char));
      frame_to_smptestring(smpte_offset,ts_offset);
    } break;
    case 'm': mousepointer_mac(2); break;
    case '.': { //'.' // resize 100%
      resize_mac(movie_width, movie_height);
    } break;
    case ',': { //',' // resize to aspect ratio
      unsigned int my_Width,my_Height;
      getsize_mac(&my_Width,&my_Height);
      if( ((float)movie_width/(float)movie_height) < ((float)my_Width/(float)my_Height) )
        my_Width=floor((float)my_Height * (float)movie_width / (float)movie_height);
      else
        my_Height=floor((float)my_Width * (float)movie_height / (float)movie_width);
      resize_mac(my_Width, my_Height);
    } break;
    case '<': { //'<' // resize *.83
      unsigned int my_Width,my_Height;
      getsize_mac(&my_Width,&my_Height);
      float step=0.2*my_Height;
      my_Width-=floor(step*((float)movie_width/(float)movie_height));
      my_Height-=step;
      resize_mac(my_Width, my_Height);
    } break;
    case '>': { //'>' // resize *1.2
      unsigned int my_Width,my_Height;
      getsize_mac(&my_Width,&my_Height);
      float step=0.2*my_Height;
      my_Width+=floor(step*((float)movie_width/(float)movie_height));
      my_Height+=step;
      resize_mac(my_Width, my_Height);
    } break;
#ifdef CROPIMG
    case '[': { 
      xoffset-=2;
      if (xoffset<0) xoffset=0;
      force_redraw=1;
    } break;
    case ']': { 
      xoffset+=2;
      if (xoffset>movie_width) xoffset=movie_width;
      force_redraw=1;
    } break;
#endif
    case 0x8: jackt_rewind(); break;
    case ' ': jackt_toggle(); break;
    default: 
      printf("yet unhandled keyboard event: '%c' 0x%x\n",c,c);
      break;
  }
  checkMyMenu();
}

OSStatus mac_menu_cmd(OSStatus result, HICommand *acmd) {
  switch ( acmd->commandID ) {
    case kHICommandQuit:
    case mQuit:
      if ((interaction_override&0x4) == 0 || ask_close())
        loop_flag=0;
      break;
    case kHICommandAbout:
      break;
    case kHICommandClose:
      break;
    case kHICommandOpen:
    case mOpen:
      NavOpenDocument();
      break;
    case kHICommandPreferences:
    case mSettings:
      NavOpenSettings();
      break;
    case mHalfScreen:
      resize_mac(movie_width/2, movie_height/2);
      break;
    case mNormalScreen:
      resize_mac(movie_width, movie_height);
    //SizeWindow(theWindow, d_width, (d_width/movie_aspect), 1);
      break;
  //case kHICommandFullScreen:
    case mFullScreen:
      vo_fs = (!(vo_fs)); window_fullscreen();
      break;
    case mDoubleScreen:
      resize_mac(movie_width*2, movie_height*2);
      break;
    case mKeepAspect:
      want_letterbox = !want_letterbox;
      force_redraw=1;
      window_resized_mac();
      break;
    case mOSDFrame:
      OSD_mode^=OSD_FRAME; 
      force_redraw=1;
      break;
    case mOSDSmpte:
      OSD_mode^=OSD_SMPTE;
      force_redraw=1;
      break;
    case mOSDOffO:
      OSD_mode&=~OSD_OFFF;
      OSD_mode&=~OSD_OFFS;
      force_redraw=1;
      break;
    case mOSDOffS:
      OSD_mode&=~OSD_OFFF;
      OSD_mode|=OSD_OFFS;
      force_redraw=1;
      break;
    case mOSDOffF:
      OSD_mode&=~OSD_OFFS;
      OSD_mode|=OSD_OFFF;
      force_redraw=1;
      break;
    case mOSDBox:
      OSD_mode^=OSD_BOX;
      force_redraw=1;
    break;
    case mSeekAny:
      seekflags=SEEK_ANY;
      force_redraw=1;
      break;
    case mSeekKeyFrame:
      seekflags=SEEK_KEY;
      force_redraw=1;
      break;
    case mSeekContinuous:
      seekflags=SEEK_CONTINUOUS;
      force_redraw=1;
      break;
    case mJackPlay:
      jackt_start();
      break;
    case mJackRewind:
      jackt_rewind();
      break;
    case mJackStop:
      jackt_stop();
      break;
    case mSyncJack:
	open_jack();
#ifdef HAVE_MIDI
	if (midi_connected()) midi_close();
#endif
      break;
    case mSyncJackMidi:
    case mSyncPortMidi: {
	if (jack_connected()) close_jack();
#ifdef HAVE_MIDI
	if (midi_connected()) midi_close();
        if (acmd->commandID == mSyncPortMidi) {
          midi_choose_driver("portmidi");
        } else {
          midi_choose_driver("jack");
        }
        char *mp = "-1";
	midi_open(mp);
#endif
	if (!midi_connected()) {
          printf("MIDI CONNECT FAILED!\n");
          DialogRef alertDialog;
          CreateStandardAlert(kAlertNoteAlert, CFSTR("Midi connect failed!"), CFSTR("could not connect to MIDI Port. check the settings."),NULL, &alertDialog);
          RunStandardAlert(alertDialog, NULL, NULL);
        } 
      }
      break;
    case mSyncNone:
	if (jack_connected()) close_jack();
#ifdef HAVE_MIDI
	if (midi_connected()) midi_close();
#endif
      break;
#if 1
    case kHICommandNew:
    case kHICommandSave:
    case kHICommandSaveAs:
    case kHICommandUndo:
    case kHICommandCut:
    case kHICommandCopy:
    case kHICommandPaste:
    case kHICommandClear:
    case kHICommandSelectAll:
      DisableMenuItem(acmd->menu.menuRef, acmd->menu.menuItemIndex);
      break;
#endif
    default:
      printf("yet unhandled menu entry id:%u\n",(unsigned int)acmd->commandID);
      result = eventNotHandledErr;
      break;
  }
  checkMyMenu(); // update checked menu items
  return result;
}

#endif /* HAVE_MAC */

// vim: sw=2 sts=2 ts=8 et
