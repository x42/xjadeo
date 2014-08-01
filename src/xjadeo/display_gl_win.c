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

#include <windowsx.h>
#include "icons/xjadeo-color-ico.h"

#ifndef WM_MOUSEWHEEL
# define WM_MOUSEWHEEL 0x020A
#endif
#define XJ_CLOSE_MSG (WM_USER + 50)

void xapi_open(void *d);

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
	wglMakeCurrent(_gl_hdc, _gl_hglrc);
}

static void gl_swap_buffers() {
	SwapBuffers(_gl_hdc);
}


#ifdef WINMENU

enum wMenuId {
	mLoad = 1,

	mSyncJack,
	mSyncLTC,
	mSyncMTCJACK,
	mSyncMTCPort,
	mSyncNone,

	mSize50,
	mSize100,
	mSize150,
	mSizeAspect,
	mSizeLetterbox,
	mWinOnTop,
	mWinFullScreen,
	mWinMouseVisible,

	mOsdFN,
	mOsdTC,
	mOsdOffsetNone,
	mOsdOffsetFN,
	mOsdOffsetTC,
	mOsdBox,

	mJackPlayPause,
	mJackPlay,
	mJackStop,
	mJackRewind,
};

static void open_context_menu(HWND hwnd, int x, int y) {
	HMENU hMenu = CreatePopupMenu();
	HMENU hSubMenuSync = CreatePopupMenu();
	HMENU hSubMenuSize = CreatePopupMenu();
	HMENU hSubMenuOSD  = CreatePopupMenu();
	HMENU hSubMenuJack = CreatePopupMenu();

	AppendMenu(hSubMenuSync, MF_STRING, mSyncJack, "Jack");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncLTC, "LTC");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncMTCJACK, "MTC (JACK)");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncMTCPort, "MTC (PortMidi)");
	AppendMenu(hSubMenuSync, MF_STRING, mSyncNone, "None");

	AppendMenu(hSubMenuSize, MF_STRING, mSize50, "50%");
	AppendMenu(hSubMenuSize, MF_STRING, mSize100, "100%");
	AppendMenu(hSubMenuSize, MF_STRING, mSize150, "150%");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mSizeAspect, "Reset Aspect");
	AppendMenu(hSubMenuSize, MF_STRING, mSizeLetterbox, "Letterbox");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mWinFullScreen, "Full Screen");
	AppendMenu(hSubMenuSize, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuSize, MF_STRING, mWinMouseVisible, "Mouse Cursor");

	AppendMenu(hSubMenuOSD, MF_STRING, mOsdFN, "Frame Number");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdTC, "Timecode");
	AppendMenu(hSubMenuOSD, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdOffsetNone, "Offset Off");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdOffsetFN, "Offset FN");
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdOffsetTC, "Offset TC");
	AppendMenu(hSubMenuOSD, MF_SEPARATOR, 0, NULL);
	AppendMenu(hSubMenuOSD, MF_STRING, mOsdBox, "Background");

	AppendMenu(hSubMenuJack, MF_STRING, mJackPlayPause, "Play/Pause");
	AppendMenu(hSubMenuJack, MF_STRING, mJackPlay, "Play");
	AppendMenu(hSubMenuJack, MF_STRING, mJackStop, "Stop");
	AppendMenu(hSubMenuJack, MF_STRING, mJackRewind, "Rewind");

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

	if (OSD_mode&OSD_FRAME) {
		CheckMenuItem(hSubMenuOSD, mOsdFN, MF_CHECKED | MF_BYCOMMAND);
	}
	if (OSD_mode&OSD_SMPTE) {
		CheckMenuItem(hSubMenuOSD, mOsdTC, MF_CHECKED | MF_BYCOMMAND);
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
	unsigned int flags_load = 0;
	unsigned int flags_sync = 0;
	unsigned int flags_jack = 0;
	if (ui_syncsource() != SYNC_JACK || (interaction_override&OVR_JCONTROL)) {
		flags_jack = MF_DISABLED | MF_GRAYED;
	}
	if (interaction_override & OVR_MENUSYNC) {
		flags_sync = MF_DISABLED | MF_GRAYED;
	}
	if (interaction_override & OVR_LOADFILE) {
		flags_load = MF_DISABLED | MF_GRAYED;
	}

	AppendMenu(hMenu, MF_STRING | MF_DISABLED, 0, "XJadeo " VERSION);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_STRING | flags_load, mLoad, "Open Video");
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_STRING | MF_POPUP | flags_sync, (UINT_PTR)hSubMenuSync, "Sync");
	AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenuSize, "Display");
	AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenuOSD, "OSD");
	AppendMenu(hMenu, MF_STRING | MF_POPUP | flags_jack, (UINT_PTR)hSubMenuJack, "Transport");

	/* and go */
	TrackPopupMenuEx(hMenu,
			TPM_LEFTALIGN | TPM_LEFTBUTTON,
			x, y, hwnd, NULL);

	DestroyMenu (hSubMenuOSD);
	DestroyMenu (hSubMenuJack);
	DestroyMenu (hSubMenuSize);
	DestroyMenu (hSubMenuSync);
	DestroyMenu (hMenu);
}

static void win_load_file(HWND hwnd) {
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
		printf("Load: '%s'\n", fn);
		xapi_open(fn);
	}
}

static void win_handle_menu(HWND hwnd, enum wMenuId id) {
	switch(id) {
		case mLoad:            win_load_file(hwnd); break;
		case mSyncJack:        ui_sync_to_jack(); break;
		case mSyncLTC:         ui_sync_to_jack(); break;
		case mSyncMTCJACK:     ui_sync_to_mtc_jack(); break;
		case mSyncMTCPort:     ui_sync_to_mtc_portmidi(); break;
		case mSyncNone:        ui_sync_none(); break;
		case mSize50:          XCresize_percent(50); break;
		case mSize100:         XCresize_percent(100); break;
		case mSize150:         XCresize_percent(150); break;
		case mSizeAspect:      XCresize_aspect(0); break;
		case mSizeLetterbox:   Xletterbox(2); break;
		case mWinOnTop:        Xontop(2); break;
		case mWinFullScreen:   Xfullscreen(2); break;
		case mWinMouseVisible: Xmousepointer(2); break;
		case mOsdFN:           ui_osd_fn(); break;
		case mOsdTC:           ui_osd_tc(); break;
		case mOsdOffsetNone:   ui_osd_offset_none(); break;
		case mOsdOffsetFN:     ui_osd_offset_fn(); break;
		case mOsdOffsetTC:     ui_osd_offset_tc(); break;
		case mOsdBox:          ui_osd_box(); break;
		case mJackPlayPause:   jackt_toggle(); break;
		case mJackPlay:        jackt_start(); break;
		case mJackStop:        jackt_stop(); break;
		case mJackRewind:      jackt_rewind(); break;
	}
}
#endif

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
	switch (message) {
		case WM_CREATE:
		case WM_SHOWWINDOW:
		case WM_SIZE:
			{
				RECT rect;
				GetClientRect(_gl_hwnd, &rect);
				gl_reshape (rect.right - rect.left, rect.bottom - rect.top);
				force_redraw = 1;
			}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(_gl_hwnd, &ps);
			xjglExpose();
			EndPaint(_gl_hwnd, &ps);
		}
		break;
	case WM_LBUTTONDOWN:
		xjglButton(1);
		break;
	case WM_MBUTTONDOWN:
		xjglButton(2);
		break;
	case WM_RBUTTONDOWN:
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
		xjglButton((short)HIWORD(wParam) > 0 ? 4 : 5);
		break;
	case WM_KEYDOWN:
		{
			static BYTE kbs[256];
			if (GetKeyboardState(kbs) != FALSE) {
				char lb[2];
				UINT scanCode = (lParam >> 8) & 0xFFFFFF00;
				if ( 1 == ToAscii(wParam, scanCode, kbs, (LPWORD)lb, 0)) {
					const char buf [2] = {(char)lb[0] , 0};
					xjglKeyPress((char)lb[0], buf);
				}
			}
		}
		break;
	case WM_KEYUP:
		break;
	case WM_QUIT:
	case XJ_CLOSE_MSG:
		if ((interaction_override&OVR_QUIT_WMG) == 0) {
			loop_flag = 0;
		}
		break;
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

int gl_open_window () {
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
	_gl_wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	_gl_wc.lpszMenuName  = NULL;
	_gl_wc.lpszClassName = MainWndClass;
	_gl_wc.hIconSm       = 0; // 16x16

	if (!RegisterClassEx(&_gl_wc)) {
		fprintf(stderr, "cannot regiser window class\n");
		MessageBox(NULL, TEXT("Error registering window class."), TEXT("Error"), MB_ICONERROR | MB_OK);
		return 1;
	}

	_gl_width = ffctv_width;
	_gl_height = ffctv_height;

	winFlags = start_fullscreen ? (WS_POPUP | WS_CLIPCHILDREN) : WS_OVERLAPPEDWINDOW;
	RECT wr = { 0, 0, (long)_gl_width, (long)_gl_height };
	AdjustWindowRectEx(&wr, winFlags, FALSE, WS_EX_TOPMOST);

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
		return 1;
	}

	_gl_hdc = GetDC(_gl_hwnd);
	if (!_gl_hdc) {
		DestroyWindow(_gl_hwnd);
		UnregisterClass(_gl_wc.lpszClassName, NULL);
		DestroyIcon(xjadeo_icon);
		return 1;
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
		return -1;
	}
	SetPixelFormat(_gl_hdc, format, &pfd);

	_gl_hglrc = wglCreateContext(_gl_hdc);
	if (!_gl_hglrc) {
		fprintf(stderr, "Cannot create openGL context.\n");
		ReleaseDC(_gl_hwnd, _gl_hdc);
		DestroyWindow(_gl_hwnd);
		UnregisterClass(_gl_wc.lpszClassName, NULL);
		DestroyIcon(xjadeo_icon);
		return -1;
	}

	wglMakeCurrent(_gl_hdc, _gl_hglrc);

	if (start_fullscreen) { gl_set_fullscreen(1); }
	if (start_ontop) { gl_set_ontop(1); }

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
		gl_close_window ();
		return 1;
	}

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
	return 0;
}

void gl_close_window() {
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(_gl_hglrc);
	ReleaseDC(_gl_hwnd, _gl_hdc);
	DestroyWindow(_gl_hwnd);
	DestroyCursor(hCurs_none);
	UnregisterClass(_gl_wc.lpszClassName, NULL);
	DestroyIcon(xjadeo_icon);
}

void gl_handle_events () {
	MSG msg;
	while (PeekMessage(&msg, _gl_hwnd, 0, 0, PM_REMOVE)) {
		handleMessage(_gl_hwnd, msg.message, msg.wParam, msg.lParam);
	}
	if (_gl_reexpose) {
		_gl_reexpose = false;
		xjglExpose();
	}
}

void gl_render (uint8_t *mybuffer) {
	xjglExpose();
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
	if (_gl_fullscreen) {
		GetWindowRect(_gl_hwnd, &fs_rect);
#if 0
		printf("GR %d %d %d %d: %dx%d\n", fs_rect.top, fs_rect.left, fs_rect.bottom, fs_rect.right,
				fs_rect.bottom - fs_rect.top, fs_rect.right - fs_rect.left);
#endif
		ChangeDisplaySettings(NULL, CDS_FULLSCREEN);
		winFlags = WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
		SetWindowLongPtr(_gl_hwnd, GWL_STYLE, winFlags);
		SetWindowPos (_gl_hwnd,
				HWND_TOPMOST,
				0, 0,
				GetSystemMetrics(SM_CXSCREEN /*SM_CXFULLSCREEN*/),
				GetSystemMetrics(SM_CYSCREEN /*SM_CYFULLSCREEN*/),
				SWP_ASYNCWINDOWPOS);
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
				SWP_ASYNCWINDOWPOS);
		ChangeDisplaySettings(NULL, CDS_RESET);
	}
	ShowWindow (_gl_hwnd, SW_SHOW);
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
