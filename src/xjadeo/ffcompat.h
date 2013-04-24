#ifdef OLD_FFMPEG
#include <avcodec.h> // needed for PIX_FMT 
#include <avformat.h>
#else
#include <libavcodec/avcodec.h> // needed for PIX_FMT 
#include <libavformat/avformat.h>
#endif

#ifdef HAVE_SWSCALE
#ifdef OLD_FFMPEG
#include <swscale.h>
#else
#include <libswscale/swscale.h>
#endif
#endif

#include <time.h>
#include <getopt.h>
#include <sys/time.h>
#include <unistd.h>

/* ffmpeg backwards compat */

#ifndef CODEC_TYPE_VIDEO
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#endif
#ifndef CODEC_TYPE_DATA
#define CODEC_TYPE_DATA AVMEDIA_TYPE_DATA
#endif
#ifndef CODEC_TYPE_AUDIO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#endif


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 123, 0)
static inline int
avcodec_open2(AVCodecContext *avctx, AVCodec *codec, void **options __attribute__((unused)))                                                     
{
  return avcodec_open(avctx, codec);
}
#endif

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52, 111, 0)
static inline int
avformat_find_stream_info(AVFormatContext *ic, void **options)
{
  return av_find_stream_info(ic);
}
#endif

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 5, 0)
static inline void
avformat_close_input(AVFormatContext **s)
{
  av_close_input_file(*s);                                                                                                                       
}

#endif


