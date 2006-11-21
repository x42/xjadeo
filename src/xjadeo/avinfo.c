/* xjadeo - jack video monitor
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
 * (c) 2006  Robin Gareus <robin@gareus.org>
 *
 * parts of this go back to code found "for free" on the ffmpeg mailing list:
 * thanks Paul Curtis! It basically does what 'ffmpeg -i' 
 * does, but has a couple of features that can be used in scripts.
 *
 */

#define EXIT_FAILURE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>

#include <ffmpeg/avformat.h>

char *program_name;
int want_quiet = 0;	/*< --quiet, --silent */
int want_mode  = 0;	/*< 0:xml 1:time 2:videoinfo */


static void usage (int status)
{
  printf ("%s - \
video file info\n", program_name);
  printf ("usage: %s [Options] <video-file>\n", program_name);
  printf (""
"Options:\n"
"  -h   display this help and exit\n"
"  -x   print XML (defualt).\n"
"  -t   print duration of file in seconds.\n"
"  -v   dump human readable video stream information.\n"
"  -c   dump comma separated video stream information.\n"
"       one line for each video stream:\n"
"        file-duration,file-fps,stream-fps,width,height,codec,colorspace\n"
"       Units: file-len: sec; fps: 1/sec; w&h: pixels; codec+fmt: text\n"
);
  exit (status);
}


static struct option const long_options[] =
{
  {"xml", no_argument, 0, 'x'},
  {"csv", no_argument, 0, 'c'},
  {"text", no_argument, 0, 'v'},
  {"time", no_argument, 0, 't'},
  {"duration", no_argument, 0, 't'},
  {NULL, 0, NULL, 0}
};
	
static int decode_switches (int argc, char **argv) {
  int c;
  while ((c = getopt_long (argc, argv, 
		   "h"	/* help */
		   "x"	/* xml out */
		   "v"	/* video streams csv */
		   "c"	/* video streams csv */
		   "t",	/* file duration */
		   long_options, (int *) 0)) != EOF)
  { switch (c) {
    case 't':
      want_mode = 1;
      break;
    case 'c':
      want_mode = 3;
      break;
    case 'v':
      want_mode = 2;
      break;
    case 'x':
      want_mode = 0;
      break;
    case 'h':
      usage (0);
    default:
      usage (EXIT_FAILURE);
  } } /* while switch */
  return optind;
}

	
void print_error(const char *filename, int err)
{
  switch (err) {
  case AVERROR_NUMEXPECTED:
    fprintf(stderr, "%s: Incorrect image filename syntax.\n"
		"Use '%%d' to specify the image number:\n"
		"  for img1.jpg, img2.jpg, ..., use 'img%%d.jpg';\n"
		"  for img001.jpg, img002.jpg, ..., use 'img%%03d.jpg'.\n",
		filename);
    break;
  case AVERROR_INVALIDDATA:
    fprintf(stderr, "%s: Error while parsing header\n", filename);
    break;
  case AVERROR_NOFMT:
    fprintf(stderr, "%s: Unknown format\n", filename);
    break;
  default:
    fprintf(stderr, "%s: Error while opening file\n", filename);
    break;
  }
}

int main(int argc, char *argv[])
{
  AVFormatContext *ic;
  AVFormatParameters params, *ap = &params;
  AVCodec *p;
  AVInputFormat *file_iformat = NULL;
  int err, ret, i, flags;
  char str[80];
  char *fn = NULL;
  int64_t hours, mins, secs, us;

  program_name = argv[0];

  i = decode_switches (argc, argv);

  if ((i+1)== argc) fn = argv[i];
  else usage (EXIT_FAILURE);
  
  av_register_all();

  err = av_open_input_file(&ic, fn, file_iformat, 0, ap);
  if (err < 0) {
    print_error(fn, err);
    return 1;
  }

  ret = av_find_stream_info(ic);
  if (ret < 0) {
    fprintf(stderr, "%s: could not find codec parameters\n", fn);
    return 1;
  }

#if defined(__BIG_ENDIAN__) //  (__ppc__) ?
// this cast is weird, but it works.. the bytes seem to be in 'correct' order, but the two
// 4byte-words are swapped. ?!
  int64_t dur = (int64_t) (ic->duration);
  secs = (int) ( ((double) (((dur&0xffffffff)<<32)|((dur>>32)&0xffffffff))) / (double) AV_TIME_BASE );
#else
  secs = (int) (ic->duration / AV_TIME_BASE);
#endif

  if (want_mode == 1) { /* duration only */
    printf("%lld", secs);
    return 0;
  }

  if (want_mode == 2) { /* CSV video stream info */
    for (i = 0; i < ic->nb_streams; i++) {
      AVStream *st = ic->streams[i];
      AVCodecContext *codec;
#if LIBAVFORMAT_BUILD <= 4629
      codec = &(st->codec);
#else
      codec = st->codec;
#endif
      p = avcodec_find_decoder(codec->codec_id);
      if (codec->width) {
	us = ic->duration % AV_TIME_BASE;
	mins = secs / 60;
	secs %= 60;
	hours = mins / 60;
	mins %= 60;

	printf("%02lld:%02lld:%02lld.%01lld - ", hours, mins, secs, (us * 10) / AV_TIME_BASE);
#if LIBAVFORMAT_BUILD <= 4629
	printf("fps:%.2f ", (double) codec->frame_rate / (double) codec->frame_rate_base);
#else
	printf("fps:%.2f ",1.0/av_q2d(st->time_base));
#endif
	printf("w:%d ", codec->width);
	printf("h:%d #", codec->height);
	if (p) printf("%s,", p->name);
	printf("%s", avcodec_get_pix_fmt_name(codec->pix_fmt));
	printf("\n");
	break;
      }
    }
    return 0;
  }

  if (want_mode == 3) { /* CSV video stream info */
    for (i = 0; i < ic->nb_streams; i++) {
      AVStream *st = ic->streams[i];
      AVCodecContext *codec;
#if LIBAVFORMAT_BUILD <= 4629
      codec = &(st->codec);
#else
      codec = st->codec;
#endif
      p = avcodec_find_decoder(codec->codec_id);
      if (codec->width) {
	printf("%lld,", secs);
	printf("%.2f,",1.0/av_q2d(st->time_base));
#if LIBAVFORMAT_BUILD <= 4629
	printf("%.2f,", (double) codec->frame_rate / (double) codec->frame_rate_base);
#else
	printf("%.2f,", 1/av_q2d(codec->time_base));
#endif
	printf("%d,", codec->width);
	printf("%d,", codec->height);
	if (p) printf("%s,", p->name);
	printf("%s", avcodec_get_pix_fmt_name(codec->pix_fmt));
	printf("\n");
      }
    }
    return 0;
  }

  /* XML output */

  printf("<av>\n");
  printf(" <xmlversion>0.1</xmlversion>\n");
  printf(" <length>%lld</length>\n", secs);
  printf(" <duration>%lld</duration>\n", ic->duration);
  printf(" <avtimebase>%d</avtimebase>\n", AV_TIME_BASE);

  us = ic->duration % AV_TIME_BASE;
  mins = secs / 60;
  secs %= 60;
  hours = mins / 60;
  mins %= 60;

  printf("  <time>%02lld:%02lld:%02lld.%01lld</time>\n", hours, mins, secs, (us * 10) / AV_TIME_BASE);
  printf("  <bitrate>%d</bitrate>\n", ic->bit_rate);
  printf("  <size>%lld</size>\n", ic->file_size);
  if (ic->title) printf("  <streamtitle>%s</streamtitle>\n", ic->title);

  if (ic->copyright) printf("  <streamcopyright>%s</streamcopyright>\n", ic->copyright);

  if (ic->author) printf("  <streamauthor>%s</streamauthor>\n", ic->author);

#if LIBAVFORMAT_BUILD > 4629
  printf("  <muxrate>%d</muxrate>\n", ic->mux_rate);
#endif
  printf("  <streams>%d</streams>\n", ic->nb_streams);

  for (i = 0; i < ic->nb_streams; i++) {
    AVStream *st = ic->streams[i];
      AVCodecContext *codec;
#if LIBAVFORMAT_BUILD <= 4629
      codec = &(st->codec);
#else
      codec = st->codec;
#endif
    avcodec_string(str, sizeof(str), codec, 0);
    switch (codec->codec_type) {
    case CODEC_TYPE_VIDEO:
      printf("  <video>%s</video>\n", str);
      printf("  <stream id='%d' type='video'>\n", i);
      break;
    case CODEC_TYPE_AUDIO:
      printf("  <audio>%s</audio>\n", str);
      printf("  <stream id='%d' type='audio'>\n", i);
      break;
    case CODEC_TYPE_DATA:
      printf("  <stream id='%d' type='data'>\n", i);
      break;
    default:
      printf("  <stream id='%d' type='unknown'>\n", i);
      break;
    }

    p = avcodec_find_decoder(codec->codec_id);

    flags = ic->iformat->flags;
    if (p) printf("    <codec>%s</codec>\n", p->name);
    else printf("    <codec>unknown</codec>\n");

    printf("    <duration>%lld</duration>\n", st->duration);
    printf("    <avtimebasenum>%d</avtimebasenum>\n", st->time_base.num);
    printf("    <avtimebaseden>%d</avtimebaseden>\n", st->time_base.den);
#if LIBAVFORMAT_BUILD <= 4629
    printf("    <avtimerate>%.2f</avtimerate>\n", 1.0/av_q2d(st->time_base));
#else
    printf("    <avtimerate>%.2f</avtimerate>\n", 1.0/av_q2d(st->time_base));
#endif
    printf("    <bitrate>%d</bitrate>\n", codec->bit_rate);

#if LIBAVFORMAT_BUILD > 4629
    if (flags & AVFMT_SHOW_IDS) 
      printf("    <formatid>0x%x</formatid>\n", st->id);
#endif

    if (codec->width) {
      printf("    <width>%d</width>\n", codec->width);
      printf("    <height>%d</height>\n", codec->height);
#if LIBAVFORMAT_BUILD <= 4629
      printf("    <framerate>%.2f</framerate>\n", (double) codec->frame_rate / (double) codec->frame_rate_base);
#else
      printf("    <framerate>%.2f</framerate>\n", 1/av_q2d(codec->time_base));
#endif
      printf("    <pixelformat>%s</pixelformat>\n", avcodec_get_pix_fmt_name(codec->pix_fmt));
    }

    if (codec->channels) 
      printf("    <channels>%d</channels>\n", codec->channels);

    if (codec->sample_rate)
      printf("    <samplerate>%d</samplerate>\n", codec->sample_rate);

    printf("  </stream>\n");
  }
  printf("</av>\n");
  return 0;
}

/* vi:set ts=8 sts=2 sw=2: */
