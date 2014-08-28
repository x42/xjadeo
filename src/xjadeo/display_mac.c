/* xjadeo - openGL display for OSX
 *
 * (C) 2008,2014 Robin Gareus <robin@gareus.org>
 *
 * this code was inspired by mplayer's libvo/vo_quartz.c
 * (c) 2004 Nicolas Plourde <nicolasplourde@gmail.com>
 * (C) 2004 Romain Dolbeau <romain@dolbeau.org>
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
 */

#include "xjadeo.h"

#if defined PLATFORM_OSX && (defined __i386 || defined __ppc__)

void xapi_open(void *d);

extern int loop_flag;
extern int OSD_mode;
extern int force_redraw; // tell the main event loop that some cfg has changed
extern int want_letterbox;
extern int hide_mouse;
extern int interaction_override; // disable some options.

extern int movie_width;
extern int movie_height;
extern int ffctv_width;
extern int ffctv_height;
extern float movie_aspect;
extern double delay;
extern double framerate;
extern int start_fullscreen;
extern int start_ontop;

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

#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>

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
static OSStatus DialogEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
void mac_put_key(UInt32 key, UInt32 charcode);
OSStatus mac_menu_cmd(OSStatus result, HICommand *acmd);

///
static int vo_fs = 0; // enter fullscreen - user setting
static int vo_mac_fs = 0; // we are in fullscreen

///
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
        mOSDOffO,
        mOSDOffS,
        mOSDOffF,
        mSyncJack,
        mSyncLTC,
        mSyncJackMidi,
        mSyncPortMidi,
        mSyncNone,
        mJackPlay,
        mJackStop,
        mJackRewind
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
  CheckMenuItem (osdMenu, 1, (OSD_mode&OSD_FRAME)!=0);
  CheckMenuItem (osdMenu, 2, (OSD_mode&OSD_SMPTE)!=0);
  CheckMenuItem (osdMenu, 5, (OSD_mode&OSD_BOX)!=0);

  CheckMenuItem (osdoMenu, 1, (OSD_mode&(OSD_OFFS|OSD_OFFF))==0);
  CheckMenuItem (osdoMenu, 2, (OSD_mode&OSD_OFFS)!=0);
  CheckMenuItem (osdoMenu, 3, (OSD_mode&OSD_OFFF)!=0);

  CheckMenuItem (zoomMenu, 6, want_letterbox);

  CheckMenuItem (syncMenu, 1, ui_syncsource() == SYNC_JACK);
  CheckMenuItem (syncMenu, 4, ui_syncsource() == SYNC_LTC);
  CheckMenuItem (syncMenu, 5, ui_syncsource() == SYNC_MTC_PORTMIDI);
  CheckMenuItem (syncMenu, 6, ui_syncsource() == SYNC_MTC_JACK);
  CheckMenuItem (syncMenu, 7, ui_syncsource() == SYNC_NONE);
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
  //AppendMenuItemTextWithCFString(movMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  //AppendMenuItemTextWithCFString(movMenu, CFSTR("Seek"), 0, 0, &index);

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
  //Create Screen Menu
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
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("JACK Transport"), 0, 0, &index);
  AppendMenuItemTextWithCFString(syncMenu, NULL, kMenuItemAttrSeparator, 0, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("LTC (jack)"), 0, mSyncLTC, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("MTC (portmidi)"), 0, mSyncPortMidi, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("MTC (jackmidi)"), 0, mSyncJackMidi, &index);
  AppendMenuItemTextWithCFString(syncMenu, CFSTR("none"), 0, mSyncNone, &index);

  //Create Jack Menu
  CreateNewMenu (0, 0, &jackMenu);
  jackMenuTitle = CFSTR("JACK Transport");
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

DialogRef midiAlertDialog = 0;
static OSStatus midiAlert(void) {
  OSStatus status = 0;
  const EventTypeSpec win_events[] = {
    { kEventClassWindow, kEventWindowClosed },
    { kEventClassCommand, kEventCommandProcess }
  };
  CreateStandardAlert(kAlertNoteAlert, CFSTR("Midi connect failed!"), CFSTR("could not connect to MIDI Port. check the settings."), NULL, &midiAlertDialog);
  //RunStandardAlert(midiAlertDialog, NULL, NULL);
  ShowWindow (GetDialogWindow(midiAlertDialog));
  InstallWindowEventHandler (GetDialogWindow(midiAlertDialog), NewEventHandlerUPP (DialogEventHandler), GetEventTypeCount(win_events), win_events, GetDialogWindow(midiAlertDialog), NULL);
  RunAppModalLoopForWindow (GetDialogWindow(midiAlertDialog));
  return(status);
}

// TODO
DialogRef settingsDialog = 0;
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
  const EventTypeSpec win_events[] = {
    { kEventClassWindow, kEventWindowClosed },
    { kEventClassCommand, kEventCommandProcess }
  };
  CreateStandardAlert(kAlertNoteAlert, CFSTR("settings dialog"), CFSTR("not yet implemented. use .xjadeorc file in your $HOME to make settings persist."),NULL, &settingsDialog);
  InstallWindowEventHandler (GetDialogWindow(settingsDialog), NewEventHandlerUPP (DialogEventHandler), GetEventTypeCount(win_events), win_events, GetDialogWindow(settingsDialog), NULL);
  ShowWindow (GetDialogWindow(settingsDialog));
  RunAppModalLoopForWindow (GetDialogWindow(settingsDialog));
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
      unsigned char revpath[PATH_MAX];
      OSErr error;
      error = FSRefMakePath(&fsRef, revpath, sizeof(revpath));
      if (error !=noErr) continue;
      if (!(interaction_override&OVR_LOADFILE))
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
        if(kind == kEventMouseUp) {
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
          result = eventNotHandledErr;
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

static OSStatus DialogEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
  if (midiAlertDialog) {
    QuitAppModalLoopForWindow(GetDialogWindow(midiAlertDialog));
    DisposeDialog(midiAlertDialog);
    midiAlertDialog=0;
  }
  if (settingsDialog) {
    QuitAppModalLoopForWindow(GetDialogWindow(settingsDialog));
    DisposeDialog(settingsDialog);
    settingsDialog=0;
  }
  return noErr;
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
        if ((interaction_override&OVR_QUIT_WMG) == 0 || ask_close()) {
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

  uint32_t d_height= ffctv_height;
  uint32_t d_width = ffctv_width;

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
  GetWindowBounds(theWindow, kWindowContentRgn, &oldWinBounds);

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
  if (vo_fs != vo_mac_fs)
    window_fullscreen();
}

int get_fullscreen_mac() {
  return (vo_fs);
}

void mousepointer_mac (int a) {
  if (a>1) a = !hide_mouse;
  //printf("mouse cursor: %s\n",a?"hide":"show");
  if (a) {
    CGDisplayHideCursor(kCGDirectMainDisplay);
    hide_mouse = 1;
  } else {
    CGDisplayShowCursor(kCGDirectMainDisplay);
    hide_mouse = 0;
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

void mac_letterbox_change (void){
  force_redraw=1;
  window_resized_mac();
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
    case 0x35: if ((interaction_override&OVR_QUIT_KEY) == 0) {
                 remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) 0xff1b);
                 loop_flag=0; return;  // ESCAPE
               }
    default: c= (char) charcode;
  }
  switch (c) {
    case 'a': ontop_mac(winLevel==2?0:1); break;
    case 'f': fullscreen_mac(!vo_fs); break;
    case 'l':
      want_letterbox = !want_letterbox;
      mac_letterbox_change();
      break;
    case 'o':
      ui_osd_offset_cycle();
      break;
    case 'p':
      ui_osd_permute();
      break;
    case 's':
      ui_osd_tc();
      break;
    case 'v': //'v' // OSD - current video frame
      ui_osd_fn();
      break;
    case 'b': //'b' // OSD - black box
      ui_osd_box();
      break;
    case 'C': //'C' // OSD - clear all
      ui_osd_clear();
      break;
    case 'i': //'i' // OSD - fileinfo
      ui_osd_fileinfo();
      break;
    case 'g': //'g' // OSD - geometry
      ui_osd_geo();
      break;
    case '\\':
      XCtimeoffset(0, (unsigned int) charcode);
      break;
    case '+':
      XCtimeoffset(1, (unsigned int) charcode);
      break;
    case '-':
      XCtimeoffset(-1, (unsigned int) charcode);
      break;
    case '{':
      XCtimeoffset(-2, (unsigned int) charcode);
      break;
    case '}':
      XCtimeoffset(2, (unsigned int) charcode);
      break;
    case 'm': mousepointer_mac(2); break;
    case '.': //'.' // resize 100%
      XCresize_percent(100);
      break;
    case ',': //',' // resize to aspect ratio
      XCresize_aspect(0);
      break;
    case '<': //'<' // resize *.83
      XCresize_scale(-1);
      break;
    case '>': //'>' // resize *1.2
      XCresize_scale(1);
      break;
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
    case 0x8:
      if ((interaction_override&OVR_JCONTROL) == 0) jackt_rewind();
      remote_notify(NTY_KEYBOARD, 310, "keypress=%d # backspace", (unsigned int) 0xff08);
      break;
    case ' ':
      if ((interaction_override&OVR_JCONTROL) == 0) jackt_toggle();
      remote_notify(NTY_KEYBOARD, 310, "keypress=%d # space", (unsigned int) 0x0020);
      break;
    default:
      remote_notify(NTY_KEYBOARD, 310, "keypress=%d", (unsigned int) charcode);
      //printf("yet unhandled keyboard event: '%c' 0x%x\n",c,c);
      break;
  }
  checkMyMenu();
}

OSStatus mac_menu_cmd(OSStatus result, HICommand *acmd) {
  switch ( acmd->commandID ) {
    case kHICommandQuit:
    case mQuit:
      if ((interaction_override&OVR_QUIT_OSX) == 0 || ask_close())
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
      {
        resize_mac(ffctv_width/2, ffctv_height/2);
      }
      break;
    case mNormalScreen:
      {
        resize_mac(ffctv_width, ffctv_height);
      }
      break;
  //case kHICommandFullScreen:
    case mFullScreen:
      vo_fs = (!(vo_fs)); window_fullscreen();
      break;
    case mDoubleScreen:
      {
        resize_mac(ffctv_width*2, ffctv_height*2);
      }
      break;
    case mKeepAspect:
      want_letterbox = !want_letterbox;
      force_redraw=1;
      window_resized_mac();
      break;
    case mOSDFrame:
      ui_osd_fn();
      break;
    case mOSDSmpte:
      ui_osd_tc();
      break;
    case mOSDOffO:
      ui_osd_offset_none();
      break;
    case mOSDOffS:
      ui_osd_offset_tc();
      break;
    case mOSDOffF:
      ui_osd_offset_fn();
      break;
    case mOSDBox:
      ui_osd_box();
    break;
    case mJackPlay:
      if ((interaction_override&OVR_JCONTROL) == 0)
        jackt_start();
      break;
    case mJackRewind:
      if ((interaction_override&OVR_JCONTROL) == 0)
        jackt_rewind();
      break;
    case mJackStop:
      jackt_stop();
      break;
    case mSyncJack:
	ui_sync_to_jack();
      break;
    case mSyncLTC:
	ui_sync_to_ltc();
      break;
    case mSyncJackMidi:
	ui_sync_to_mtc_jack();
      break;
    case mSyncPortMidi:
	ui_sync_to_mtc_portmidi();
	if (ui_syncsource() != SYNC_MTC_PORTMIDI) { midiAlert(); }
      break;
    case mSyncNone:
      ui_sync_none();
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

#endif /* PLATFORM_OSX PPC,i386 */
// vim: sw=2 sts=2 ts=8 et
