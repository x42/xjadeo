/* xjadeo - openGL display for Windows
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
#if (defined HAVE_GL && defined PLATFORM_WINDOWS)

#include <pthread.h>
#include <windowsx.h>
#include "icons/xjadeo8_ico.h"

#ifndef WM_MOUSEWHEEL
# define WM_MOUSEWHEEL 0x020A
#endif
#ifndef TPM_NOANIMATION
static const UINT TPM_NOANIMATION = 0x4000L;
#endif

#define XJ_CLOSE_MSG  (WM_USER + 50)
#define XJ_CLOSE_WIN  (WM_USER + 49)
#define XJ_FULLSCREEN (WM_USER + 10)

static pthread_t wingui_thread;
static pthread_mutex_t wingui_sync = PTHREAD_MUTEX_INITIALIZER;
volatile int wingui_thread_status = 0;

static pthread_mutex_t win_vbuf_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t          win_vbuf_size = 0;
static uint8_t        *win_vbuf = NULL;

void xapi_open(void *d);
void xapi_close (void *d);

static HWND  _gl_hwnd;
static HDC   _gl_hdc;
static HGLRC _gl_hglrc;
static WNDCLASSEX _gl_wc;

static HCURSOR hCurs_dflt = NULL;
static HCURSOR hCurs_none = NULL;
static RECT fs_rect = {0, 0, 320, 240};
static HICON xjadeo_icon = NULL;
static int winFlags;

static void gl_make_current() {
	; //wglMakeCurrent(_gl_hdc, _gl_hglrc);
}

static void gl_swap_buffers() {
	SwapBuffers(_gl_hdc);
}

void gl_newsrc () {
	pthread_mutex_lock(&win_vbuf_lock);
	free(win_vbuf);
	win_vbuf_size = video_buffer_size();
	if (win_vbuf_size > 0) {
		win_vbuf = malloc(win_vbuf_size * sizeof(uint8_t));
		gl_reallocate_texture(movie_width, movie_height);
	}
	pthread_mutex_unlock(&win_vbuf_lock);
}

static volatile uint8_t sync_sem;
static void gl_sync_lock()   { sync_sem = 1; pthread_mutex_lock(&wingui_sync); }
static void gl_sync_unlock() { sync_sem = 0; pthread_mutex_unlock(&wingui_sync); }

#define PTLL gl_sync_lock()
#define PTUL gl_sync_unlock();

static uint8_t context_menu_visible = 0;

#ifdef WINMENU

enum wMenuId {
	mLoad = 1,
	mClose,
	mQuit,

	mSyncJack,
	mSyncLTC,
	mSyncMTCJACK,
	mSyncMTCPort,
	mSyncNone,

	mSize50,
	mSize100,
	mSize150,
	mSizeInc,
	mSizeDec,
	mSizeAspect,
	mSizeLetterbox,
	mWinOnTop,
	mWinFullScreen,
	mWinMouseVisible,

	mOsdTC,
	mOsdVtcNone,
	mOsdVtcTc,
	mOsdVtcFn,
	mOsdPosition,
	mOsdOffsetNone,
	mOsdOffsetFN,
	mOsdOffsetTC,
	mOsdBox,
	mOsdFileInfo,
	mOsdGeometry,
	mOsdClear,

	mOffsetZero,
	mOffsetPF,
	mOffsetMF,
	mOffsetPM,
	mOffsetMM,
	mOffsetPH,
	mOffsetMH,

	mJackPlayPause,
	mJackPlay,
	mJackStop,
	mJackRewind,

	mRecent,
};

static const char *win_basename(const char *fn) {
	if (!fn) return NULL;
	size_t l = strlen(fn);
	if (l < 1) return fn;
	if (fn[l] == '/' || fn[l] == '\\') --l;
	while (l > 0 && fn[l] != '/' && fn[l] != '\\') --l;
	return &fn[++l];
}

static void open_context_menu(HWND hwnd, int x, int y) {
	HMENU hMenu = CreatePopupMenu();
	HMENU hSubMenuFile = CreatePopupMenu();
	HMENU hSubMenuSync = CreatePopupMenu();
	HMENU hSubMenuSize = CreatePopupMenu();
	HMENU hSubMenuOSD  = CreatePopupMenu();
	HMENU hSubMenuOffs = CreatePopupMenu();
	HMENU hSubMenuJack = CreatePopupMenu();
	HMENU hSubRecent   = CreatePopupMenu();

	AppendMenu(hSubMenuSync, MF_STRING, mSyncJack, "JACK");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncLTC, "LTC");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncMTCJACK, "MTC (JACK)");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncMTCPort, "MTC (PortMidi)");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncNone, "None");

	AppendMenu(hSubMenuSize, MF_STRING, mSize50, "50%");
	AppendMenu(hSubMenuSize, MF_STRING, mSize100, "100%\t .");
	AppendMenu(hSubMenuSize, MF_STRING, mSize150, "150%");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mSizeDec, "-20%\t<");
	AppendMenu(hSubMenuSize, MF_STRING, mSizeInc, "+20%\t>");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mSizeAspect, "Reset Aspect\t ,");
	AppendMenu(hSubMenuSize, MF_STRING, mSizeLetterbox, "Retain Aspect (Letterbox)\t L");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mWinOnTop, "On Top\t A");
	AppendMenu(hSubMenuSize, MF_STRING, mWinFullScreen, "Full Screen\t F");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mWinMouseVisible, "Mouse Cursor\t M");

	unsigned int nfo_flags = 0;
	if (movie_height < OSD_MIN_NFO_HEIGHT) {
		nfo_flags |= MF_DISABLED | MF_GRAYED;
	}

	AppendMenu(hSubMenuOSD, MF_STRING, mOsdTC, "External Timecode\t S");
	AppendMenu(hSubMenuOSD, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdVtcNone, "VTC Off\t V");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdVtcTc, "VTC Timecode");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdVtcFn, "VTC Frame Number");
	AppendMenu(hSubMenuOSD, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdOffsetNone, "Offset Off\t O");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdOffsetTC, "Offset Timecode");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdOffsetFN, "Offset Frame Number");
	AppendMenu(hSubMenuOSD, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuOSD, MF_STRING | nfo_flags, mOsdFileInfo, "Time Info\t I");
	AppendMenu(hSubMenuOSD, MF_STRING | nfo_flags, mOsdGeometry, "Geometry\t G");
	AppendMenu(hSubMenuOSD, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdBox, "Background\t B");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdPosition, "Swap Position\t P");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdClear, "CLear All\t Shift+C");

	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetZero, "Reset\t \\");
	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetPF,   "+1 Frame\t +");
	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetMF,   "-1 Frame\t -");
	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetPM,   "+1 Minute\t }");
	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetMM,   "-1 Minute\t {");
	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetPH,   "+1 Hour");
	AppendMenu(hSubMenuOffs, MF_STRING, mOffsetMH,   "-1 Hour");

	AppendMenu(hSubMenuJack, MF_STRING, mJackPlayPause, "Play/Pause\t Space");
	AppendMenu(hSubMenuJack, MF_STRING, mJackPlay, "Play");
	AppendMenu(hSubMenuJack, MF_STRING, mJackStop, "Stop");
	AppendMenu(hSubMenuJack, MF_STRING, mJackRewind, "Rewind\t Backspace");

	/* update menu items */
	switch (ui_syncsource()) {
		case SYNC_JACK:
			CheckMenuItem(hSubMenuSync, mSyncJack, MF_CHECKED | MF_BYCOMMAND);
			break;
		case SYNC_LTC:
			CheckMenuItem(hSubMenuSync, mSyncLTC, MF_CHECKED | MF_BYCOMMAND);
			break;
		case SYNC_MTC_JACK:
			CheckMenuItem(hSubMenuSync, mSyncMTCJACK, MF_CHECKED | MF_BYCOMMAND);
			break;
		case SYNC_MTC_PORTMIDI:
			CheckMenuItem(hSubMenuSync, mSyncMTCPort, MF_CHECKED | MF_BYCOMMAND);
			break;
		default:
			CheckMenuItem(hSubMenuSync, mSyncNone, MF_CHECKED | MF_BYCOMMAND);
			break;
	}

	if (OSD_mode&OSD_SMPTE) {
		CheckMenuItem(hSubMenuOSD, mOsdTC, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_FRAME) {
		CheckMenuItem(hSubMenuOSD, mOsdVtcFn, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_VTC) {
		CheckMenuItem(hSubMenuOSD, mOsdVtcTc, MF_CHECKED | MF_BYCOMMAND);
	}
	if (!(OSD_mode&(OSD_FRAME|OSD_VTC))) {
		CheckMenuItem(hSubMenuOSD, mOsdVtcNone, MF_CHECKED | MF_BYCOMMAND);
	}
	if (!(OSD_mode&(OSD_OFFF|OSD_OFFS))) {
		CheckMenuItem(hSubMenuOSD, mOsdOffsetNone, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_OFFF) {
		CheckMenuItem(hSubMenuOSD, mOsdOffsetFN, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_OFFS) {
		CheckMenuItem(hSubMenuOSD, mOsdOffsetTC, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_BOX) {
		CheckMenuItem(hSubMenuOSD, mOsdBox, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_NFO) {
		CheckMenuItem(hSubMenuOSD, mOsdFileInfo, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_GEO) {
		CheckMenuItem(hSubMenuOSD, mOsdGeometry, MF_CHECKED | MF_BYCOMMAND);
	}

	if (Xgetletterbox()) {
		CheckMenuItem(hSubMenuSize, mSizeLetterbox, MF_CHECKED | MF_BYCOMMAND);
	}
	if (Xgetontop()) {
		CheckMenuItem(hSubMenuSize, mWinOnTop, MF_CHECKED | MF_BYCOMMAND);
	}
	if (Xgetfullscreen()) {
		CheckMenuItem(hSubMenuSize, mWinFullScreen, MF_CHECKED | MF_BYCOMMAND);
	}
	if (!Xgetmousepointer()) {
		CheckMenuItem(hSubMenuSize, mWinMouseVisible, MF_CHECKED | MF_BYCOMMAND);
	}

	// built top-level w/o ModifyMenu
	unsigned int flags_open = 0;
	unsigned int flags_close = 0;
	unsigned int flags_quit = 0;
	unsigned int flags_sync = 0;
	unsigned int flags_offs = 0;
	unsigned int flags_jack = 0;
	unsigned int flags_recent = 0;
	unsigned int recent_cnt = x_fib_recent_count ();
	if (!have_open_file()) {
		flags_close = MF_DISABLED | MF_GRAYED;
	}
	if (ui_syncsource() != SYNC_JACK || (interaction_override&OVR_JCONTROL)) {
		flags_jack = MF_DISABLED | MF_GRAYED;
	}
	if (interaction_override & OVR_MENUSYNC) {
		flags_sync = MF_DISABLED | MF_GRAYED;
	}
	if (interaction_override & OVR_LOADFILE) {
		flags_open = MF_DISABLED | MF_GRAYED;
		flags_close = MF_DISABLED | MF_GRAYED;
		flags_recent = MF_DISABLED | MF_GRAYED;
	}
	if ((interaction_override&OVR_AVOFFSET) != 0 ) {
		flags_offs = MF_DISABLED | MF_GRAYED;
	}
	if ((interaction_override&OVR_QUIT_KEY) != 0 ) {
		flags_quit = MF_DISABLED | MF_GRAYED;
	}
	if (recent_cnt == 0) {
		flags_recent = MF_DISABLED | MF_GRAYED;
	}

	AppendMenu(hSubMenuFile, MF_STRING | flags_open, mLoad, "Open\t Ctrl+O");
	AppendMenu(hSubMenuFile, MF_STRING | flags_close, mClose, "Close\t Ctrl+W");
	AppendMenu(hSubMenuFile, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuFile, MF_STRING | MF_POPUP | flags_recent, (UINT_PTR)hSubRecent, "Recent");
	AppendMenu(hSubMenuFile, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuFile, MF_STRING | flags_quit, mQuit,  "Quit\t Ctrl+Q");

	AppendMenu(hMenu, MF_STRING | MF_DISABLED, 0, "XJadeo " VERSION);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	if (!(flags_open && flags_quit))
		AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenuFile, "File");
	AppendMenu(hMenu, MF_STRING | MF_POPUP | flags_sync, (UINT_PTR)hSubMenuSync, "Sync");
	AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenuSize, "Display");
	AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenuOSD, "OSD");
	AppendMenu(hMenu, MF_STRING | MF_POPUP | flags_offs, (UINT_PTR)hSubMenuOffs, "Offset");
	AppendMenu(hMenu, MF_STRING | MF_POPUP | flags_jack, (UINT_PTR)hSubMenuJack, "Transport");

	/* recently used*/
	int i;
	for (i = 0; i < recent_cnt && i < 8; ++i) {
		if (!x_fib_recent_at (i)) break;
		char *tmp = strdup (x_fib_recent_at (i));
		AppendMenu(hSubRecent, MF_STRING , mRecent + i, win_basename (tmp));
		free(tmp);
	}

	/* and go */
	context_menu_visible = 1;
	TrackPopupMenuEx(hMenu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION,
			x, y, hwnd, NULL);
	context_menu_visible = 0;

	DestroyMenu (hSubMenuOSD);
	DestroyMenu (hSubMenuJack);
	DestroyMenu (hSubMenuSize);
	DestroyMenu (hSubMenuSync);
	DestroyMenu (hMenu);
}

static void win_load_file (HWND hwnd) {
	if (interaction_override & OVR_LOADFILE) return;
	char fn[1024] = "";
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = fn;
	ofn.nMaxFile = 1024;
	ofn.lpstrTitle = "xjadeo - Load Video File";
	ofn.lpstrFilter = "Video Files\0"
		"*.avi;*.mov;*.mkv;*.mpg;*.vob;*.ogg;*.ogv;*.mp4;*.mpeg;*.webm;*.flv;*.asf;*.avs;*.dts;*.m4v;*.dv;*.dirac;*.h264;"
		"*.wmv;*.mtsC;*.ts;*.264\0"
		"All\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_NONETWORKBUTTON | OFN_HIDEREADONLY | OFN_READONLY;
	//ofn.lpstrInitialDir = ;

	if (GetOpenFileName (&ofn)) {
#if 0
		printf("Load: '%s'\n", fn);
#endif
		PTLL;
		xapi_open(fn);
		PTUL;
	}
}

static void win_close_video () {
	if (interaction_override & OVR_LOADFILE) return;
	PTLL;
	xapi_close(NULL);
	PTUL;
}

static void win_quit_xjadeo () {
	if (interaction_override & OVR_QUIT_KEY) return;
	loop_flag=0;
}

static void win_handle_menu(HWND hwnd, enum wMenuId id) {
	switch(id) {
		case mLoad:            win_load_file(hwnd); break;
		case mClose:           win_close_video(); break;
		case mQuit:            win_quit_xjadeo(); break;
		case mSyncJack:        PTLL; ui_sync_to_jack(); PTUL; break;
		case mSyncLTC:         PTLL; ui_sync_to_ltc(); PTUL; break;
		case mSyncMTCJACK:     PTLL; ui_sync_to_mtc_jack(); PTUL; break;
		case mSyncMTCPort:     PTLL; ui_sync_to_mtc_portmidi(); PTUL; break;
		case mSyncNone:        PTLL; ui_sync_none(); PTUL; break;
		case mSize50:          XCresize_percent(50); break;
		case mSize100:         XCresize_percent(100); break;
		case mSize150:         XCresize_percent(150); break;
		case mSizeInc:         XCresize_scale( 1); break;
		case mSizeDec:         XCresize_scale(-1); break;
		case mSizeAspect:      XCresize_aspect(0); break;
		case mSizeLetterbox:   Xletterbox(2); break;
		case mWinOnTop:        Xontop(2); break;
		case mWinFullScreen:   Xfullscreen(2); break;
		case mWinMouseVisible: Xmousepointer(2); break;
		case mOsdVtcNone:      PTLL; ui_osd_vtc_off(); PTUL; break;
		case mOsdVtcTc:        PTLL; ui_osd_vtc_tc(); PTUL; break;
		case mOsdVtcFn:        PTLL; ui_osd_vtc_fn(); PTUL; break;
		case mOsdTC:           PTLL; ui_osd_tc(); PTUL; break;
		case mOsdPosition:     PTLL; ui_osd_permute(); PTUL; break;
		case mOsdOffsetNone:   PTLL; ui_osd_offset_none(); PTUL; break;
		case mOsdOffsetFN:     PTLL; ui_osd_offset_fn(); PTUL; break;
		case mOsdOffsetTC:     PTLL; ui_osd_offset_tc(); PTUL; break;
		case mOsdBox:          PTLL; ui_osd_box(); PTUL; break;
		case mOsdFileInfo:     PTLL; ui_osd_fileinfo(); PTUL; break;
		case mOsdGeometry:     PTLL; ui_osd_geo(); PTUL; break;
		case mOsdClear:        PTLL; ui_osd_clear(); PTUL; break;
		case mOffsetZero:      PTLL; XCtimeoffset( 0, 0); PTUL; break;
		case mOffsetPF:        PTLL; XCtimeoffset( 1, 0); PTUL; break;
		case mOffsetMF:        PTLL; XCtimeoffset(-1, 0); PTUL; break;
		case mOffsetPM:        PTLL; XCtimeoffset( 2, 0); PTUL; break;
		case mOffsetMM:        PTLL; XCtimeoffset(-2, 0); PTUL; break;
		case mOffsetPH:        PTLL; XCtimeoffset( 3, 0); PTUL; break;
		case mOffsetMH:        PTLL; XCtimeoffset(-3, 0); PTUL; break;
		case mJackPlayPause:   jackt_toggle(); break;
		case mJackPlay:        jackt_start(); break;
		case mJackStop:        jackt_stop(); break;
		case mJackRewind:      jackt_rewind(); break;
		default:
			if (id >= mRecent && !(interaction_override & OVR_LOADFILE)) {
				const char *fn = x_fib_recent_at (id - mRecent);
				if (fn) {
					PTLL; xapi_open((void*) fn); PTUL;
				}
			}
	}
}
#endif

static void win_set_fullscreen () {
	if (_gl_fullscreen) {
		MONITORINFO mi = { sizeof(mi) };
		GetWindowRect(_gl_hwnd, &fs_rect);
#if 0
		printf("GR %d %d %d %d: %dx%d\n", fs_rect.top, fs_rect.left, fs_rect.bottom, fs_rect.right,
				fs_rect.bottom - fs_rect.top, fs_rect.right - fs_rect.left);
#endif
		winFlags = WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
		SetWindowLongPtr(_gl_hwnd, GWL_STYLE, winFlags);

		if (GetMonitorInfo(MonitorFromWindow(_gl_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowPos (_gl_hwnd,
					HWND_TOPMOST,
					mi.rcMonitor.left, mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED);
			if (mi.dwFlags & MONITORINFOF_PRIMARY) {
				ChangeDisplaySettings(NULL, CDS_FULLSCREEN);
			}
		} else {
			SetWindowPos (_gl_hwnd,
					HWND_TOPMOST,
					0, 0,
					GetSystemMetrics(SM_CXSCREEN /*SM_CXFULLSCREEN*/),
					GetSystemMetrics(SM_CYSCREEN /*SM_CYFULLSCREEN*/),
					SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED);
			ChangeDisplaySettings(NULL, CDS_FULLSCREEN);
		}
	} else {
		winFlags = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		SetWindowPos (_gl_hwnd,
				_gl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
				fs_rect.left, fs_rect.top, 0, 0,
				SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOCOPYBITS);
		SetWindowLongPtr(_gl_hwnd, GWL_STYLE, winFlags);
		SetWindowPos (_gl_hwnd,
				_gl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
				fs_rect.left, fs_rect.top,
				fs_rect.right - fs_rect.left, fs_rect.bottom - fs_rect.top,
				SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED);
		ChangeDisplaySettings(NULL, CDS_RESET);
	}
	ShowWindow (_gl_hwnd, SW_SHOW);
	force_redraw = 1;
}

#if 0
static int check_wgl_extention(const char *ext) {
	if (!ext || strchr(ext, ' ') || *ext == '\0') {
		return 0;
	}
	const char *exts = (const char*) glGetString(GL_EXTENSIONS);
	if (!exts) {
		return 0;
	}

	const char *start = exts;
	while (1) {
		const char *tmp = strstr(start, ext);
		if (!tmp) break;
		const char *end = tmp + strlen(ext);
		if (tmp == start || *(tmp - 1) == ' ')
			if (*end == ' ' || *end == '\0') return 1;
		start = end;
	}
	return 0;
}

static void *win_glGetProcAddress(const char* proc) {
	void * func = NULL;
	static int initialized = 0;
	static HMODULE handle;
	static void * (*wgl_getProcAddress)(const char *proc);

	if (!initialized) {
		initialized = 1;
		handle = GetModuleHandle("OPENGL32.DLL");
		wgl_getProcAddress =
			(void* (*)(const char*))
			GetProcAddress(handle, "wglGetProcAddress");
	}

	if (wgl_getProcAddress) {
		func = wgl_getProcAddress(proc);
	}
	if (!func) {
		func = GetProcAddress(NULL, proc);
	}
	return func;
}
#endif

static LRESULT
handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
#if 0
	if (message != 0x0200 && message != 0x0113 && message != 0x0020 && message != 0x0084 && message != 0x0121)
	printf("MSG: %04X  @ %p\n", message, (void*)hwnd);
#endif
	switch (message) {
		case WM_CREATE:
		case WM_SHOWWINDOW:
		case WM_SIZE:
			if (hwnd == _gl_hwnd)
			{
				RECT rect;
				GetClientRect(_gl_hwnd, &rect);
				gl_reshape (rect.right - rect.left, rect.bottom - rect.top);
				InvalidateRect(_gl_hwnd, NULL, 0);
			}
		case WM_SIZING:
			xjglExpose(win_vbuf);
			break;
		case WM_PAINT:
			if (hwnd != _gl_hwnd)
				return DefWindowProc(hwnd, message, wParam, lParam);
			else
			{
				pthread_mutex_lock(&win_vbuf_lock);
				xjglExpose(win_vbuf);
				pthread_mutex_unlock(&win_vbuf_lock);
			}
			ValidateRect(hwnd, NULL);
			break;
#if 0
		case WM_ERASEBKGND:
			if (hwnd != _gl_hwnd)
				return DefWindowProc(hwnd, message, wParam, lParam);
			break;
#endif
		case WM_MOUSEMOVE:
			if (osd_seeking && ui_syncsource() == SYNC_NONE && OSD_mode & OSD_POS) {
				const float sk = calc_slider (GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				if (sk >= 0) {
					// no lock, userFrame is queried atomically
					// and transport is disconnected when osd_seeking is set
					ui_sync_manual (sk);
				}
			}
			break;
		case WM_LBUTTONDOWN:
			if (ui_syncsource() == SYNC_NONE && OSD_mode & OSD_POS) {
				const float sk = calc_slider (GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				if (sk >= 0) {
					PTLL;
					ui_sync_manual (sk);
					osd_seeking = 1;
					force_redraw = 1;
					PTUL;
				}
			}
			break;
		case WM_LBUTTONUP:
			if (osd_seeking) {
				PTLL;
				osd_seeking = 0;
				force_redraw = 1;
				PTUL;
			} else
				xjglButton(1);
			break;
		case WM_MBUTTONUP:
			xjglButton(2);
			break;
		case WM_RBUTTONUP:
#ifdef WINMENU
			if (1) {
				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				ClientToScreen(hwnd, &pt);
				open_context_menu(hwnd, pt.x, pt.y);
			}
			else
#endif
				xjglButton(3);
			break;
#ifdef WINMENU
		case WM_CONTEXTMENU:
			{
				int x0, y0;
				gl_get_window_pos(&x0, &y0);
				open_context_menu(hwnd, x0 + _gl_width * .25, y0 + _gl_height * .25);
			}
			break;
		case WM_COMMAND:
			win_handle_menu(hwnd, LOWORD(wParam));
			break;
#endif
		case WM_MOUSEWHEEL:
			gl_sync_lock();
			xjglButton((short)HIWORD(wParam) > 0 ? 4 : 5);
			gl_sync_unlock();
			break;
		case WM_KEYDOWN:
			{
				static BYTE kbs[256];
				if (GetKeyboardState(kbs) != FALSE) {
					char lb[2];
					UINT scanCode = (lParam >> 8) & 0xFFFFFF00;

					if ( 1 == ToAscii(wParam, scanCode, kbs, (LPWORD)lb, 0)) {
						const char buf [2] = {(char)lb[0] , 0};
						if ((GetKeyState(VK_CONTROL) & GetKeyState('O') & 0x8000)
								&& (interaction_override & OVR_LOADFILE) == 0) {
							win_load_file (hwnd);
						}
						else if ((GetKeyState(VK_CONTROL) & GetKeyState('W') & 0x8000)
								&& (interaction_override & OVR_LOADFILE) == 0) {
								win_close_video ();
						}
						else if ((GetKeyState(VK_CONTROL) & GetKeyState('Q') & 0x8000)
								&& (interaction_override&OVR_QUIT_KEY) == 0) {
							win_quit_xjadeo ();
						}
						else if (!strcmp(buf, "f")) {
							// direct fullscreen handling
							_gl_fullscreen^=1;
							win_set_fullscreen ();
						} else {
							xjglKeyPress ((char)lb[0], buf);
						}
					}
				}
			}
			break;
		case XJ_FULLSCREEN:
			win_set_fullscreen();
			break;
		case WM_KEYUP:
			break;
		case WM_QUIT:
		case XJ_CLOSE_MSG:
			gl_sync_lock();
			if ((interaction_override&OVR_QUIT_WMG) == 0) {
				loop_flag = 0;
			}
			gl_sync_unlock();
			break;
		case WM_ENTERIDLE:
			if (context_menu_visible)
				xjglExpose(win_vbuf);
			return DefWindowProc(hwnd, message, wParam, lParam);
			break;
		case WM_EXITSIZEMOVE:
			force_redraw = 1;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}
static LRESULT CALLBACK
wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			PostMessage(hwnd, WM_SHOWWINDOW, TRUE, 0);
			return 0;
		case WM_CLOSE:
			PostMessage(hwnd, XJ_CLOSE_MSG, wParam, lParam);
			return 0;
		case WM_DESTROY:
			return 0;
		default:
			return handleMessage(hwnd, message, wParam, lParam);
	}
}

void win_close_window() {
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(_gl_hglrc);
	ReleaseDC(_gl_hwnd, _gl_hdc);
	DestroyWindow(_gl_hwnd);
	DestroyCursor(hCurs_none);
	UnregisterClass(_gl_wc.lpszClassName, NULL);
	DestroyIcon(xjadeo_icon);
}

static void *win_open_window (void *arg) {
	LPCTSTR MainWndClass = TEXT("xjadeo");
	int offset = LookupIconIdFromDirectory ((BYTE*)xjadeo_win_ico, TRUE);
	if (offset > 0) {
		BITMAPINFOHEADER *img = (BITMAPINFOHEADER *)(xjadeo_win_ico + offset );
		xjadeo_icon = CreateIconFromResource (
				(BYTE*)img, img->biSize,
				TRUE, 0x00030000);
	} else {
		xjadeo_icon = LoadImage(NULL, "xjadeo.ico", IMAGE_ICON,
				0, 0, LR_SHARED | LR_DEFAULTSIZE | LR_LOADFROMFILE) ;
	}

	hCurs_dflt = LoadCursor(NULL, IDC_ARROW);

	ZeroMemory(&_gl_wc, sizeof(WNDCLASSEX));
	_gl_wc.cbSize        = sizeof(WNDCLASSEX);
	_gl_wc.style         = 0;
	_gl_wc.lpfnWndProc   = (WNDPROC)&wndProc;
	_gl_wc.cbClsExtra    = 0;
	_gl_wc.cbWndExtra    = 0;
	_gl_wc.hInstance     = NULL;
	_gl_wc.hIcon         = xjadeo_icon ? xjadeo_icon : LoadIcon(NULL, IDI_APPLICATION);
	_gl_wc.hCursor       = hCurs_dflt;
	_gl_wc.hbrBackground = NULL; // (HBRUSH)GetStockObject(BLACK_BRUSH);
	_gl_wc.lpszMenuName  = NULL;
	_gl_wc.lpszClassName = MainWndClass;
	_gl_wc.hIconSm       = 0; // 16x16

	if (!RegisterClassEx(&_gl_wc)) {
		fprintf(stderr, "cannot regiser window class\n");
		MessageBox(NULL, TEXT("Error registering window class."), TEXT("Error"), MB_ICONERROR | MB_OK);
		wingui_thread_status = -1;
		pthread_exit (NULL);
		return (NULL);
	}

	_gl_width = ffctv_width;
	_gl_height = ffctv_height;

	winFlags = start_fullscreen ? (WS_POPUP | WS_CLIPCHILDREN) : WS_OVERLAPPEDWINDOW;
	RECT wr = { 0, 0, (long)_gl_width, (long)_gl_height };
	AdjustWindowRectEx(&wr, winFlags, FALSE, WS_EX_TOPMOST);

	SystemParametersInfo(SPI_SETDROPSHADOW, 0, FALSE, 0);
	SystemParametersInfo(SPI_SETMENUFADE, 0, FALSE, 0);
	SystemParametersInfo(SPI_SETMENUANIMATION, 0, FALSE, 0);

	_gl_hwnd = CreateWindowEx (
			0,
			MainWndClass, MainWndClass,
			winFlags,
			CW_USEDEFAULT, CW_USEDEFAULT,
			wr.right - wr.left, wr.bottom - wr.top,
			NULL, NULL, NULL, NULL);

	if (!_gl_hwnd) {
		fprintf(stderr, "cannot open window\n");
		MessageBox(NULL, TEXT("Error creating main window."), TEXT("Error"), MB_ICONERROR | MB_OK);
		UnregisterClass(_gl_wc.lpszClassName, NULL);
		DestroyIcon(xjadeo_icon);
		wingui_thread_status = -1;
		pthread_exit (NULL);
		return (NULL);
	}

	_gl_hdc = GetDC(_gl_hwnd);
	if (!_gl_hdc) {
		DestroyWindow(_gl_hwnd);
		UnregisterClass(_gl_wc.lpszClassName, NULL);
		DestroyIcon(xjadeo_icon);
		wingui_thread_status = -1;
		pthread_exit (NULL);
		return (NULL);
	}

	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize      = sizeof(pfd);
	pfd.nVersion   = 1;
	pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int format = ChoosePixelFormat(_gl_hdc, &pfd);
	if (format == 0) {
		fprintf(stderr, "Pixel Format is not supported.\n");
		ReleaseDC(_gl_hwnd, _gl_hdc);
		DestroyWindow(_gl_hwnd);
		UnregisterClass(_gl_wc.lpszClassName, NULL);
		DestroyIcon(xjadeo_icon);
		wingui_thread_status = -1;
		pthread_exit (NULL);
		return (NULL);
	}
	SetPixelFormat(_gl_hdc, format, &pfd);

	_gl_hglrc = wglCreateContext(_gl_hdc);
	if (!_gl_hglrc) {
		fprintf(stderr, "Cannot create openGL context.\n");
		ReleaseDC(_gl_hwnd, _gl_hdc);
		DestroyWindow(_gl_hwnd);
		UnregisterClass(_gl_wc.lpszClassName, NULL);
		DestroyIcon(xjadeo_icon);
		wingui_thread_status = -1;
		pthread_exit (NULL);
		return (NULL);
	}

	wglMakeCurrent(_gl_hdc, _gl_hglrc);

	if (start_ontop) { gl_set_ontop(1); }
	if (start_fullscreen) {
		_gl_fullscreen = 1;
		win_set_fullscreen();
	}

	BYTE ANDmaskCursor[16];
	BYTE XORmaskCursor[16];
	memset(ANDmaskCursor, 0xff, sizeof(ANDmaskCursor));
	memset(XORmaskCursor, 0, sizeof(ANDmaskCursor));
	hCurs_none = CreateCursor (
			(HINSTANCE)GetWindowLongPtr(_gl_hwnd, GWLP_HINSTANCE),
			2, 2,
			4, 4,
			ANDmaskCursor, XORmaskCursor);

	gl_init();

	if (gl_reallocate_texture(movie_width, movie_height)) {
		win_close_window ();
		wingui_thread_status = -1;
		pthread_exit (NULL);
		return (NULL);
	}
	gl_newsrc();

#if 0 // check for VBlank sync
	if (check_wgl_extention("WGL_EXT_swap_control")) {
		printf("WGL: have WGL_EXT_swap_control\n");
		BOOL (*wglSwapIntervalEXT)(int interval) =
			(BOOL (*)(int))
			win_glGetProcAddress("wglSwapIntervalEXT");
		if (wglSwapIntervalEXT) {
			wglSwapIntervalEXT(1);
			if (want_verbose)
				printf("WGL: use Vblank \n");
		}
	}
#endif

	// disable screensaver
	SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);

	//ShowWindow(_gl_hwnd, SW_HIDE);
	//ShowWindow(_gl_hwnd, SW_SHOW);
	ShowWindow(_gl_hwnd, WS_VISIBLE);
	ShowWindow(_gl_hwnd, SW_RESTORE);
	UpdateWindow(_gl_hwnd);

	wingui_thread_status = 1;

	MSG msg;
	while (wingui_thread_status) {
		if(GetMessage (&msg, 0, 0, 0) < 0) {
			break; // error
		}
		handleMessage(_gl_hwnd, msg.message, msg.wParam, msg.lParam);
		if (_gl_reexpose) {
			pthread_mutex_lock(&win_vbuf_lock);
			_gl_reexpose = false;
			xjglExpose(win_vbuf);
			pthread_mutex_unlock(&win_vbuf_lock);
		}
	}

	if (wingui_thread_status) {
		// This can't really happen, can it?
		MessageBox(NULL, TEXT("The Windows application event loop was terminated unexpectedly."),
				TEXT("Error"), MB_ICONERROR | MB_OK);
		loop_flag = 0; // Irregular exit
	}

	win_close_window();
	pthread_exit (NULL);
	return (NULL);
}

int gl_open_window () {
	// use a dedicated thread for the windows UI
	// to work around blocking behaviour on window
	// resize and context-menu access.
	if (wingui_thread_status) return -1;
	if (pthread_create (&wingui_thread, NULL, win_open_window, NULL)) {
		return -1;
	}
	while (wingui_thread_status == 0) {
		Sleep(1);
	}
	if (wingui_thread_status < 0) {
		wingui_thread_status = 0;
		pthread_join (wingui_thread, NULL);
	}
	return wingui_thread_status > 0 ? 0 : -1;
}

void gl_close_window() {
	if (wingui_thread_status == 0) return;
	wingui_thread_status = 0;
	PostMessage(_gl_hwnd, XJ_CLOSE_WIN, 0, 0); // wake up GetMessage
	pthread_join (wingui_thread, NULL);

	free(win_vbuf); win_vbuf = NULL;
	win_vbuf_size = 0;
}

void gl_handle_events () {
	pthread_mutex_unlock(&wingui_sync); // XXX TODO first-time lock
	while (sync_sem) {
		sched_yield();
	}
	// TODO explicitly wake up potential lock-holders
	pthread_mutex_lock(&wingui_sync);
}

void gl_render (uint8_t *buffer) {
	if (!win_vbuf) return;
	pthread_mutex_lock(&win_vbuf_lock);
	memcpy(win_vbuf, buffer, win_vbuf_size * sizeof(uint8_t));
	pthread_mutex_unlock(&win_vbuf_lock);
	InvalidateRect(_gl_hwnd, NULL, 0);
}

void gl_resize (unsigned int x, unsigned int y) {
	if (_gl_fullscreen) { return; }
	RECT wr = { 0, 0, (long)x, (long)y };
	AdjustWindowRectEx(&wr, winFlags, FALSE, WS_EX_TOPMOST);

	SetWindowPos (_gl_hwnd,
			_gl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
			0, 0, wr.right - wr.left, wr.bottom - wr.top,
			SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOREPOSITION);
	UpdateWindow(_gl_hwnd); // calls gl_reshape()
}

void gl_get_window_size (unsigned int *w, unsigned int *h) {
	*w = _gl_width;
	*h = _gl_height;
}

void gl_position (int x, int y) {
	SetWindowPos (_gl_hwnd,
			_gl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
			x, y, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

void gl_get_window_pos (int *rx, int *ry) {
	RECT rect;
	GetWindowRect(_gl_hwnd, &rect);
	*rx = rect.left;
	*ry = rect.top;
}

void gl_set_ontop (int action) {
	if (action==2) _gl_ontop ^= 1;
	else _gl_ontop = action ? 1 : 0;

	SetWindowPos (_gl_hwnd,
			_gl_ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
			0, 0, 0, 0, (_gl_ontop ? 0 : SWP_NOACTIVATE) | SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE);
}


void gl_set_fullscreen (int action) {
	if (action==2) _gl_fullscreen^=1;
	else _gl_fullscreen = action ? 1 : 0;
	PostMessage(_gl_hwnd, XJ_FULLSCREEN, 0, 0);
}

void gl_mousepointer (int action) {
	if (action==2) hide_mouse ^= 1;
	else hide_mouse = action ? 1 : 0;
	if (hide_mouse) {
		SetClassLong(_gl_hwnd, GCL_HCURSOR, (LONG)hCurs_none);
		SetCursor(hCurs_none);
	} else {
		SetClassLong(_gl_hwnd, GCL_HCURSOR, (LONG)hCurs_dflt);
		SetCursor(hCurs_dflt);
	}
}

int  gl_get_ontop () {
	return _gl_ontop;
}

int  gl_get_fullscreen () {
	return _gl_fullscreen;
}

#endif
