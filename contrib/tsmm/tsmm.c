/* tsmm - time stamp movie maker
 *
 * (c) 2006 *  Robin Gareus <robin@gareus.org>
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
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <tiffio.h>

#include <xjadeo.h>


char *program_name;
/* hardcoded settings */
int want_quiet = 1;
int want_verbose = 0;
int want_debug = 0;
int want_dropframes =0; /* --dropframes -N  BEWARE! */
#ifdef HAVE_MIDI
int midi_clkconvert =0;	/* --midifps [0:MTC|1:VIDEO|2:RESAMPLE] */
#endif
int want_autodrop =1;   /* --nodropframes -n (hidden option) */
double	framerate = 25;


#define OSD_fontfile FONT_FILE
extern unsigned char ST_image[][ST_WIDTH];
extern int ST_rightend;
int OSD_mode = 0;
#define ST_BG ((OSD_mode&OSD_BOX)?0:1)

typedef struct {
	char *text;
	int xpos;
	int yperc;
} text_element;


static void usage (int status)
{
  printf ("%s -   render time stamps on tiff frames\n", program_name);
  printf ("usage: %s <base-path>\n", program_name);
  printf (""
"\n  This is work in progres. it generates a tiff image sequence"
"\n  with the frame number and SMPTE rendered on a black tiff."
"\n    mkdir /tmp/sequence/ "
"\n    %s /tmp/sequence/"
"\n  use with .../xjadeo/trunk/contrib/tsmm/timestampmoviemaker.sh"
"\n", program_name);
  exit (status);
}


void set_positions(int *xalign, int *yalign, int w, int h, int xpos, int yperc) {
	if (xalign) {
		if (xpos == OSD_LEFT) *xalign=ST_PADDING; // left
		else if (xpos == OSD_RIGHT) *xalign=w-ST_PADDING-ST_rightend; // right
		else *xalign=(w-ST_rightend)/2; // center
	}
	if (yalign) 
		*yalign= (h - ST_HEIGHT) * yperc /100.0;

   //	if (xalign && yalign) printf ("DEBUG: x:%i y:%i\n",*xalign,*yalign);
}


int render_frame (char *filename, int w, int h, text_element *te)  {
	TIFF *out;
	uint32 rps;
	uint16 bps, spp ;
	uint32 imageStripsize;
	int nos;  //< number of strips
	int i,ii;
	tdata_t buf;
	tstrip_t strip;
	int xalign, yalign;

	if (!te) return(1);
	out = TIFFOpen(filename, "w");
	if (!out ) { printf("could not open output file\n"); return 1; }

	rps=w; spp=3; bps=8;

	/* Magical stuff to create the image */

	TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, bps); 
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, spp);
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rps); 

	imageStripsize = TIFFStripSize (out);
	nos = w*h*spp/imageStripsize;
	strip = 0;

	if (nos != 1) {
		printf("image size needs to be mult. of 8, or image is too large.\n");
		return (1);
	}

	if (!want_quiet) {
		printf ("myInfo:  w:%u h:%u bps:%u spp:%u rps:%u\n",w,h,bps,spp,rps);
		printf ("extInfo: nos:%u strip-size:%u\n",nos,imageStripsize);
	}

	buf = _TIFFmalloc(imageStripsize);
	memset(buf,0,imageStripsize);
	ii=0;
	while (te[ii].text) {
		int x,y;
		if (want_verbose)
			printf("rendering text: %s\n",te[ii].text);
		if ( render_font(OSD_fontfile, te[ii].text) ) return(1);
		set_positions(&xalign, &yalign, w, h, te[ii].xpos, te[ii].yperc);
		for (x=0; x<ST_rightend && (x+xalign) < w ;x++) {
			for (y=0; y<ST_HEIGHT && (y+yalign) < h;y++) {
				if (ST_image[y][x]>= ST_BG) {
					i= spp* ((x+xalign)+w*(y+yalign));
					((uint8*)buf)[i+0] = ST_image[y][x];
					((uint8*)buf)[i+1] = ST_image[y][x];
					((uint8*)buf)[i+2] = ST_image[y][x];
				}
			}
		}
		ii++;
	}

	TIFFWriteEncodedStrip(out, strip, buf, imageStripsize);
	_TIFFfree(buf);

	TIFFClose(out);
	return(0);
}

int test_dir(char *d) {
	struct stat s;
	int result= stat(d, &s);
	if (result != 0) return 1; /* stat() failed */
	if (!S_ISDIR(s.st_mode)) return 1; /* is not a directory file */
	if (s.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))  return 0; /* is writeable */
	return 1;
}

#define MAX_PATH 1024 
int main (int argc, char **argv) {
	char filename[MAX_PATH];
	char *filepath = NULL;
	text_element *te = calloc(5,sizeof(text_element));
	int i;
	int err = 0;
	int last_frame=150;
	int w=336;
	int h=192;
	program_name=argv[0];

	if (argc>1) filepath=argv[1];
	else usage(1);

	//last_frame=10*60*framerate;

	if (test_dir(filepath)) {
		printf("directory %s does not exist\n",filepath);
		return(1);
	}
	printf("writing %s/frame_%07i.tif -> %s/frame_%07i.tif\n",filepath,0,filepath,last_frame);
	printf("waiting 5 sec. press CTRL-C to interrupt.\n");  fflush(stdout);
	sleep(5);

	te[0].xpos=OSD_CENTER;
	te[0].yperc=5;
	te[1].xpos=OSD_CENTER;
	te[1].yperc=98;

	for (i=0;i<last_frame && !err;i++) {
		snprintf(filename,MAX_PATH,"%s/frame_%07i.tif",filepath,i);
		printf(" file: %s%c",filename,want_verbose?'\n':'\r');
		if (want_verbose) fflush(stdout);
		if (te[0].text) free(te[0].text); 
		if (te[1].text) free(te[1].text);
		asprintf(&(te[0].text),"Frame: %i",i);
		te[1].text=calloc(48,sizeof(char));
		frame_to_smptestring(te[1].text,i,want_dropframes);
		err = render_frame(filename,w,h,te);
	}
	snprintf(filename,MAX_PATH,"/tmp/test/frame_%04i.tif",i);
	if (te[0].text) free(te[0].text);
	if (te[1].text) free(te[1].text);

	te[0].text=strdup("time stamp");
	te[0].xpos=OSD_LEFT;
	te[0].yperc=5;

	te[1].text=strdup("movie maker");
	te[1].xpos=OSD_LEFT;
	te[1].yperc=30;

	te[2].text=strdup("(c) 2006 GPL");
	te[2].xpos=OSD_LEFT;
	te[2].yperc=80;

	te[3].text=strdup("robin@gareus.org");
	te[3].xpos=OSD_LEFT;
	te[3].yperc=100;

	return(render_frame(filename,w,h,te));

	// clean up
	if (te[0].text) free(te[0].text);
	if (te[1].text) free(te[1].text);
	if (te[2].text) free(te[2].text);
	if (te[3].text) free(te[3].text);
	free(te);
}
