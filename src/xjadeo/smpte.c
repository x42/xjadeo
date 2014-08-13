/* xjadeo - simple timecode parser.
 *
 * (C) 2006 Robin Gareus <robin@gareus.org>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PLATFORM_WINDOWS
#if (!defined __uint32_t_defined && !defined _STDINT_H)
typedef long long int int64_t;
#endif
#else
#include <stdint.h>
#endif


enum { SMPTE_FRAME = 0, SMPTE_SEC, SMPTE_MIN, SMPTE_HOUR, SMPTE_OVERFLOW, SMPTE_LAST };

/* binary coded decimal (BCD) digits
 * not to mix up with SMPTE struct (in midi.c)
 * HH:MM:SS:FF
 */
typedef struct {
	int v[(SMPTE_LAST)];
} bcd;

#ifdef HAVE_CONFIG_H 	/* XJADEO include */
#include <config.h>
#endif

extern double framerate;
extern int want_dropframes; // force drop-frame TC (command-line arg) default: 0
extern int want_autodrop;   // force disable drop-frame TC (command-line arg) default:1
extern int have_dropframes;
#define FPS framerate

#ifdef HAVE_MIDI
extern int midi_clkconvert;
#else
static int midi_clkconvert =0;
#endif

#define FIX_SMPTE_OVERFLOW(THIS,NEXT,INC) \
  if (s->v[(THIS)] >= (INC)) { int ov = (int) floor((double) s->v[(THIS)] / (INC));  s->v[(THIS)] -= ov*(INC); s->v[(NEXT)]+=ov;} \
  if (s->v[(THIS)] < 0 ) { int ov= (int) floor((double) s->v[(THIS)] / (INC));   s->v[(THIS)] -= ov*(INC); s->v[(NEXT)]+=ov;}

static void parse_int (bcd *s, int val) {
	int i;
	for (i=0;i < SMPTE_LAST;i++)
		s->v[i]=0;

	s->v[SMPTE_FRAME]= val;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,ceil(FPS));
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

// FORMAT [[[HH:]MM:]SS:]FF
static void parse_string (bcd *s, char *val) {
	int i;
	char *buf = strdup(val);
	char *t;

	for (i = 0; i < SMPTE_LAST; ++i)
		s->v[i]=0;

	i=0;
	while (i < SMPTE_OVERFLOW && buf && (t=strrchr(buf,':'))) {
		char *tmp=t+1;
		s->v[i] = atoi(tmp);
		*t=0;
		i++;
	}
	if (i < SMPTE_OVERFLOW) s->v[i]= atoi(buf);

	free(buf);
	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC, ceil(FPS));
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

/* legacy version of smpte_to_frame(...)
 * does not do any frame-dropping.. handy.
 */
static int to_frame(bcd *s) {
	int frame=0;
	int sec=0;
#if 0
	printf("%s %02i#%02i:%02i:%02i:%02i\n","DBG: ",
			s->v[SMPTE_OVERFLOW],
			s->v[SMPTE_HOUR],
			s->v[SMPTE_MIN],
			s->v[SMPTE_SEC],
			s->v[SMPTE_FRAME]);
#endif
	sec=((((s->v[SMPTE_HOUR]*60)+s->v[SMPTE_MIN])*60)+s->v[SMPTE_SEC]);
	if (s->v[SMPTE_OVERFLOW]<0) {
		sec=86400-sec;
		sec*=-1;
	}
	// TODO: check if this behaves correctly for non integer FPS :->
	//frame=(int) floor(sec*FPS)+s->v[SMPTE_FRAME];
	frame=(int) floor(sec * ceil(FPS)) + s->v[SMPTE_FRAME];
	return (frame);
}

/*
 * Insert two frame numbers at the
 * start of every minute except the tenth.
 */
static int64_t insert_drop_frames (int64_t frames) {
	int64_t minutes = (frames / 17982L) * 10L; ///< 17982 frames in 10 mins base.
	int64_t off_f = frames % 17982L;
	int64_t off_adj =0;

	if (off_f >= 1800L) { // nothing to do in the first minute
		off_adj  = 2 + 2 * (int64_t) floor(((off_f-1800L) / 1798L));
	}

	return ( 1800L * minutes + off_f + off_adj);
}


/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

int64_t smpte_to_frame(int type, int f, int s, int m, int h, int overflow);

/*
 * used for parsing user input '-o' 'set offset', etc
 * basically the same as smpte_to_frame(...)
 */
int64_t smptestring_to_frame (char *str) { // TODO int64
	bcd s;
	int64_t frame;
	// TODO handle negative prefix
	parse_string (&s,str);
	if ((strchr(str, ':') && have_dropframes && want_autodrop)||want_dropframes) {
		frame= smpte_to_frame (
				2 /*29.97fps */,
				s.v[SMPTE_FRAME],
				s.v[SMPTE_SEC],
				s.v[SMPTE_MIN],
				s.v[SMPTE_HOUR],
				0);

		if (s.v[SMPTE_OVERFLOW]<0) {
			frame=30*86400-frame;
			frame*=-1;
		}
	} else
		frame = (int64_t)to_frame(&s);

	return (frame);
}

/* any smpte output (verbose and OSD) */
int frame_to_smptestring(char *smptestring, int64_t frame, uint8_t add_sign) {
	bcd s;
	if (!smptestring) return 0;

	int64_t frames = frame;
	char sep = ':';

	if ((have_dropframes && want_autodrop)||want_dropframes) {
		frames = insert_drop_frames(frames);
		sep = ';';
	}

	if (add_sign && frames < 0) {
		parse_int(&s, (int) -frames);
	} else {
		parse_int(&s, (int) frames);
	}

	if (add_sign) {
		snprintf(smptestring,14,"%c%02i:%02i:%02i%c%02i",
				(frames < 0) ? '-' : ' ',
				s.v[SMPTE_HOUR],
				s.v[SMPTE_MIN],
				s.v[SMPTE_SEC],
				sep,
				s.v[SMPTE_FRAME]);
	} else {
		snprintf(smptestring,13,"%02i:%02i:%02i%c%02i",
				s.v[SMPTE_HOUR],
				s.v[SMPTE_MIN],
				s.v[SMPTE_SEC],
				sep,
				s.v[SMPTE_FRAME]);
	}
	return s.v[SMPTE_OVERFLOW];
}

int64_t smpte_to_frame(int type, const int f, const int s, const int m, const int h, const int overflow) {
	int64_t frame = 0;
	double fps = FPS;

	switch(type) {
		case 0: fps=24.0; break;
		case 1: fps=25.0; break;
		case 2: fps=30.0*1000.0/1001.0; break;
		case 3: fps=30.0; break;
	}

	if (type==2 || want_dropframes) {
		/*
		 * Drop frame numbers (not frames) 00:00 and 00:01 at the
		 * start of every minute except the tenth.
		 *
		 * dropframes are not required or permitted when operating at
		 * 24, 25, or 30 frames per second.
		 *
		 */
		int64_t base_time = (int64_t)((h*3600) + ((m/10) * 10 * 60)) * fps;
		int64_t off_m = m % 10;
		int64_t off_s = (off_m * 60) + s;
		int64_t off_f = (30 * off_s) + f - (2 * off_m);
		frame = base_time + off_f;
		fps=30;
		have_dropframes = 1;
	} else {
		frame = f + fps * (int64_t)(s + 60*m + 3600*h);
		have_dropframes = 0;
	}

	switch (midi_clkconvert) {
		case 1: // force video fps
			frame = f +  (int64_t) floor(FPS * ( s + 60*m+ 3600*h));
			break;
		case 2: // 'convert' FPS.
			frame = (int64_t) rint(frame * FPS / fps);
			break;
		default: // use MTC fps info
			;
			break;
	}
	return(frame);
}
