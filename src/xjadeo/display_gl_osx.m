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

void xapi_open(void *d);
extern double framerate;

#import <Cocoa/Cocoa.h>
#include <pthread.h>

static pthread_mutex_t osx_vbuf_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t          osx_vbuf_size = 0;
static uint8_t        *osx_vbuf = NULL;

static int osxgui_status = -1000;
static NSAutoreleasePool *pool = nil;

static pthread_mutex_t osxgui_sync = PTHREAD_MUTEX_INITIALIZER;
static volatile uint8_t sync_sem;
static void gl_sync_lock()   { if (osxgui_status > 0) { sync_sem = 1; pthread_mutex_lock(&osxgui_sync); }}
static void gl_sync_unlock() { if (osxgui_status > 0) { sync_sem = 0; pthread_mutex_unlock(&osxgui_sync); }}

#define PTLL gl_sync_lock()
#define PTUL gl_sync_unlock()

//forward declaration, need for 64bit apps on 10.5
OSErr UpdateSystemActivity(UInt8 d);

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
- (void)miniaturize:(id)sender;
- (void)deminiaturize:(id)sender;
@end


@implementation XjadeoWindow

- (id)initWithContentRect:(NSRect)contentRect
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

- (BOOL)windowShouldClose:(id)sender
{
	if ((interaction_override&OVR_QUIT_WMG) == 0) {
		loop_flag = 0;
		return YES;
	}
	return NO;
}

- (void)becomeKeyWindow:(id)sender
{
}

- (BOOL) canBecomeKeyWindow:(id)sender{
	return YES;
}

- (void)miniaturize:(id)sender
{
	loop_run = 0;
	[super miniaturize:sender];
}

- (void)deminiaturize:(id)sender
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
- (void) mouseDown:(NSEvent*)event;
- (void) rightMouseDown:(NSEvent*)event;
- (void) keyDown:(NSEvent*)event;
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
	xjglExpose(osx_vbuf);
	pthread_mutex_unlock(&osx_vbuf_lock);
}

-(void)updateTrackingAreas
{
	if (trackingArea != nil) {
		[self removeTrackingArea:trackingArea];
		[trackingArea release];
	}

	const int opts = (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways);
	trackingArea = [ [NSTrackingArea alloc] initWithRect:[self bounds]
	                                             options:opts
	                                               owner:self
	                                            userInfo:nil];
	[self addTrackingArea:trackingArea];
}

- (void)mouseEntered:(NSEvent*)theEvent
{
	[self updateTrackingAreas];
	gl_mousepointer(hide_mouse);
}

- (void)mouseExited:(NSEvent*)theEvent
{
	CGDisplayShowCursor(kCGDirectMainDisplay);
}

- (void) mouseDragged:(NSEvent*)event
{
	NSRect frame = [[self window] frame];
	NSPoint mp   = [self convertPoint: [event locationInWindow] fromView: self];
	if (!resizing && (frame.size.width - mp.x > 16 || mp.y > 16)) return;
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
	[[self window] setFrame: frame display: YES animate: NO];
}
- (void) mouseDown:(NSEvent*)event
{
	xjglButton(1);
}

- (void) mouseUp:(NSEvent*)event
{
	resizing = FALSE;
}

- (void) rightMouseDown:(NSEvent*)event
{
	xjglButton(3);
}

- (void) scrollWheel:(NSEvent*)event
{
	int xs = [event deltaX];
	if (xs > 0) {
		xjglButton(5);
	} else {
		xjglButton(4);
	}
	[self updateTrackingAreas];
}

- (void) keyDown:(NSEvent*)event
{
	if (!([event isARepeat])) {
		NSString* chars = [event characters];
		char buf[2] = {[chars characterAtIndex:0], 0};
		xjglKeyPress([chars characterAtIndex:0], buf);
	}
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

static NSMenuItem *fileOpen;
static NSMenuItem *syncJACK;
#ifdef HAVE_LTC
static NSMenuItem *syncLTC;
#endif
#ifdef HAVE_JACKMIDI
static NSMenuItem *syncMTCJ; // jack
#endif
#ifdef HAVE_PORTMIDI
static NSMenuItem *syncMTCP; // portmidi
#endif
static NSMenuItem *syncNone;

static void update_sync_menu() {
	switch(ui_syncsource()) {
		case SYNC_JACK:
			[syncNone setState:NSOffState];
			[syncJACK setState:NSOnState];
#ifdef HAVE_LTC
			[syncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[syncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[syncMTCP setState:NSOffState];
#endif
		break;
		case SYNC_LTC:
			[syncNone setState:NSOffState];
			[syncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[syncLTC  setState:NSOnState];
#endif
#ifdef HAVE_JACKMIDI
			[syncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[syncMTCP setState:NSOffState];
#endif
		break;
		case SYNC_MTC_PORTMIDI:
			[syncNone setState:NSOffState];
			[syncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[syncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[syncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[syncMTCP setState:NSOnState];
#endif
		break;
		case SYNC_MTC_JACK:
			[syncNone setState:NSOffState];
			[syncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[syncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[syncMTCJ setState:NSOnState];
#endif
#ifdef HAVE_PORTMIDI
			[syncMTCP setState:NSOffState];
#endif
		break;
		default:
			[syncNone setState:NSOnState];
			[syncJACK setState:NSOffState];
#ifdef HAVE_LTC
			[syncLTC  setState:NSOffState];
#endif
#ifdef HAVE_JACKMIDI
			[syncMTCJ setState:NSOffState];
#endif
#ifdef HAVE_PORTMIDI
			[syncMTCP setState:NSOffState];
#endif
		break;
	}
	if (interaction_override&OVR_MENUSYNC) {
		[syncNone setEnabled:NO];
		[syncJACK setEnabled:NO];
#ifdef HAVE_LTC
		[syncLTC  setEnabled:NO];
#endif
#ifdef HAVE_JACKMIDI
		[syncMTCJ setEnabled:NO];
#endif
#ifdef HAVE_PORTMIDI
		[syncMTCP setEnabled:NO];
#endif
	} else {
		[syncNone setEnabled:YES];
		[syncJACK setEnabled:YES];
#ifdef HAVE_LTC
		[syncLTC  setEnabled:YES];
#endif
#ifdef HAVE_JACKMIDI
		[syncMTCJ setEnabled:YES];
#endif
#ifdef HAVE_PORTMIDI
		[syncMTCP setEnabled:YES];
#endif
	}

	if (interaction_override&OVR_LOADFILE) {
		[fileOpen setEnabled:NO];
	} else {
		[fileOpen setEnabled:YES];
	}
}

@interface NSApplication (XJ)
@end

@implementation NSApplication (XJ)

/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
	if ((interaction_override&OVR_QUIT_OSX) == 0) {
		loop_flag = 0;
	}
}

- (void)showAbout:(id)sender
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


- (void)openVideo:(id)sender
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
			NSLog(@"%@", url.path);
			const char *fn= [url.path UTF8String];
			PTLL; xapi_open((void*)fn); PTUL;
			break;
		}
	}
	if (osx_window) {
		[osx_window makeKeyAndOrderFront:osx_window];
	}
}

- (void)syncToJack:(id)sender
{
	PTLL; ui_sync_to_jack(); PTUL;
	update_sync_menu();
}

- (void)syncToLTC:(id)sender
{
	PTLL; ui_sync_to_ltc(); PTUL;
	update_sync_menu();
}

- (void)syncToMTCJ:(id)sender
{
	PTLL; ui_sync_to_mtc_jack(); PTUL;
	update_sync_menu();
}

- (void)syncToMTCP:(id)sender
{
	PTLL; ui_sync_to_mtc_portmidi(); PTUL;
	update_sync_menu();
}

- (void)syncToNone:(id)sender
{
	PTLL; ui_sync_none(); PTUL;
}

@end

static void gl_make_current() {
	NSOpenGLContext* context = [osx_glview openGLContext];
	[context makeCurrentContext];
}

static void gl_swap_buffers() {
	glSwapAPPLE();
}

void gl_newsrc () {
	pthread_mutex_lock(&osx_vbuf_lock);
	free(osx_vbuf);
	osx_vbuf_size = video_buffer_size();
	if (osx_vbuf_size > 0) {
		osx_vbuf = malloc(osx_vbuf_size * sizeof(uint8_t));
		gl_reallocate_texture(movie_width, movie_height);
	}
	pthread_mutex_unlock(&osx_vbuf_lock);
}

static void makeAppMenu(void) {
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

	fileOpen = [fileMenu addItemWithTitle:@"Open" action:@selector(openVideo:) keyEquivalent:@"o"];

	if (interaction_override&OVR_LOADFILE) {
		[fileOpen setEnabled:NO];
	} else {
		[fileOpen setEnabled:YES];
	}

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

	syncJACK = [syncMenu addItemWithTitle:@"JACK" action:@selector(syncToJack:) keyEquivalent:@"j"];
#ifdef HAVE_LTC
	syncLTC  = [syncMenu addItemWithTitle:@"LTC" action:@selector(syncToLTC:) keyEquivalent:@"l"];
#endif
#ifdef HAVE_JACKMIDI
	syncMTCJ  = [syncMenu addItemWithTitle:@"MTC (jackmidi)" action:@selector(syncToMTCJ:) keyEquivalent:@"m"];
#endif
#ifdef HAVE_PORTMIDI
	syncMTCP  = [syncMenu addItemWithTitle:@"MTC (portmidi)" action:@selector(syncToMTCP:) keyEquivalent:@"p"];
#endif
	syncNone = [syncMenu addItemWithTitle:@"None" action:@selector(syncToNone:) keyEquivalent:@""];
	[syncNone setEnabled:NO];

	syncMenuItem = [[NSMenuItem alloc] initWithTitle:@"Sync" action:nil keyEquivalent:@""];
	[syncMenuItem setSubmenu:syncMenu];
	[[NSApp mainMenu] addItem:syncMenuItem];
	[syncMenu release];
	[syncMenuItem release];

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

static void osx_close_window() {
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

	gl_init();
	gl_resize(ffctv_width, ffctv_height);
	if (gl_reallocate_texture(movie_width, movie_height)) {
		osx_close_window();
		return -1;
	}
	gl_newsrc();

	[window setIsVisible:YES];

	if (start_fullscreen) { gl_set_fullscreen(1); }
	if (start_ontop) { gl_set_ontop(1); }
	return 0;
}

static void osx_post_event(int data1) {
    NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0,0)
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
		usleep(10000);
	}
	if (!pool) pool = [[NSAutoreleasePool alloc] init];
	if (osxgui_status > 0) return 0; // already open
	osx_post_event(1);
	while (osxgui_status == 0) {
		usleep(10000);
	}
	return osxgui_status > 0 ? 0 : -1;
}

void osx_shutdown() {
	if (pool && osxgui_status != -1000) {
		osx_post_event(0);
	}
	if (pool) {
		[pool release]; pool = NULL;
	}
}

void gl_close_window() {
	if (osxgui_status <= 0) {
		return;
	}
	osx_post_event(2);
	while (osxgui_status) {
		usleep(10000);
	}
	free(osx_vbuf); osx_vbuf = NULL;
	osx_vbuf_size = 0;
}

void gl_handle_events () {
	pthread_mutex_unlock(&osxgui_sync); // XXX TODO first-time lock
	while (sync_sem) {
		sched_yield();
	}
	pthread_mutex_lock(&osxgui_sync);
	osx_post_event(0); // TODO only on remote msg or similar event
}

void osx_main () {
	[NSAutoreleasePool new];
	[NSApplication sharedApplication];
	makeAppMenu();
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
							if (osx_open_window()) {
								osxgui_status = -1;
							} else {
								osxgui_status = 1;
							}
						}
						break;
					case 2:
						if (osxgui_status > 0) {
							osx_close_window();
							osxgui_status = 0;
						}
						break;
					case 3:
						pthread_mutex_lock(&osx_vbuf_lock);
						xjglExpose(osx_vbuf);
						pthread_mutex_unlock(&osx_vbuf_lock);
						[osx_glview setNeedsDisplay: YES];
						break;
					default:
						break;
				}
				break;
			}

			[NSApp sendEvent: event];
		}
		while(event != nil && loop_flag);

		// TODO: use a timer, and callbacks,
		// currently triggered by osx_post_event() from gl_handle_events()
		static int periodic_sync = 5;
		if (--periodic_sync == 0) {
			periodic_sync = framerate * 50;
			update_sync_menu();
			UpdateSystemActivity(1 /*UsrActivity*/);
			// TODO use a one time call
			// IOPMAssertionCreateWithName() NoDisplaySleepAssertion etc.
		} else if (periodic_sync % (int)(2 * framerate) == 0) {
			update_sync_menu();
		}
	}
	if (osxgui_status > 0) {
		osx_close_window();
		osxgui_status = 0;
	}
	[NSAutoreleasePool release];
	osxgui_status = -1000;
}


void gl_render (uint8_t *buffer) {
	if (!osx_vbuf) return;
	pthread_mutex_lock(&osx_vbuf_lock);
	memcpy(osx_vbuf, buffer, osx_vbuf_size * sizeof(uint8_t));
	pthread_mutex_unlock(&osx_vbuf_lock);
	[osx_glview setNeedsDisplay: YES];
	//osx_post_event(3); // explicit expose, blocks
}


void gl_resize (unsigned int x, unsigned int y) {
	[osx_window setContentSize:NSMakeSize(x, y) ];
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

static NSRect nofs_frame;

void gl_set_fullscreen (int action) {
	// TODO check if we need performSelectorOnMainThread
	if (action==2) _gl_fullscreen^=1;
	else _gl_fullscreen = action ? 1 : 0;
	if (_gl_fullscreen) {
		nofs_frame = [osx_window frame];
#if 0
		[osx_window setLevel:NSFloatingWindowLevel + 1];
		NSRect mainDisplayRect = [[NSScreen mainScreen] frame];
		NSRect viewRect = NSMakeRect(0.0, 0.0, mainDisplayRect.size.width, mainDisplayRect.size.height);
		[osx_window setFrame:viewRect display:YES animate:YES];
		//[osx_window setStyleMask:NSBorderlessWindowMask];
#else
		[osx_glview setFullScreen:YES];
#endif
	} else {
#if 0
		gl_set_ontop(_gl_ontop);
		[osx_window setFrame:nofs_frame display:YES animate:YES];
		//[osx_window setStyleMask:(NSClosableWindowMask | NSTitledWindowMask | NSResizableWindowMask)];
		[NSApplication setPresentationOptions:NSApplicationPresentationDefault];
#else
		[osx_glview setFullScreen:NO];
#endif
	}
}

void gl_mousepointer (int action) {
	if (action==2) hide_mouse^=1;
	else hide_mouse = action ? 1 : 0;

	if (hide_mouse) {
		CGDisplayHideCursor(kCGDirectMainDisplay);
	} else {
		CGDisplayShowCursor(kCGDirectMainDisplay);
	}
}

int gl_get_ontop () {
return _gl_ontop;
}
int gl_get_fullscreen () {
return _gl_fullscreen;
}
#endif
