/* simple smpte parser.
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

//#define FPS 25
extern double framerate;
#define FPS framerate

enum { SMPTE_FRAME = 0, SMPTE_SEC, SMPTE_MIN, SMPTE_HOUR, SMPTE_OVERFLOW, SMPTE_LAST };

typedef struct {
	int v[(SMPTE_LAST)];
} smpte;

#define FIX_SMPTE_OVERFLOW(THIS,NEXT,INC) \
	while (s->v[(THIS)] >= (INC)) { s->v[(THIS)] -= (INC); s->v[(NEXT)]++;} \
	while (s->v[(THIS)] < 0  ) { s->v[(THIS)] += (INC); s->v[(NEXT)]--;}

void parse_int (smpte *s, int val) {
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
void parse_string (smpte *s, char *val) {
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
	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

int to_frame(smpte *s) {
	int frame=0;
	frame=((((s->v[SMPTE_HOUR]*60)+s->v[SMPTE_MIN])*60)+s->v[SMPTE_SEC]);
	if (s->v[SMPTE_HOUR]>11) {
		frame=86400-frame;
		frame*=-1;
	}
	frame=(int) floor(frame*FPS);
	frame+=s->v[SMPTE_FRAME];
	return (frame);
}

void add (smpte*s, smpte *s0, smpte *s1) {
	int i;
	for (i=0;i<SMPTE_OVERFLOW;i++) s->v[i]=s0->v[i]+s1->v[i];

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

void sub (smpte*s, smpte *s0, smpte *s1) {
	int i;
	for (i=0;i<SMPTE_OVERFLOW;i++) s->v[i]=s0->v[i]-s1->v[i];

	FIX_SMPTE_OVERFLOW(SMPTE_FRAME,SMPTE_SEC,FPS);
	FIX_SMPTE_OVERFLOW(SMPTE_SEC,SMPTE_MIN,60);
	FIX_SMPTE_OVERFLOW(SMPTE_MIN,SMPTE_HOUR,60);
	FIX_SMPTE_OVERFLOW(SMPTE_HOUR,SMPTE_OVERFLOW,24);
}

#if 0
void dump(smpte *s, char *info) {
	printf("%s %02i:%02i:%02i:%02i -- %i\n",info?info:"",
			s->v[SMPTE_HOUR],
			s->v[SMPTE_MIN],
			s->v[SMPTE_SEC],
			s->v[SMPTE_FRAME], to_frame(s));
}

int main (int argc, char **argv) {
	smpte n0,n1;
	smpte d0;

//	if (argc != 2) return(1);
	parse_string(&n0,argv[1]);
	parse_int(&n1,1100);
	sub(&d0,&n0,&n1);

	dump(&n0,"S1  : ");	
	dump(&n1,"S2  : ");	
	dump(&d0,"RES : ");	

	return(0);
}
#endif

long int smptestring_to_frame (char *str) {
	smpte n0;
	parse_string(&n0,str);
	return ((long int)to_frame(&n0));
}

void frame_to_smptestring(char *smptestring, long int frame) {
	smpte s;
	if (!smptestring) return;

	parse_int(&s, (int) frame);
	snprintf(smptestring,13,"%02i:%02i:%02i:%02i",
			s.v[SMPTE_HOUR],
			s.v[SMPTE_MIN],
			s.v[SMPTE_SEC],
			s.v[SMPTE_FRAME]);
}


