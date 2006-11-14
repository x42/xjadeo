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
	int v[(SMPTE_LAST)];
} bcd;


#ifdef HAVE_CONFIG_H 	/* XJADEO include */
extern double framerate;
extern int midi_clkconvert;
#define FPS framerate
#else			 /* Standalone */
//#define FPS 25
void dump(bcd *s, char *info);
int midi_clkconvert =0;
int framerate = 25;
#define FPS framerate
#endif


#define FIX_SMPTE_OVERFLOW(THIS,NEXT,INC) \
	if (s->v[(THIS)] >= (INC)) { int ov= (int) floor((double) s->v[(THIS)] / (INC));  s->v[(THIS)] -= ov*(INC); s->v[(NEXT)]+=ov;} \
	if (s->v[(THIS)] < 0 ) { int ov= (int) floor((double) s->v[(THIS)] / (INC));   s->v[(THIS)] -= ov*(INC); s->v[(NEXT)]+=ov;} 

void parse_int (bcd *s, int val) {
	int i;
	for (i=0;i<SMPTE_LAST;i++)
		s->v[i]=0;

	s->v[SMPTE_FRAME]= (int) val;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
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
	//dump(s,"DEBUG  : ");	

	free(buf);
	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

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
	frame=(int) floor(sec*FPS)+s->v[SMPTE_FRAME];
	return (frame);
}

void add (bcd*s, bcd *s0, bcd *s1) {
	int i;
	for (i=0;i<SMPTE_LAST;i++) s->v[i]=s0->v[i]+s1->v[i];
	//s->v[SMPTE_OVERFLOW]=0;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

void sub (bcd*s, bcd *s0, bcd *s1) {
	int i;
	for (i=0;i<SMPTE_LAST;i++) s->v[i]=s0->v[i]-s1->v[i];
	//s->v[SMPTE_OVERFLOW]=0;

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

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

//	if (argc != 2) return(1);
	parse_string(&n0,argv[1]);
	parse_int(&n1,25*60);
	sub(&d0,&n0,&n1);

	dump(&n0,"S1  : ");	
	dump(&n1,"S2  : ");	
	dump(&d0,"RES : ");	

	return(0);
}
#endif

long int smptestring_to_frame (char *str) {
	bcd n0;
	parse_string(&n0,str);
	return ((long int)to_frame(&n0));
}

void frame_to_smptestring(char *smptestring, long int frame) {
	bcd s;
	if (!smptestring) return;

	parse_int(&s, (int) frame);
	snprintf(smptestring,13,"%02i:%02i:%02i:%02i",
			s.v[SMPTE_HOUR],
			s.v[SMPTE_MIN],
			s.v[SMPTE_SEC],
			s.v[SMPTE_FRAME]);
}


long int smpte_to_frame(int type, int f, int s, int m, int h, int overflow) {
	long frame =0 ;
	int fps= FPS;

	switch(type) {
		case 0: fps=24; break;
		case 1: fps=25; break;
		case 2: fps=29; break;
		case 3: fps=30; break;
	}
	switch (midi_clkconvert) {
		case 2: // force video fps
			frame = f +  (int) floor(FPS * ( s + 60*m+ 3600*h));
		break;
		case 3: // 'convert' FPS.
			frame = f + fps * ( s + 60*m + 3600*h);
			frame = (int) rint(frame * FPS / fps);
		break;
		default: // use MTC fps info
			frame = f + fps * ( s + 60*m + 3600*h);
		break;
	}
	return(frame);
}
