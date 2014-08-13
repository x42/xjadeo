/* tsmm - time stamp movie maker
 *
 * (c) 2006, 2014  Robin Gareus <robin@gareus.org>
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

#include "xjadeo.h"

#define MIN(A,B) (((A)<(B)) ? (A) : (B))
#define OSD_fontfile "/home/rgareus/.fonts/DroidSansMono.ttf"
//#define OSD_fontfile FONT_FILE

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

int want_quiet = 0;
int want_verbose = 0;

/* statisfy 'extern' variables in xjadeo/smpte.c */
int want_dropframes = 0; // _force_ drop-frame TC  default: 0
int have_dropframes = 0; // use drop-frame-timecode (use with 30000.0/1001.0 fps)
int want_autodrop = 1;   //
#ifdef HAVE_MIDI
int midi_clkconvert = 0;
#endif

double framerate = 25;

/* freetype extern vars */
extern int ST_height;
extern int ST_top;
extern unsigned char ST_image[][ST_WIDTH];
extern int ST_rightend;

typedef struct {
	char *text;
	int xpos;
	int yperc;
} text_element;

static void usage (int status)
{
	printf ("tsmm - render time-code on tiff frames\n");
	printf ("usage: tsmm <folder> [fps] [duration] [height]\n");
	printf (""
"\n  tsmm generates a tiff image-sequence, ever image  "
"\n  with the frame number and timecode rendered on a black background."
"\n  default is 25fps and a duration of 2:10:15"
"\n  example:"
"\n    mkdir /tmp/seq/ "
"\n    tsmm /tmp/seq/"
"\n    ffmpeg -f image2 -i /tmp/seq/frame_%%7d.tiff /tmp/tsmm25.avi"
"\n");
	exit (status);
}

static void set_positions (int *xalign, int *yalign, int w, int h, int xpos, int yperc) {
	if (xalign) {
		if (xpos == OSD_LEFT) *xalign = ST_PADDING; // left
		else if (xpos == OSD_RIGHT) *xalign = w-ST_PADDING-ST_rightend; // right
		else *xalign = (w-ST_rightend)/2; // center
	}
	if (yalign) {
		int fh = MIN(ST_HEIGHT, h/12);
		*yalign = (h - fh) * yperc /100.0;
	}
}


static int render_frame (char *filename, int w, int h, text_element *te)  {
	TIFF *out;
	uint32 rps;
	uint16 bps, spp ;
	uint32 imageStripsize;
	int nos;  //< number of strips
	int i, ii;
	tdata_t buf;
	tstrip_t strip;
	int xalign, yalign;

	if (!te) return 1;

	out = TIFFOpen (filename, "w");
	if (!out) {
		printf ("Could not open output file '%s'.\n", filename);
		return 1;
	}

	rps = w; spp = 3; bps = 8;

	/* Magical stuff to create the image */

	TIFFSetField (out, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField (out, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField (out, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
	TIFFSetField (out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField (out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField (out, TIFFTAG_BITSPERSAMPLE, bps);
	TIFFSetField (out, TIFFTAG_SAMPLESPERPIXEL, spp);
	TIFFSetField (out, TIFFTAG_ROWSPERSTRIP, rps);

	imageStripsize = TIFFStripSize (out);
	nos = w*h*spp/imageStripsize;
	strip = 0;

	if (nos != 1) {
		printf ("image size needs to be mult. of 8, or image is too large.\n");
		return (1);
	}

	if (want_verbose) {
		printf ("myInfo:  w:%u h:%u bps:%u spp:%u rps:%u\n", w, h, bps, spp, rps);
		printf ("extInfo: nos:%u strip-size:%u\n", nos, imageStripsize);
	}

	buf = _TIFFmalloc (imageStripsize);
	memset (buf, 0, imageStripsize);

	ii = 0;

	while (te[ii].text) {
		int x, y;
		if (render_font (OSD_fontfile, te[ii].text, h/12, 0))
			return 1;
		const int fh = ST_height;
		const int fo = ST_HEIGHT - 8 - ST_top;
		set_positions (&xalign, &yalign, w, h, te[ii].xpos, te[ii].yperc);
		for (x = 0; x < ST_rightend && (x + xalign) < w; ++x) {
			for (y = 0; y < fh && (y + yalign) < h; ++y) {
				if (ST_image[y+fo][x] >= 1) {
					i = spp* ((x + xalign) + w * (y + yalign));
					((uint8*)buf)[i + 0] = ST_image[y + fo][x];
					((uint8*)buf)[i + 1] = ST_image[y + fo][x];
					((uint8*)buf)[i + 2] = ST_image[y + fo][x];
				}
			}
		}
		++ii;
	}

	TIFFWriteEncodedStrip (out, strip, buf, imageStripsize);
	_TIFFfree (buf);

	TIFFClose (out);
	return 0;
}

static int test_dir (char *d) {
	struct stat s;
	int result = stat (d, &s);
	if (result != 0) return 1; /* stat() failed */
	if (!S_ISDIR(s.st_mode)) return 1; /* is not a directory file */
	if (s.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))  return 0; /* is writeable */
	return 1;
}

int main (int argc, char **argv) {
	char filename[MAX_PATH];
	char *filepath = NULL;
	text_element *te = calloc (5, sizeof (text_element));
	int i;
	int err = 0;
	int last_frame = 150;
	int w = 640;
	int h = 360;

	if (argc < 2) usage (1);
	if (argc > 1) filepath = argv[1];
	if (argc > 2) {
		framerate = atof (argv[2]);
		if (framerate == 25) want_dropframes = 0;
		else if (framerate == 29.97) want_dropframes = 1;
		else if (framerate == 24) want_dropframes = 0;
		else if (framerate == 30) want_dropframes = 0;
		else {
			fprintf (stderr, "\nWARNING: untested video framerate!\n\n");
		}
	}
	if (argc > 3) {
		last_frame = smptestring_to_frame (argv[3]);
	} else {
		last_frame = smptestring_to_frame ("2:10:15");
	}
	if (argc > 4) {
		h = atoi (argv[4]);
		w = h * 16 / 9;
		if (w < 352 || w > 2048) {
			fprintf (stderr, "\nWARNING: untested video geometry!\n\n");
		}
	}
	if (argc > 5) usage (1);

	if (test_dir (filepath)) {
		printf ("Directory '%s' does not exist or is not writable.\n", filepath);
		return 1;
	}

	printf ("Writing %s/frame_%07i.tif -> %s/frame_%07i.tif\n",
			filepath, 0, filepath, last_frame);
	printf ("waiting 2 sec. press CTRL-C to interrupt.\n");
	fflush (stdout);
	sleep (2);

	te[0].xpos = OSD_CENTER;
	te[0].yperc = 15;
	te[1].xpos = OSD_CENTER;
	te[1].yperc = 88;

	// first frame / add info
	snprintf (filename, MAX_PATH, "%s/frame_%07i.tif", filepath, 0);

	asprintf (&(te[0].text), "Frame: %i", 0);
	te[1].text = calloc (48, sizeof (char));
	frame_to_smptestring (te[1].text, 0, 0);

	te[2].text = calloc (48, sizeof (char));
	sprintf (te[2].text, "Duration: ");
	frame_to_smptestring (te[2].text+10, last_frame, 0);
	te[2].xpos = OSD_CENTER;
	te[2].yperc = 30;

	asprintf (&(te[3].text), "FPS: %g", framerate);
	te[3].xpos = OSD_CENTER;
	te[3].yperc = 70;

#ifdef OFFSET
	int offset = framerate * 15;
	strcpy (te[1].text, "offset: ");
	frame_to_smptestring (&te[1].text[strlen (te[1].text)], offset, 0);
#else
	frame_to_smptestring (te[1].text, 0, 0);
#endif
	render_frame (filename, w, h, te);

	free (te[2].text); te[2].text = NULL;
	free (te[3].text); te[3].text = NULL;

	// prepare loop

#ifdef OFFSET
	te[2].xpos = OSD_CENTER;
	te[2].yperc = 45;
	te[2].text = calloc (48, sizeof (char));
	strcat (te[2].text, "start: ")
		frame_to_smptestring (&te[2].text[strlen (te[2].text)], offset, 0);
	int fcnt = 1;
#endif

	for (i = 1; i < last_frame && !err; ++i) {
		snprintf (filename, MAX_PATH, "%s/frame_%07i.tif", filepath, i);
		if (!want_quiet && !(i%7)) {
			printf (" file: %s%s", filename, want_verbose ? "\n" : "        \r");
			fflush (stdout);
		}
		if (want_verbose) fflush (stdout);

		if (te[0].text) free (te[0].text);
		if (te[1].text) free (te[1].text);
#ifndef OFFSET
		asprintf (&(te[0].text), "Frame: %i", i);
		te[1].text = calloc (48, sizeof (char));
		frame_to_smptestring (te[1].text, i, 0);
#else
		int f = i;
		int stopframe = 0;
		te[1].text = calloc (64, sizeof (char));
		frame_to_smptestring (te[1].text, f + offset, 0);
		asprintf (&(te[0].text), "Frame: %4d (rel: %4d)", fcnt, i);
		++fcnt;
#endif
		err = render_frame (filename, w, h, te);
	}

	if (te[0].text) free (te[0].text);
	if (te[1].text) free (te[1].text);
#ifdef OFFSET
	if (te[2].text) free (te[2].text);
#endif

	/* end */
	te[0].text = strdup ("Time Stamp");
	te[0].xpos = OSD_CENTER;
	te[0].yperc = 20;

	te[1].text = strdup ("Movie Maker");
	te[1].xpos = OSD_CENTER;
	te[1].yperc = 30;

	te[2].text = strdup ("(c) 2006, 2014 GPL");
	te[2].xpos = OSD_CENTER;
	te[2].yperc = 70;

	te[3].text = strdup ("robin@gareus.org");
	te[3].xpos = OSD_CENTER;
	te[3].yperc = 80;

	snprintf (filename, MAX_PATH, "/%s/frame_%07i.tif", filepath, i);
	render_frame (filename, w, h, te); i++;
	snprintf (filename, MAX_PATH, "/%s/frame_%07i.tif", filepath, i);
	render_frame (filename, w, h, te); i++;
	snprintf (filename, MAX_PATH, "/%s/frame_%07i.tif", filepath, i);
	render_frame (filename, w, h, te);

	// clean up
	if (te[0].text) free (te[0].text);
	if (te[1].text) free (te[1].text);
	if (te[2].text) free (te[2].text);
	if (te[3].text) free (te[3].text);
	free (te);
	return 0;
}
