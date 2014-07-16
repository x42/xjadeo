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

// For keyboard shortcuts
void jackt_toggle();
void jackt_rewind();
extern long ts_offset;
extern char *smpte_offset;
extern double framerate;

// globals
extern int 	force_redraw;
extern int 	interaction_override; // disable some options.
extern int  movie_width;
extern int  movie_height;

///////////////////////////////////////////////////////////////////////////////

static int        _gl_width;
static int        _gl_height;
static int        _gl_ontop = 0;
static int        _gl_fullscreen = 0;
static int        _gl_mousepointer = 0;

static float      _gl_quad_x = 1.0;
static float      _gl_quad_y = 1.0;
static int        _gl_reexpose = 0;
unsigned int      _gl_texture_id = 0;

///////////////////////////////////////////////////////////////////////////////
static void gl_make_current();
static void gl_swap_buffers();

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

static void gl_reallocate_texture(int width, int height) {
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
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#ifndef HAVE_WINDOWS
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif
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

static void xjglExpose() {
	if (!buffer) return;
	gl_make_current();
	opengl_draw (movie_width, movie_height, buffer);
	glFlush();
	gl_swap_buffers();
}

static void xjglButton(int btn) {
	if (btn == 1) {
		gl_resize (movie_width, movie_height);
	} else {
		const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
		int w = _gl_width;
		int h = _gl_height;
		if (btn == 5 && w > 32 && h > 32)  {
			const float step = sqrtf ((float)h);
			w -= floorf (step * asp_src);
			h -= step;
		}
		if (btn == 4) {
			const float step = sqrtf ((float)h);
			w += floorf (step * asp_src);
			h += step;
		}
		if( asp_src < ((float)w / (float)h) )
			w = floorf ((float)h * asp_src);
		else
			h = floorf ((float)w / asp_src);
		gl_resize (w,h);
	}
}

static void xjglKeyPress(const unsigned int sym, const char *key) {
	if (!strcmp(key, "q") || sym == 0xff1b) {
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
		gl_set_fullscreen(2);
	}
	else if (!strcmp(key, "o")) {
		if (OSD_mode&OSD_OFFF) {
			OSD_mode&=~OSD_OFFF;
			OSD_mode|=OSD_OFFS;
		} else if (OSD_mode&OSD_OFFS) {
			OSD_mode^=OSD_OFFS;
		} else {
			OSD_mode^=OSD_OFFF;
		}
		force_redraw=1;
	}
	else if (!strcmp(key, "l")) {
		want_letterbox=!want_letterbox;
		gl_letterbox_change();
		_gl_reexpose = 1;
	}
	else if (!strcmp(key, "s")) {
		OSD_mode^=OSD_SMPTE;
		force_redraw=1;
	}
	else if (!strcmp(key, "v")) {
		OSD_mode^=OSD_FRAME;
		force_redraw=1;
	}
	else if (!strcmp(key, "b")) {
		OSD_mode^=OSD_BOX;
		force_redraw=1;
	}
	else if (!strcmp(key, "C")) {
		OSD_mode=0;
		force_redraw=1;
	}
	else if (!strcmp(key, ".")) {
		gl_resize(movie_width, movie_height);
	}
	else if (!strcmp(key, ",")) {
		const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
		int w = _gl_width;
		int h = _gl_height;
		if( asp_src < ((float)_gl_width / (float)_gl_height) )
			w = rintf ((float)_gl_height * asp_src);
		else
			h = rint((float)_gl_width / asp_src);
		gl_resize(w, h);
	}
	else if (!strcmp(key, "<")) {
		const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
		int w = _gl_width;
		int h = _gl_height;
		float step = 0.2 * h;
		w -= floorf(step * asp_src);
		h -= step;
		gl_resize(w, h);
	}
	else if (!strcmp(key, ">")) {
		const float asp_src = movie_aspect ? movie_aspect : (float)movie_width/(float)movie_height;
		int w = _gl_width;
		int h = _gl_height;
		float step = 0.2 * h;
		w += floorf(step * asp_src);
		h += step;
		gl_resize(w, h);
	}
	else if (!strcmp(key, "+")) {
		if ((interaction_override&OVR_AVOFFSET) != 0 ) {
			remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
			return;
		}
		ts_offset++;
		force_redraw=1;
		if (smpte_offset) free(smpte_offset);
		smpte_offset= calloc(15,sizeof(char));
		frame_to_smptestring(smpte_offset,ts_offset);
	}
	else if (!strcmp(key, "-")) {
		if ((interaction_override&OVR_AVOFFSET) != 0 ) {
			remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
			return;
		}
		ts_offset--;
		force_redraw=1;
		if (smpte_offset) free(smpte_offset);
		smpte_offset= calloc(15,sizeof(char));
		frame_to_smptestring(smpte_offset,ts_offset);
	}
	else if (!strcmp(key, "}")) {
		if ((interaction_override&OVR_AVOFFSET) != 0 ) {
			remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
			return;
		}
		if (framerate > 0) {
			ts_offset+= framerate *60;
		} else {
			ts_offset+= 25*60;
		}
		force_redraw=1;
		if (smpte_offset) free(smpte_offset);
		smpte_offset= calloc(15,sizeof(char));
		frame_to_smptestring(smpte_offset,ts_offset);
	}
	else if (!strcmp(key, "{")) {
		if ((interaction_override&OVR_AVOFFSET) != 0 ) {
			remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
			return;
		}
		if (framerate > 0) {
			ts_offset-= framerate *60;
		} else {
			ts_offset-= 25*60;
		}
		force_redraw=1;
		if (smpte_offset) free(smpte_offset);
		smpte_offset= calloc(15,sizeof(char));
		frame_to_smptestring(smpte_offset,ts_offset);
	}
	else if (!strcmp(key, "m")) {
		gl_mousepointer(2);
	}
	else if (sym == 0xff08 || sym == 0x7f) { // BackSpace, Del
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
		remote_notify(NTY_KEYBOARD, 310, "keypress=%d", sym);
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
#endif
