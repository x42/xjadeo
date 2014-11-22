/* xjadeo - jack video monitor
 *
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

#include "display_gl_common.h"
#if (defined HAVE_GL && defined PLATFORM_OSX)

void xapi_open (void *d);
void xapi_close (void *d);
extern double framerate;

#import <Cocoa/Cocoa.h>
#include <libgen.h>
#include <pthread.h>

static pthread_mutex_t osx_vbuf_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t          osx_vbuf_size = 0;
static uint8_t        *osx_vbuf = NULL;

static int osxgui_status = -1000;
static NSAutoreleasePool *pool = nil;

static pthread_mutex_t osxgui_sync = PTHREAD_MUTEX_INITIALIZER;
static volatile uint8_t sync_sem;
static void gl_sync_lock ()   { if (osxgui_status > 0) { sync_sem = 1; pthread_mutex_lock (&osxgui_sync); }}
static void gl_sync_unlock () { if (osxgui_status > 0) { sync_sem = 0; pthread_mutex_unlock (&osxgui_sync); }}

static void gl_newsrc_sync () ;
static bool _newsrc = false;

#define PTLL gl_sync_lock()
#define PTUL gl_sync_unlock()

//forward declaration, need for 64bit apps on 10.5
OSErr UpdateSystemActivity (UInt8 d);

@interface MenuDLG : NSObject
{
	int menuID;
}
- (void)setMenuId:(int)id;
@end

__attribute__ ((visibility ("hidden")))
@interface XjadeoWindow : NSWindow
{
@public
}

- (id) initWithContentRect:(NSRect)contentRect
                 styleMask:(unsigned int)aStyle
                   backing:(NSBackingStoreType)bufferingType
                     defer:(BOOL)flag;
- (BOOL) windowShouldClose:(id)sender;
- (void) becomeKeyWindow:(id)sender;
- (BOOL) canBecomeKeyWindow:(id)sender;
- (void) miniaturize:(id)sender;
- (void) deminiaturize:(id)sender;
@end


@implementation XjadeoWindow

- (id) initWithContentRect:(NSRect)contentRect
                 styleMask:(unsigned int)aStyle
                   backing:(NSBackingStoreType)bufferingType
                     defer:(BOOL)flag
{
	@try {
		NSWindow* w = [super initWithContentRect:contentRect
		                               styleMask:(NSClosableWindowMask | NSTitledWindowMask | NSMiniaturizableWindowMask)
		                                 backing:NSBackingStoreBuffered defer:NO];

		[w setAcceptsMouseMovedEvents:YES];
		[w setLevel:NSNormalWindowLevel];

		return w;
	}
	@catch ( NSException *e ) {
		return nil;
	}
	return nil;
}

- (BOOL) windowShouldClose:(id)sender
{
	if ((interaction_override&OVR_QUIT_WMG) == 0) {
		loop_flag = 0;
		return YES;
	}
	return NO;
}

- (void) becomeKeyWindow:(id)sender
{
}

- (BOOL) canBecomeKeyWindow:(id)sender{
	return YES;
}

- (void) miniaturize:(id)sender
{
	loop_run = 0;
	[super miniaturize:sender];
}

- (void) deminiaturize:(id)sender
{
	loop_run = 1;
	[super deminiaturize:sender];
}

@end


__attribute__ ((visibility ("hidden")))
@interface XjadeoOpenGLView : NSOpenGLView
{
	int colorBits;
	int depthBits;
	BOOL resizing;
@public
	NSTrackingArea* trackingArea;
}

- (id) initWithFrame:(NSRect)frame
           colorBits:(int)numColorBits
           depthBits:(int)numDepthBits;
- (void) reshape;
- (void) drawRect:(NSRect)rect;
- (void) setFullScreen:(int)onoff;

@end

@implementation XjadeoOpenGLView

- (id) initWithFrame:(NSRect)frame
           colorBits:(int)numColorBits
           depthBits:(int)numDepthBits
{
	colorBits = numColorBits;
	depthBits = numDepthBits;
	resizing  = FALSE;

	NSOpenGLPixelFormatAttribute pixelAttribs[16] = {
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFAColorSize,
		colorBits,
		NSOpenGLPFADepthSize,
		depthBits,
		0
	};

	NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttribs];

	if (pixelFormat) {
		self = [super initWithFrame:frame pixelFormat:pixelFormat];
		[pixelFormat release];
		if (self) {
			[[self openGLContext] makeCurrentContext];
			[self reshape];
		}
	} else {
		self = nil;
	}
	return self;
}

- (void) reshape
{
	[[self openGLContext] update];

	NSRect bounds = [self bounds];
	gl_reshape(bounds.size.width, bounds.size.height);
}

- (void) drawRect:(NSRect)rect
{
	pthread_mutex_lock(&osx_vbuf_lock);
	if (_newsrc) { gl_newsrc_sync(); }
	xjglExpose(osx_vbuf);
	pthread_mutex_unlock(&osx_vbuf_lock);
}

-(void) updateTrackingAreas
{
	if (trackingArea != nil) {
		[self removeTrackingArea:trackingArea];
		[trackingArea release];
	}

	const int opts = (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways);
	trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
	                                             options:opts
	                                               owner:self
	                                            userInfo:nil];
	[self addTrackingArea:trackingArea];
}

- (void) mouseEntered:(NSEvent*)theEvent
{
	[self updateTrackingAreas];
	gl_mousepointer (hide_mouse);
}

- (void) mouseExited:(NSEvent*)theEvent
{
	CGDisplayShowCursor (kCGDirectMainDisplay);
}

- (void) mouseDragged:(NSEvent*)event
{
	NSRect frame = [[self window] frame];
	NSPoint mp   = [self convertPoint:[event locationInWindow] fromView:self];
	if (!resizing && osd_seeking && ui_syncsource() == SYNC_NONE && OSD_mode & OSD_POS) {
		const float sk = calc_slider (mp.x, _gl_height - mp.y);
		if (sk >= 0) {
			// no lock, userFrame is queried atomically
			// and transport is disconnected when osd_seeking is set
			ui_sync_manual (sk);
		}
	}
	else if (resizing || !(mp.y > 16 || frame.size.width - mp.x > 16)) {
		resizing = TRUE;

		// TODO use absolute mouse position, clamp to window edge
		CGFloat originY = frame.origin.y;
		CGFloat deltaY  = [event deltaY];

		frame.origin.y = (originY + frame.size.height) - (frame.size.height + deltaY);
		frame.size.width  += [event deltaX];
		frame.size.height += deltaY;

		if (frame.size.width < 80)
			frame.size.width = 80;

		if (frame.size.height < 60) {
			frame.size.height = 60;
			frame.origin.y = originY;
		}
		[[self window] setFrame:frame display:YES animate:NO];
	}
}

- (void) mouseDown:(NSEvent*)event
{
	if (ui_syncsource() == SYNC_NONE && OSD_mode & OSD_POS) {
		NSPoint mp = [self convertPoint:[event locationInWindow] fromView:self];
		const float sk = calc_slider (mp.x, _gl_height - mp.y);
		if (sk >= 0) {
			PTLL;
			ui_sync_manual (sk);
			osd_seeking = 1;
			force_redraw = 1;
			PTUL;
		}
	}
}

- (void) mouseUp:(NSEvent*)event
{
	if (resizing || osd_seeking) {
		if (osd_seeking) {
			PTLL;
			osd_seeking = 0;
			force_redraw = 1;
			PTUL;
		}
		resizing = FALSE;
	} else {
		xjglButton(1);
	}
}

- (void) rightMouseUp:(NSEvent*)event
{
	xjglButton(3);
}

- (void) scrollWheel:(NSEvent*)event
{
	const double dy = [event deltaY];
	if (dy > 0) {
		xjglButton(4);
	} else if (dy < 0) {
		xjglButton(5);
	}
	[self updateTrackingAreas];
}

- (void) keyDown:(NSEvent*)event
{
	NSString* chars = [event characters];
	char buf[2] = {[chars characterAtIndex:0], 0};
	xjglKeyPress([chars characterAtIndex:0], buf);
}

- (void) setFullScreen:(int)onoff
{
	if([self isInFullScreenMode] && !onoff)
	{
		[self exitFullScreenModeWithOptions:nil];
	}
	else if (![self isInFullScreenMode] && onoff)
	{
		[self enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
	}
}

@end

static XjadeoOpenGLView* osx_glview;
static id osx_window;


// Menus
static NSMenuItem *mFileOpen;
static NSMenuItem *mFileClose;
static NSMenuItem *mFileRecent;
static NSMenuItem *mJackTransport;
static NSMenuItem *mSyncJACK;
#ifdef HAVE_LTC
static NSMenuItem *mSyncLTC;
#endif
#ifdef HAVE_JACKMIDI
static NSMenuItem *mSyncMTCJ; // jack
#endif
#ifdef HAVE_PORTMIDI
static NSMenuItem *mSyncMTCP; // portmidi
#endif
static NSMenuItem *mSyncNone;

static NSMenuItem *mOsdTC;
static NSMenuItem *mOsdVtcOff;
static NSMenuItem *mOsdVtcTc;
static NSMenuItem *mOsdVtcFn;
static NSMenuItem *mOsdOffOff;
static NSMenuItem *mOsdOffTc;
static NSMenuItem *mOsdOffFn;
static NSMenuItem *mOsdBox;
static NSMenuItem *mOsdTNfo;
static NSMenuItem *mOsdGNfo;
static NSMenuItem *mOsdGeo;

static NSMenuItem *mDpyLetterbox;
static NSMenuItem *mDpyOnTop;
static NSMenuItem *mDpyFullscreen;
static NSMenuItem *mDpyMouseCursor;

static void update_sync_menu() {
	switch(ui_syncsource()) {
		case SYNC_JACK:
			[mJackTransport setEnabled:YES];
			[mSyncNone setState:NSOffState];
			[mSyncJACK setState:NSOnState];
#ifdef HAVE_LTC
			[mSyncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[mSyncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[mSyncMTCP setState:NSOffState];
#endif
		break;
		case SYNC_LTC:
			[mJackTransport setEnabled:NO];
			[mSyncNone setState:NSOffState];
			[mSyncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[mSyncLTC  setState:NSOnState];
#endif
#ifdef HAVE_JACKMIDI
			[mSyncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[mSyncMTCP setState:NSOffState];
#endif
		break;
		case SYNC_MTC_PORTMIDI:
			[mJackTransport setEnabled:NO];
			[mSyncNone setState:NSOffState];
			[mSyncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[mSyncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[mSyncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[mSyncMTCP setState:NSOnState];
#endif
		break;
		case SYNC_MTC_JACK:
			[mJackTransport setEnabled:NO];
			[mSyncNone setState:NSOffState];
			[mSyncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[mSyncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[mSyncMTCJ setState:NSOnState];
#endif
#ifdef HAVE_PORTMIDI
			[mSyncMTCP setState:NSOffState];
#endif
		break;
		default:
			[mJackTransport setEnabled:NO];
			[mSyncNone setState:NSOnState];
			[mSyncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[mSyncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[mSyncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[mSyncMTCP setState:NSOffState];
#endif
		break;
	}
	if (interaction_override&OVR_MENUSYNC) {
		[mJackTransport setEnabled:NO];
		[mSyncNone setEnabled:NO];
		[mSyncJACK setEnabled:NO];
#ifdef HAVE_LTC
		[mSyncLTC  setEnabled:NO];
#endif
#ifdef HAVE_JACKMIDI
		[mSyncMTCJ setEnabled:NO];
#endif
#ifdef HAVE_PORTMIDI
		[mSyncMTCP setEnabled:NO];
#endif
	} else {
		[mSyncNone setEnabled:YES];
		[mSyncJACK setEnabled:YES];
#ifdef HAVE_LTC
		[mSyncLTC  setEnabled:YES];
#endif
#ifdef HAVE_JACKMIDI
		[mSyncMTCJ setEnabled:YES];
#endif
#ifdef HAVE_PORTMIDI
		[mSyncMTCP setEnabled:YES];
#endif
	}
}

static void update_osd_menu () {
	[mOsdTC     setState:(OSD_mode & OSD_SMPTE) ? NSOnState : NSOffState];
	[mOsdVtcOff setState:(OSD_mode & (OSD_SMPTE|OSD_VTC)) ? NSOffState : NSOnState];
	[mOsdVtcTc  setState:(OSD_mode & OSD_FRAME) ? NSOnState : NSOffState];
	[mOsdVtcFn  setState:(OSD_mode & OSD_VTC)   ? NSOnState : NSOffState];
	[mOsdOffOff setState:(OSD_mode & (OSD_OFFF|OSD_OFFS)) ? NSOffState : NSOnState];
	[mOsdOffTc  setState:(OSD_mode & OSD_OFFS)  ? NSOnState : NSOffState];
	[mOsdOffFn  setState:(OSD_mode & OSD_OFFF)  ? NSOnState : NSOffState];
	[mOsdBox    setState:(OSD_mode & OSD_BOX)   ? NSOnState : NSOffState];
	[mOsdTNfo   setState:(OSD_mode & OSD_NFO)   ? NSOnState : NSOffState];
	[mOsdGNfo   setState:(OSD_mode & OSD_GEO)   ? NSOnState : NSOffState];
	[mOsdTNfo   setEnabled:(movie_height < OSD_MIN_NFO_HEIGHT) ? NO : YES];
	[mOsdGNfo   setEnabled:(movie_height < OSD_MIN_NFO_HEIGHT) ? NO : YES];
	[mOsdGeo    setState:(OSD_mode & OSD_GEO)   ? NSOnState : NSOffState];
	[mOsdGeo    setEnabled:(movie_height < OSD_MIN_NFO_HEIGHT) ? NO : YES];
}

static void update_dpy_menu () {
	[mDpyLetterbox   setState:Xgetletterbox()    ? NSOnState : NSOffState];
	[mDpyOnTop       setState:Xgetontop()        ? NSOnState : NSOffState];
	[mDpyFullscreen  setState:Xgetfullscreen()   ? NSOnState : NSOffState];
	[mDpyMouseCursor setState:Xgetmousepointer() ? NSOnState : NSOffState];
}

@implementation MenuDLG
- (void)setMenuId: (int)id
{
	menuID = id;
}
- (void) menuWillOpen: (NSMenu *)menu
{
	switch (menuID) {
		case 1:
			if (interaction_override&OVR_LOADFILE) {
				[mFileOpen setEnabled:NO];
				[mFileClose setEnabled:NO];
				[mFileRecent setEnabled:NO];
			} else {
				unsigned int i, recent = x_fib_recent_count();
				[mFileOpen setEnabled:YES];
				[mFileClose setEnabled:  have_open_file() ? YES : NO];
				[mFileRecent setEnabled: recent > 0 ? YES : NO];

				NSMenu *recentMenu = [[NSMenu alloc] initWithTitle:@"Recently Used"];
				for (i = 0; i < recent && i < 8; ++i) {
					if (!x_fib_recent_at (i)) break;
					char *tmp = strdup (x_fib_recent_at (i));
					char *base = basename(tmp);
					if (base) {
						NSString *name = [NSString stringWithUTF8String:base];
						NSMenuItem *mi = [recentMenu addItemWithTitle:name action:@selector(openRecent:) keyEquivalent:@""];
						[mi setRepresentedObject: [NSNumber numberWithInt:i]];
					}
					free(tmp);
				}
				[mFileRecent setSubmenu:recentMenu];
				[recentMenu release];
			}
			break;
		case 2:
			update_sync_menu();
			break;
		case 3:
			update_osd_menu();
			break;
		case 4:
			update_dpy_menu();
			break;
		default:
			break;
	}
}
@end


@interface NSApplication (XJ)
@end

@implementation NSApplication (XJ)

/* Invoked from the Quit menu item */
- (void) terminate: (id)sender
{
	if ((interaction_override&OVR_QUIT_OSX) == 0) {
		loop_flag = 0;
	}
}

- (void) showAbout: (id)sender
{
	NSAttributedString *credits = [[NSAttributedString alloc]
		initWithString:@"(C) 2006-2014\nRobin Gareus & Luis Garrido"
		];

	NSDictionary *aboutDict = [NSDictionary dictionaryWithObjectsAndKeys:
		credits, @"Credits",
		@"GNU General Public Licence Version 2", @"Copyright",
		@"XJadeo", @"ApplicationName",
		@VERSION, @"Version",
		nil];
	[NSApp orderFrontStandardAboutPanelWithOptions:aboutDict];
	[credits release];
}


- (void) openVideo: (id)sender
{
	if (interaction_override&OVR_LOADFILE) return;
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:YES];
	[panel setCanChooseDirectories:NO];
	[panel setAllowsMultipleSelection:NO];

	NSInteger clicked = [panel runModal];

	if (clicked == NSFileHandlingPanelOKButton) {
		for (NSURL *url in [panel URLs]) {
			if (![url isFileURL]) continue;
			//NSLog(@"%@", url.path);
			const char *fn= [url.path UTF8String];
			PTLL; xapi_open ((void*)fn); PTUL;
			break;
		}
	}
	if (osx_window) {
		[osx_window makeKeyAndOrderFront:osx_window];
	}
}

- (void) closeVideo: (id)sender
{
	if (interaction_override&OVR_LOADFILE) return;
	PTLL; xapi_close (NULL); PTUL;
}

- (void) openRecent: (id)sender
{
	if (interaction_override&OVR_LOADFILE) return;
	int selectedItem = [[sender representedObject] intValue];
	const char *fn = x_fib_recent_at (selectedItem);
	if (fn) {
		PTLL; xapi_open ((void*)fn); PTUL;
	}
}

- (void) syncJack: (id)sender { PTLL; ui_sync_to_jack(); PTUL; }
- (void) syncLTC: (id)sender  { PTLL; ui_sync_to_ltc(); PTUL; }
- (void) syncMTCJ: (id)sender { PTLL; ui_sync_to_mtc_jack(); PTUL; }
- (void) syncMTCP: (id)sender { PTLL; ui_sync_to_mtc_portmidi(); PTUL; }
- (void) syncNone: (id)sender { PTLL; ui_sync_none(); PTUL; }

- (void) osdVtcNone: (id)sender { PTLL; ui_osd_vtc_off(); PTUL; }
- (void) osdVtcTc: (id)sender   { PTLL; ui_osd_vtc_tc(); PTUL; }
- (void) osdVtcFn: (id)sender   { PTLL; ui_osd_vtc_fn(); PTUL; }
- (void) osdTc: (id)sender      { PTLL; ui_osd_tc(); PTUL; }
- (void) osdPos: (id)sender     { PTLL; ui_osd_permute(); PTUL; }
- (void) osdOffNone: (id)sender { PTLL; ui_osd_offset_none(); PTUL; }
- (void) osdOffFn: (id)sender   { PTLL; ui_osd_offset_fn(); PTUL; }
- (void) osdOffTc: (id)sender   { PTLL; ui_osd_offset_tc(); PTUL; }
- (void) osdBox: (id)sender     { PTLL; ui_osd_box(); PTUL; }
- (void) osdNfo: (id)sender     { PTLL; ui_osd_fileinfo(); PTUL; }
- (void) osdGeo: (id)sender     { PTLL; ui_osd_geo(); PTUL; }
- (void) osdClear: (id)sender   { PTLL; ui_osd_clear(); PTUL; }

- (void) dpySize50: (id)sender      { XCresize_percent(50); }
- (void) dpySize100: (id)sender     { XCresize_percent(100); }
- (void) dpySize150: (id)sender     { XCresize_percent(150); }
- (void) dpySizeDec: (id)sender     { XCresize_scale(-1); }
- (void) dpySizeInc: (id)sender     { XCresize_scale( 1); }
- (void) dpyAspect: (id)sender      { XCresize_aspect(0); }
- (void) dpyLetterbox: (id)sender   { Xletterbox(2); }
- (void) dpyOnTop: (id)sender       { Xontop(2); }
- (void) dpyFullscreen: (id)sender  { Xfullscreen(2); }
- (void) dpyMouseCursor: (id)sender { hide_mouse^=1; }

- (void) offZero: (id)sender { PTLL; XCtimeoffset( 0, 0); PTUL; }
- (void) offPF: (id)sender   { PTLL; XCtimeoffset( 1, 0); PTUL; }
- (void) offMF: (id)sender   { PTLL; XCtimeoffset(-1, 0); PTUL; }
- (void) offPM: (id)sender   { PTLL; XCtimeoffset( 2, 0); PTUL; }
- (void) offMM: (id)sender   { PTLL; XCtimeoffset(-2, 0); PTUL; }
- (void) offPH: (id)sender   { PTLL; XCtimeoffset( 3, 0); PTUL; }
- (void) offMH: (id)sender   { PTLL; XCtimeoffset(-3, 0); PTUL; }

- (void) jackPlayPause: (id)sender  { jackt_toggle(); }
- (void) jackPlay: (id)sender       { jackt_start(); }
- (void) jackStop: (id)sender       { jackt_stop(); }
- (void) jackRewind: (id)sender     { jackt_rewind(); }
@end

static void gl_make_current () {
	NSOpenGLContext* context = [osx_glview openGLContext];
	[context makeCurrentContext];
}

static void gl_swap_buffers () {
	glSwapAPPLE ();
}

void gl_newsrc () {
	pthread_mutex_lock (&osx_vbuf_lock);
	_newsrc = true;
	pthread_mutex_unlock (&osx_vbuf_lock);
}

static void gl_newsrc_sync () {
	free (osx_vbuf);
	osx_vbuf_size = video_buffer_size ();
	if (osx_vbuf_size > 0) {
		osx_vbuf = (uint8_t*)malloc (osx_vbuf_size * sizeof(uint8_t));
		gl_reallocate_texture (movie_width, movie_height);
	}
	_newsrc = false;
}

static void makeAppMenu (void) {
	NSMenuItem *menuItem;
	id menubar = [[NSMenu new] autorelease];
	id appMenuItem = [[NSMenuItem new] autorelease];
	[menubar addItem:appMenuItem];

	[NSApp setMainMenu:menubar];

	/* Main Application Menu */
	id appMenu = [[NSMenu new] autorelease];
	NSString *appName = @"xjadeo";
	NSString *title;

#if 1
	title = [@"About " stringByAppendingString:appName];
	[appMenu addItemWithTitle:title action:@selector(showAbout:) keyEquivalent:@""];

	[appMenu addItem:[NSMenuItem separatorItem]];
#endif

	title = [@"Hide " stringByAppendingString:appName];
	[appMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

	menuItem = (NSMenuItem *)[appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];
	[appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[appMenu addItem:[NSMenuItem separatorItem]];

	title = [@"Quit " stringByAppendingString:appName];
	[appMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

	[appMenuItem setSubmenu:appMenu];

	/* Create the file menu */
	NSMenu     *fileMenu;
	NSMenuItem *fileMenuItem;
	fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
	[fileMenu setAutoenablesItems:NO];

	MenuDLG *filedlg = [[MenuDLG new] autorelease];
	[filedlg setMenuId:1];
	[fileMenu setDelegate:filedlg];

	mFileOpen  = [fileMenu addItemWithTitle:@"Open" action:@selector(openVideo:) keyEquivalent:@"o"];
	mFileClose = [fileMenu addItemWithTitle:@"Close" action:@selector(closeVideo:) keyEquivalent:@"w"];

	mFileRecent = [[NSMenuItem alloc] initWithTitle:@"Recent" action:nil keyEquivalent:@""];
	[fileMenu addItem:[NSMenuItem separatorItem]];
	[fileMenu addItem:mFileRecent];

	fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
	[fileMenuItem setSubmenu:fileMenu];
	[[NSApp mainMenu] addItem:fileMenuItem];
	[fileMenu release];
	[fileMenuItem release];

	/* Create the sync menu */
	NSMenu     *syncMenu;
	NSMenuItem *syncMenuItem;
	syncMenu = [[NSMenu alloc] initWithTitle:@"Sync"];
	[syncMenu setAutoenablesItems:NO];

	MenuDLG *syncdlg = [[MenuDLG new] autorelease];
	[syncdlg setMenuId:2];
	[syncMenu setDelegate:syncdlg];

	mSyncJACK = [syncMenu addItemWithTitle:@"JACK" action:@selector(syncJack:) keyEquivalent:@"j"];
#ifdef HAVE_LTC
	mSyncLTC  = [syncMenu addItemWithTitle:@"LTC" action:@selector(syncLTC:) keyEquivalent:@"l"];
#endif
#ifdef HAVE_JACKMIDI
	mSyncMTCJ  = [syncMenu addItemWithTitle:@"MTC (jackmidi)" action:@selector(syncMTCJ:) keyEquivalent:@"m"];
#endif
#ifdef HAVE_PORTMIDI
	mSyncMTCP  = [syncMenu addItemWithTitle:@"MTC (portmidi)" action:@selector(syncMTCP:) keyEquivalent:@"p"];
#endif
	mSyncNone = [syncMenu addItemWithTitle:@"None" action:@selector(syncNone:) keyEquivalent:@""];
	[mSyncNone setEnabled:NO];

	[syncMenu addItem:[NSMenuItem separatorItem]];

	NSMenu     *jackMenu;
	jackMenu = [[NSMenu alloc] initWithTitle:@"JACK Transport"];

	menuItem = [jackMenu addItemWithTitle:@"Play/Pause" action:@selector(jackPlayPause:) keyEquivalent:@" "];
	[menuItem setKeyEquivalentModifierMask:0];
	[jackMenu addItemWithTitle:@"Play" action:@selector(jackPlay:) keyEquivalent:@""];
	[jackMenu addItemWithTitle:@"Stop" action:@selector(jackStop:) keyEquivalent:@""];
	menuItem = [jackMenu addItemWithTitle:@"Rewind" action:@selector(jackRewind:) keyEquivalent:@"\010"];
	[menuItem setKeyEquivalentModifierMask:0];

	mJackTransport = [[NSMenuItem alloc] initWithTitle:@"JACK Transport" action:nil keyEquivalent:@""];
	[mJackTransport setSubmenu:jackMenu];
	[syncMenu addItem:mJackTransport];
	[jackMenu release];

	syncMenuItem = [[NSMenuItem alloc] initWithTitle:@"Sync" action:nil keyEquivalent:@""];
	[syncMenuItem setSubmenu:syncMenu];
	[[NSApp mainMenu] addItem:syncMenuItem];
	[syncMenu release];
	[syncMenuItem release];

	/* Create the OSD menu */
	NSMenu     *osdMenu;
	NSMenuItem *osdMenuItem;
	osdMenu = [[NSMenu alloc] initWithTitle:@"OSD"];
	[osdMenu setAutoenablesItems:NO];

	MenuDLG *osddlg = [[MenuDLG new] autorelease];
	[osddlg setMenuId:3];
	[osdMenu setDelegate:osddlg];

	mOsdTC     = [osdMenu addItemWithTitle:@"External Timecode" action:@selector(osdTc:) keyEquivalent:@"s"];
	[osdMenu addItem:[NSMenuItem separatorItem]];
	mOsdVtcOff = [osdMenu addItemWithTitle:@"VTC Off" action:@selector(osdVtcNone:) keyEquivalent:@"v"];
	mOsdVtcTc  = [osdMenu addItemWithTitle:@"VTC Timecode" action:@selector(osdVtcTc:) keyEquivalent:@""];
	mOsdVtcFn  = [osdMenu addItemWithTitle:@"VTC Frame Number" action:@selector(osdVtcFn:) keyEquivalent:@""];
	[osdMenu addItem:[NSMenuItem separatorItem]];
	mOsdOffOff = [osdMenu addItemWithTitle:@"Offset Off" action:@selector(osdOffNone:) keyEquivalent:@"o"];
	mOsdOffTc  = [osdMenu addItemWithTitle:@"Offset Timecode" action:@selector(osdOffTc:) keyEquivalent:@""];
	mOsdOffFn  = [osdMenu addItemWithTitle:@"Offset Frame Number" action:@selector(osdOffFn:) keyEquivalent:@""];
	[osdMenu addItem:[NSMenuItem separatorItem]];
	mOsdTNfo   = [osdMenu addItemWithTitle:@"Time Info" action:@selector(osdNfo:) keyEquivalent:@"i"];
	mOsdGNfo   = [osdMenu addItemWithTitle:@"Geometry" action:@selector(osdGeo:) keyEquivalent:@"g"];
	[osdMenu addItem:[NSMenuItem separatorItem]];
	mOsdBox    = [osdMenu addItemWithTitle:@"Background" action:@selector(osdBox:) keyEquivalent:@"b"];
	menuItem   = [osdMenu addItemWithTitle:@"Swap Position" action:@selector(osdPos:) keyEquivalent:@"p"];
	[menuItem   setKeyEquivalentModifierMask:0];
	[osdMenu addItem:[NSMenuItem separatorItem]];
	menuItem   = [osdMenu addItemWithTitle:@"Clear All" action:@selector(osdClear:) keyEquivalent:@"C"];
	[menuItem   setKeyEquivalentModifierMask:NSShiftKeyMask];

	[menuItem   setKeyEquivalentModifierMask:0];
	[mOsdVtcOff setKeyEquivalentModifierMask:0];
	[mOsdOffOff setKeyEquivalentModifierMask:0];
	[mOsdGNfo   setKeyEquivalentModifierMask:0];
	[mOsdTNfo   setKeyEquivalentModifierMask:0];
	[mOsdGeo    setKeyEquivalentModifierMask:0];
	[mOsdBox    setKeyEquivalentModifierMask:0];
	[mOsdTC     setKeyEquivalentModifierMask:0];

	osdMenuItem = [[NSMenuItem alloc] initWithTitle:@"OSD" action:nil keyEquivalent:@""];
	[osdMenuItem setSubmenu:osdMenu];
	[[NSApp mainMenu] addItem:osdMenuItem];
	[osdMenu release];
	[osdMenuItem release];

	/* Create the Display menu */
	NSMenu     *dpyMenu;
	NSMenuItem *dpyMenuItem;
	dpyMenu = [[NSMenu alloc] initWithTitle:@"Display"];
	[dpyMenu setAutoenablesItems:NO];

	MenuDLG *dpydlg = [[MenuDLG new] autorelease];
	[dpydlg setMenuId:4];
	[dpyMenu setDelegate:dpydlg];

	[dpyMenu addItemWithTitle:@"50%"  action:@selector(dpySize50:) keyEquivalent:@""];
	menuItem = [dpyMenu addItemWithTitle:@"100%" action:@selector(dpySize100:) keyEquivalent:@"."];
	[dpyMenu addItemWithTitle:@"150%" action:@selector(dpySize150:) keyEquivalent:@""];
	[menuItem   setKeyEquivalentModifierMask:0];

	[dpyMenu addItem:[NSMenuItem separatorItem]];
	menuItem = [dpyMenu addItemWithTitle:@"\u2013 20%" action:@selector(dpySizeDec:) keyEquivalent:@"<"];
	[menuItem   setKeyEquivalentModifierMask:NSShiftKeyMask];
	menuItem = [dpyMenu addItemWithTitle:@"+20%" action:@selector(dpySizeInc:) keyEquivalent:@">"];
	[menuItem   setKeyEquivalentModifierMask:NSShiftKeyMask];

	[dpyMenu addItem:[NSMenuItem separatorItem]];

	menuItem = [dpyMenu addItemWithTitle:@"Reset Aspect" action:@selector(dpyAspect:) keyEquivalent:@","];
	[menuItem   setKeyEquivalentModifierMask:0];
	mDpyLetterbox = [dpyMenu addItemWithTitle:@"Retain Aspect" action:@selector(dpyLetterbox:) keyEquivalent:@"l"];
	[dpyMenu addItem:[NSMenuItem separatorItem]];
	mDpyOnTop = [dpyMenu addItemWithTitle:@"Window On Top" action:@selector(dpyOnTop:) keyEquivalent:@"a"];
	mDpyFullscreen = [dpyMenu addItemWithTitle:@"Fullscreen" action:@selector(dpyFullscreen:) keyEquivalent:@"f"];
	[dpyMenu addItem:[NSMenuItem separatorItem]];
	mDpyMouseCursor = [dpyMenu addItemWithTitle:@"Mouse Cursor" action:@selector(dpyMouseCursor:) keyEquivalent:@"m"];

	[mDpyLetterbox   setKeyEquivalentModifierMask:0];
	[mDpyOnTop   setKeyEquivalentModifierMask:0];
	[mDpyFullscreen   setKeyEquivalentModifierMask:0];
	[mDpyMouseCursor   setKeyEquivalentModifierMask:0];

	dpyMenuItem = [[NSMenuItem alloc] initWithTitle:@"Display" action:nil keyEquivalent:@""];
	[dpyMenuItem setSubmenu:dpyMenu];
	[[NSApp mainMenu] addItem:dpyMenuItem];
	[dpyMenu release];
	[dpyMenuItem release];

	/* Create the Offset menu */
	NSMenu     *offMenu;
	NSMenuItem *offMenuItem;
	offMenu = [[NSMenu alloc] initWithTitle:@"Offset"];
	[offMenu setAutoenablesItems:NO];

	menuItem = [offMenu addItemWithTitle:@"Reset" action:@selector(offZero:)   keyEquivalent:@"\\"];
	[menuItem   setKeyEquivalentModifierMask:0];
	menuItem = [offMenu addItemWithTitle:@"+1 Frame"  action:@selector(offPF:) keyEquivalent:@"+"];
	[menuItem   setKeyEquivalentModifierMask:0];
	menuItem = [offMenu addItemWithTitle:@" \u20131 Frame"  action:@selector(offMF:) keyEquivalent:@"-"];
	[menuItem   setKeyEquivalentModifierMask:0];
	menuItem = [offMenu addItemWithTitle:@"+1 Minute" action:@selector(offPM:) keyEquivalent:@"{"];
	[menuItem   setKeyEquivalentModifierMask:0];
	menuItem = [offMenu addItemWithTitle:@" \u20131 Minute" action:@selector(offMM:) keyEquivalent:@"}"];
	[menuItem   setKeyEquivalentModifierMask:0];
	[offMenu addItemWithTitle:@"+1 Hour"   action:@selector(offPH:) keyEquivalent:@""];
	[offMenu addItemWithTitle:@" \u20131 Hour"   action:@selector(offMH:) keyEquivalent:@""];

	offMenuItem = [[NSMenuItem alloc] initWithTitle:@"Offset" action:nil keyEquivalent:@""];
	[offMenuItem setSubmenu:offMenu];
	[[NSApp mainMenu] addItem:offMenuItem];
	[offMenu release];
	[offMenuItem release];


	/* Create the window menu */

	NSMenu     *windowMenu;
	NSMenuItem *windowMenuItem;

	windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

	menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[windowMenu addItem:menuItem];
	[menuItem release];

	windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
	[windowMenuItem setSubmenu:windowMenu];
	[[NSApp mainMenu] addItem:windowMenuItem];

	[NSApp setWindowsMenu:windowMenu];

	[windowMenu release];
	[windowMenuItem release];
}

static void osx_close_window () {
	[NSApp removeWindowsItem:osx_window];
	[osx_window setIsVisible:NO];
	[osx_window close];
	[osx_glview release];
	[osx_window release];
	osx_window = nil;
}

static int osx_open_window () {
	const char *title = "xjadeo";

	NSString* titleString = [[NSString alloc]
		initWithBytes:title
		length:strlen(title)
		encoding:NSUTF8StringEncoding];

	id window = [[XjadeoWindow new]retain];
	if (window == nil) {
		return -1;
	}

	[window setTitle:titleString];

	osx_glview = [XjadeoOpenGLView new];
	if (!osx_glview) {
		[window release];
		return -1;
	}
	osx_window = window;

	[window setContentView:osx_glview];
	[NSApp activateIgnoringOtherApps:YES];
	[window makeFirstResponder:osx_glview];

	[window makeKeyAndOrderFront:window];

	gl_init ();
	gl_resize (ffctv_width, ffctv_height);
	if (gl_reallocate_texture (movie_width, movie_height)) {
		osx_close_window ();
		return -1;
	}

	pthread_mutex_lock (&osx_vbuf_lock);
	gl_newsrc_sync ();
	pthread_mutex_unlock (&osx_vbuf_lock);

	[window setIsVisible:YES];

	if (start_fullscreen) { gl_set_fullscreen (1); }
	if (start_ontop) { gl_set_ontop (1); }
	return 0;
}

static void osx_post_event (int data1) {
	NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
	                                    location:NSMakePoint (0,0)
	                               modifierFlags:0
	                                   timestamp:0.0
	                                windowNumber:0
	                                     context:nil
	                                     subtype:0
	                                       data1:data1
	                                       data2:0];
	[NSApp postEvent:event atStart:NO];
}


int gl_open_window () {
	while (osxgui_status == -1000) {
		usleep (10000);
	}
	if (!pool) pool = [[NSAutoreleasePool alloc] init];
	if (osxgui_status > 0) return 0; // already open
	osx_post_event (1);
	while (osxgui_status == 0) {
		usleep (10000);
	}
	return osxgui_status > 0 ? 0 : -1;
}

void osx_shutdown () {
	if (pool && osxgui_status != -1000) {
		osx_post_event (0);
	}
	if (pool) {
		[pool release]; pool = NULL;
	}
}

void gl_close_window () {
	if (osxgui_status <= 0) {
		return;
	}
	osx_post_event (2);
	while (osxgui_status) {
		usleep (10000);
	}
	free (osx_vbuf);
	osx_vbuf = NULL;
	osx_vbuf_size = 0;
}

void gl_handle_events () {
	pthread_mutex_unlock (&osxgui_sync); // XXX TODO first-time lock
	while (sync_sem) {
		sched_yield ();
	}
	pthread_mutex_lock (&osxgui_sync);
	osx_post_event (0); // TODO only on remote msg or similar event
}

void osx_main () {
	[NSAutoreleasePool new];
	[NSApplication sharedApplication];
	[NSApp setDelegate:[NSApplication sharedApplication]];
	makeAppMenu ();
	[NSApp finishLaunching];

	osxgui_status = 0;

	while (loop_flag) {
		NSEvent * event;
		do
		{
			event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantFuture] inMode:NSDefaultRunLoopMode dequeue:YES];
			if (event.type == NSApplicationDefined) {
				switch(event.data1) {
					case 1:
						if (osxgui_status == 0) {
							if (osx_open_window ()) {
								osxgui_status = -1;
							} else {
								osxgui_status = 1;
							}
						}
						break;
					case 2:
						if (osxgui_status > 0) {
							osx_close_window ();
							osxgui_status = 0;
						}
						break;
					case 3:
						pthread_mutex_lock (&osx_vbuf_lock);
						if (_newsrc) { gl_newsrc_sync(); }
						force_redraw = 1;
						xjglExpose (osx_vbuf);
						pthread_mutex_unlock (&osx_vbuf_lock);
						[osx_glview setNeedsDisplay:YES];
						break;
					default:
						break;
				}
				break;
			}

			[NSApp sendEvent:event];
		}
		while (event != nil && loop_flag);

		// TODO: use a timer, and callbacks,
		// currently triggered by osx_post_event() from gl_handle_events()
		static int periodic_sync = 5;
		if (--periodic_sync == 0) {
			periodic_sync = framerate * 50;
			UpdateSystemActivity (1 /*UsrActivity*/);
			// TODO use a one time call
			// IOPMAssertionCreateWithName() NoDisplaySleepAssertion etc.
		}
	}
	if (osxgui_status > 0) {
		osx_close_window ();
		osxgui_status = 0;
	}
	[NSAutoreleasePool release];
	osxgui_status = -1000;
}


void gl_render (uint8_t *buffer) {
	if (!osx_vbuf) return;
	pthread_mutex_lock (&osx_vbuf_lock);
	if (_newsrc) {
		osx_post_event (1);
	} else {
		memcpy (osx_vbuf, buffer, osx_vbuf_size * sizeof(uint8_t));
	}
	pthread_mutex_unlock (&osx_vbuf_lock);
	[osx_glview setNeedsDisplay:YES];
}


void gl_resize (unsigned int x, unsigned int y) {
	[osx_window setContentSize:NSMakeSize (x, y) ];
	[osx_glview reshape];
}

void gl_get_window_size (unsigned int *w, unsigned int *h) {
	*w = _gl_width;
	*h = _gl_height;
}

void gl_position (int x, int y) {
	NSRect frame = [osx_window frame];
	frame.origin.x = x;
	frame.origin.y = y;
	[osx_window setFrame:frame display:YES animate:YES];
}

void gl_get_window_pos (int *x, int *y) {
	NSRect frame = [osx_window frame];
	*x = frame.origin.x;
	*y = frame.origin.y;
}

void gl_set_ontop (int action) {
	if (action==2) _gl_ontop ^= 1;
	else _gl_ontop = action ? 1 : 0;
	if (_gl_ontop) {
		[osx_window setLevel:NSFloatingWindowLevel + 1];
	} else {
		[osx_window setLevel:NSNormalWindowLevel];
	}
}

void gl_set_fullscreen (int action) {
	// TODO check if we need performSelectorOnMainThread
	if (action==2) _gl_fullscreen^=1;
	else _gl_fullscreen = action ? 1 : 0;
	if (_gl_fullscreen) {
		[osx_glview setFullScreen:YES];
	} else {
		[osx_glview setFullScreen:NO];
	}
	if (osx_window) {
		[osx_window makeKeyAndOrderFront:osx_window];
		[osx_window makeFirstResponder:osx_glview];
	}
}

void gl_mousepointer (int action) {
	if (action==2) hide_mouse^=1;
	else hide_mouse = action ? 1 : 0;

	if (hide_mouse) {
		CGDisplayHideCursor (kCGDirectMainDisplay);
	} else {
		CGDisplayShowCursor (kCGDirectMainDisplay);
	}
}

int gl_get_ontop () {
return _gl_ontop;
}
int gl_get_fullscreen () {
return _gl_fullscreen;
}
#endif
