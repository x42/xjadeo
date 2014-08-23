/* xjadeo - jack video monitor, common openGL functions
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

#include "xjadeo.h"
#ifdef HAVE_GL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "display.h"

#ifdef __APPLE__
#include "OpenGL/glu.h"
#else
#include <GL/glu.h>
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

///////////////////////////////////////////////////////////////////////////////

static int        _gl_width;
static int        _gl_height;
static int        _gl_ontop = 0;
static int        _gl_fullscreen = 0;

static float        _gl_quad_x = 1.0;
static float        _gl_quad_y = 1.0;
static int          _gl_reexpose = 0;
static unsigned int _gl_texture_id = 0;
static int          _gl_vblank_sync = 0;

///////////////////////////////////////////////////////////////////////////////
static void gl_make_current();
static void gl_swap_buffers();

static void gl_sync_lock();
static void gl_sync_unlock();

static void gl_reshape(int width, int height) {
	gl_make_current();

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho (-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	glClear (GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	_gl_width  = width;
	_gl_height = height;

	gl_letterbox_change();
}

static int gl_reallocate_texture(int width, int height) {
	glDeleteTextures (1, &_gl_texture_id);
	glViewport (0, 0, _gl_width, _gl_height);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

	glClear (GL_COLOR_BUFFER_BIT);

	glGenTextures (1, &_gl_texture_id);
	glBindTexture (GL_TEXTURE_RECTANGLE_ARB, _gl_texture_id);
	glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
			width, height, 0,
			GL_BGRA, GL_UNSIGNED_BYTE, NULL);

	//TODO use glBindBuffer() // glBindBufferARB()

	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#ifndef PLATFORM_WINDOWS
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif
	return 0;
}

static void gl_init () {
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glDisable (GL_DEPTH_TEST);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_RECTANGLE_ARB);
}

static void opengl_draw (int width, int height, unsigned char* surf_data) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);

	glPushMatrix ();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _gl_texture_id);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
			width, height, /*border*/ 0,
			GL_BGRA, GL_UNSIGNED_BYTE, surf_data);

	glBegin(GL_QUADS);
	glTexCoord2f(           0.0f, (GLfloat) height);
	glVertex2f(-_gl_quad_x, -_gl_quad_y);

	glTexCoord2f((GLfloat) width, (GLfloat) height);
	glVertex2f( _gl_quad_x, -_gl_quad_y);

	glTexCoord2f((GLfloat) width, 0.0f);
	glVertex2f( _gl_quad_x,  _gl_quad_y);

	glTexCoord2f(            0.0f, 0.0f);
	glVertex2f(-_gl_quad_x,  _gl_quad_y);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glPopMatrix();
}

///////////////////////////////////////////////////////////////////////////////

static void xjglExpose(uint8_t *buf) {
	if (!buf) buf = buffer;
	if (!buf) return;
	gl_make_current();
	opengl_draw (movie_width, movie_height, buf);
	glFlush();
	gl_swap_buffers();
	if (_gl_vblank_sync) {
		glFinish();
	}
}

static void xjglButton(int btn) {
	switch (btn) {
		case 2:
			XCresize_aspect(0);
			break;
		case 5:
			XCresize_aspect(-1);
			break;
		case 4:
			XCresize_aspect(1);
			break;
		default:
			break;
	}
}

static void xjglKeyPress(const unsigned int sym, const char *key) {
	if (sym == 0xff1b || sym == 0x1b) {
		if ((interaction_override&OVR_QUIT_KEY) == 0) {
			loop_flag=0;
		} else {
			remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
		}
	}
	else if (!strcmp(key, "a")) {
		gl_set_ontop(2);
	}
	else if (!strcmp(key, "f")) {
		//NB. gl_win handles this directly.
		gl_set_fullscreen(2);
	}
	else if (!strcmp(key, "o")) {
		gl_sync_lock();
		ui_osd_offset_cycle();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "p")) {
		gl_sync_lock();
		ui_osd_permute();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "r")) {
		gl_sync_lock();
		ui_osd_outofrange();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "x")) {
		gl_sync_lock();
		ui_osd_pos();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "l")) {
		gl_sync_lock();
		want_letterbox=!want_letterbox;
		gl_letterbox_change();
		_gl_reexpose = 1;
		gl_sync_unlock();
	}
	else if (!strcmp(key, "s")) {
		gl_sync_lock();
		ui_osd_tc();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "v")) {
		gl_sync_lock();
		ui_osd_fn();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "b")) {
		gl_sync_lock();
		ui_osd_box();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "C")) {
		gl_sync_lock();
		ui_osd_clear();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "g")) {
		gl_sync_lock();
		ui_osd_geo();
		gl_sync_unlock();
	}
	else if (!strcmp(key, "i")) {
		gl_sync_lock();
		ui_osd_fileinfo();
		gl_sync_unlock();
	}
	else if (!strcmp(key, ".")) {
		XCresize_percent(100);
	}
	else if (!strcmp(key, ",")) {
		XCresize_aspect(0);
	}
	else if (!strcmp(key, "<")) {
		XCresize_scale(-1);
	}
	else if (!strcmp(key, ">")) {
		XCresize_scale(1);
	}
	else if (!strcmp(key, "\\")) {
		gl_sync_lock();
		XCtimeoffset(0, sym);
		gl_sync_unlock();
	}
	else if (!strcmp(key, "+")) {
		gl_sync_lock();
		XCtimeoffset(1, sym);
		gl_sync_unlock();
	}
	else if (!strcmp(key, "-")) {
		gl_sync_lock();
		XCtimeoffset(-1, sym);
		gl_sync_unlock();
	}
	else if (!strcmp(key, "}")) {
		gl_sync_lock();
		XCtimeoffset(2, sym);
		gl_sync_unlock();
	}
	else if (!strcmp(key, "{")) {
		gl_sync_lock();
		XCtimeoffset(-2, sym);
		gl_sync_unlock();
	}
	else if (!strcmp(key, "m")) {
		gl_mousepointer(2);
	}
	else if (sym == 0xff08 || sym == 0x7f || sym == 0x08) { // BackSpace, Del
		if ((interaction_override&OVR_JCONTROL) == 0)
			jackt_rewind();
		remote_notify(NTY_KEYBOARD,
				310, "keypress=%d # backspace", sym);
	}
	else if (!strcmp(key, " ") || sym == 0x0020) {
		if ((interaction_override&OVR_JCONTROL) == 0)
			jackt_toggle();
		remote_notify(NTY_KEYBOARD,
				310, "keypress=%d # space", sym);
	}
	else {
		if (want_debug) {
			printf("unassigned key pressed: 0x%x '%s' [", sym, key);
			while (*key != 0) {
				printf("%02x ", *key);
				++key;
			}
			printf("]\n");
		}
		gl_sync_lock();
		remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
		gl_sync_unlock();
	}
}

void gl_letterbox_change () {
	if (!want_letterbox) {
		_gl_quad_x = 1.0;
		_gl_quad_y = 1.0;
	} else {
		const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
		const float asp_dst = (float)_gl_width / (float)_gl_height;
		if (asp_dst > asp_src) {
			_gl_quad_x = asp_src / asp_dst;
			_gl_quad_y = 1.0;
		} else {
			_gl_quad_x = 1.0;
			_gl_quad_y = asp_dst / asp_src;
		}
	}
}

static float calc_slider(int x, int y) {
	if (interaction_override&OVR_MENUSYNC) return -1;
	// TODO: cache those for a given values of
	// _gl_width, _gl_height, movie_width, movie_height
	const float xw = _gl_width * _gl_quad_x;

	const int bar_x0 = xw * ((.5 / _gl_quad_x - .5) + PB_X / (float)movie_width);
	const int bar_xw = xw * ( (movie_width - 2 * PB_X) / (float)movie_width);

	const int bar_y1 = _gl_height * (.5 + _gl_quad_y * (BAR_Y - .5)); // 89% = bottom of OSD_bar
	const int bar_y0 = bar_y1 - (PB_H - 4) * (_gl_height * _gl_quad_y) / (float)movie_height;

	if (y > bar_y0 && y < bar_y1 && x >= bar_x0 && x <= bar_x0 + bar_xw) {
		return (100.f * (x - bar_x0) / (float)(bar_xw));
	}
	return -1;
}
#endif
