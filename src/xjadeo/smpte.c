/* simple timecode parser.
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
 * (c) 2006 
 *  Robin Gareus <robin@gareus.org>
 *
 * NOTE: compiles standalone for testing
 * 	gcc -o smpte smpte.c  -lm
 *
 * 	run ./smpte <timecode>
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

enum { SMPTE_FRAME = 0, SMPTE_SEC, SMPTE_MIN, SMPTE_HOUR, SMPTE_OVERFLOW, SMPTE_LAST };

/* binary coded decimal (BCD) digits 
 * not to mix up with SMPTE struct (in midi.c)
 * HH:MM:SS:FF
 */
typedef struct {
	int v[(SMPTE_LAST)]; ///< allocates 5 ints for a 32bit BCD - bad design :-)
} bcd;


#ifdef HAVE_CONFIG_H 	/* XJADEO include */
#include <config.h>
extern double framerate;
extern int want_dropframes; // force drop-frame TC (command-line arg) default: 0
extern int want_autodrop;   // force disable drop-frame TC (command-line arg) default:1
extern int have_dropframes; 
#define FPS framerate
#else			 /* Standalone */
//#define FPS 25
void dump(bcd *s, char *info);
double framerate = 25.0;
int want_dropframes = 0;
int want_autodrop = 1;
int have_dropframes = 0; // detected from MTC ;  TODO: force to zero if jack of user TC
#define FPS framerate
#endif

#ifdef HAVE_MIDI
extern int midi_clkconvert;
#else
int midi_clkconvert =0;
#endif


#define FIX_SMPTE_OVERFLOW(THIS,NEXT,INC) \
	if (s->v[(THIS)] >= (INC)) { int ov= (int) floor((double) s->v[(THIS)] / (INC));  s->v[(THIS)] -= ov*(INC); s->v[(NEXT)]+=ov;} \
	if (s->v[(THIS)] < 0 ) { int ov= (int) floor((double) s->v[(THIS)] / (INC));   s->v[(THIS)] -= ov*(INC); s->v[(NEXT)]+=ov;} 

void parse_int (bcd *s, int val) {
	int i;
	for (i=0;i<SMPTE_LAST;i++)
		s->v[i]=0;

	s->v[SMPTE_FRAME]= (int) val;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,(int)ceil(FPS));
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}


// FORMAT [[[HH:]MM:]SS:]FF
void parse_string (bcd *s, char *val) {
	int i;
	char *buf = strdup(val);
	char *t;

	for (i=0;i<SMPTE_LAST;i++)
		s->v[i]=0;

	i=0;
	while (i < SMPTE_OVERFLOW && buf && (t=strrchr(buf,':'))) {
		char *tmp=t+1;
		s->v[i] = (int) atoi(tmp);
		*t=0;
		i++;
	}
	if (i < SMPTE_OVERFLOW) s->v[i]= (int) atoi(buf);

	free(buf);
	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,(int)ceil(FPS));
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

/* legacy version of smpte_to_frame(...)
 * does not do any frame-dropping.. handy.
 */
int to_frame(bcd *s) {
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

void add (bcd*s, bcd *s0, bcd *s1) {
	int i;
	for (i=0;i<SMPTE_LAST;i++) s->v[i]=s0->v[i]+s1->v[i];
	//s->v[SMPTE_OVERFLOW]=0;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,(int)ceil(FPS));
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

void sub (bcd*s, bcd *s0, bcd *s1) {
	int i;
	for (i=0;i<SMPTE_LAST;i++) s->v[i]=s0->v[i]-s1->v[i];
	//s->v[SMPTE_OVERFLOW]=0;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,(int)ceil(FPS));
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/


#ifndef HAVE_CONFIG_H // standalone
void dump(bcd *s, char *info) {
	printf("%s %02i:%02i:%02i:%02i -- %i\n",info?info:"",
			s->v[SMPTE_HOUR],
			s->v[SMPTE_MIN],
			s->v[SMPTE_SEC],
			s->v[SMPTE_FRAME], to_frame(s));
}

int main (int argc, char **argv) {
	bcd n0,n1;
	bcd d0;

	if (argc != 2) { printf("usage %s <smpte>\n",argv[0]); return(1);
	framerate=25;
	parse_string(&n0,argv[1]);
	parse_int(&n1,25*60);
	sub(&d0,&n0,&n1);

	dump(&n0,"S1  : ");	
	dump(&n1,"S2  : ");	
	dump(&d0,"RES : ");	

	return(0);
}
#endif


/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/


/* 	
 * Insert two frame numbers at the 
 * start of every minute except the tenth.
 */
long  insert_drop_frames (long int frames) {
	long minutes = (frames / 17982L) * 10L; ///< 17982 frames in 10 mins base.
	long off_f = frames % 17982L;
	long off_adj =0;

	if (off_f >= 1800L) { // nothing to do in the first minute 
		off_adj  = 2 + 2 * (long) floor(((off_f-1800L) / 1798L)); 
	}

	return ( 1800L * minutes + off_f + off_adj);
}


/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

long int smpte_to_frame(int type, int f, int s, int m, int h, int overflow);

/* 
 * used for parsing user input '-o' 'set offset', etc
 * basically the same as smpte_to_frame(...)
 */
long int smptestring_to_frame (char *str) {
	bcd s;
	long int frame;
	parse_string(&s,str);
	if ((strchr(str,':') && have_dropframes && want_autodrop)||want_dropframes) {
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
		frame = (long int)to_frame(&s);

	return (frame);
}

/* any smpte output (verbose and OSD) */
void frame_to_smptestring(char *smptestring, long int frame) {
	bcd s;
	if (!smptestring) return;

	long frames = (long) floor((double) frame);
  char sep = ':';

	if ((have_dropframes && want_autodrop)||want_dropframes) {
		frames = insert_drop_frames(frames);
    sep = '.';
	}

	parse_int(&s, (int) frames);

	snprintf(smptestring,13,"%02i%c%02i%c%02i%c%02i",
			s.v[SMPTE_HOUR], sep,
			s.v[SMPTE_MIN],  sep,
			s.v[SMPTE_SEC],  sep,
			s.v[SMPTE_FRAME]);
}

long int smpte_to_frame(int type, int f, int s, int m, int h, int overflow) {
	long frame =0 ;
	double fps= FPS;

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
		long base_time = ((h*3600) + ((m/10) * 10 * 60)) * fps; // XXXFPS
		long off_m = m % 10;
		long off_s = (off_m * 60) + s;
		long off_f = (30 * off_s) + f - (2 * off_m);
		//long off_s = (long) rint(off_f * XXXFPS/fps);
		//frame = base_time + off_s;
		frame = base_time + off_f;
		fps=30; 
		have_dropframes=1;  // TODO: recalc ts_offset string when changing this
	} else {
		frame = f + fps * ( s + 60*m + 3600*h);
		have_dropframes=0;  // TODO: recalc ts_offset string when changing this
	}

	switch (midi_clkconvert) {
		case 2: // force video fps
			frame = f +  (int) floor(FPS * ( s + 60*m+ 3600*h));
		break;
		case 3: // 'convert' FPS.
			frame = (int) rint(frame * FPS / fps);
		break;
		default: // use MTC fps info
			;
		break;
	}
	return(frame);
}

#if 0
// never use this - this drops frames not smpte-timestamps!
// it can "import" external non-drop timestamps to
// local drop-frame timecode. 
long int sec_to_frame(double secs) {
	int frames = secs * framerate;
	int type = -1 ;
	bcd s;

	if (framerate >29.9 && framerate < 30.0 ) type =2;

	parse_int(&s, (int) frames);

	return (smpte_to_frame(type, 
			s.v[SMPTE_FRAME],
			s.v[SMPTE_SEC],
			s.v[SMPTE_MIN],
			s.v[SMPTE_HOUR],
			0));
}
#endif
