/* 
 *  Copyright (C) 2006 Robin Gareus  <robin AT gareus.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <jack/jack.h>
#include <jack/transport.h>


enum { SFT_NONE = 0 , SFT_FFMPEG, SFT_QUICKTIME, SFT_SNDFILE };

int m_filetype = SFT_NONE;
int m_samplerate = 48000;    // sample rate of the file
int m_channels = 2;    // channels of the file
jack_nframes_t m_length = 0; // length of file in frames. @ orig samplerate
jack_nframes_t m_resampled_length; // length of file in jack-frames. @ jack samplerate
float m_fResampleRatio = 1.0;

// parameters
int p_phaseinvert = 1; 
int p_scrublen=  3*1920; // will be rounded to jack bufsiz
int p_scrub_audio = 1;
int p_autoconnect = 1;


// prototypes - jack.c
void dothejack(void);
void loopthejack(void);


#define min( v1, v2 ) ((v1) < (v2) ? (v1) : (v2))


//////  buffer ///////

#define CACHE_S (32768)
#define FADELEN 32 

typedef struct audiocache{
	int state; 
	jack_nframes_t start, end; 
	jack_default_audio_sample_t **d;
	struct audiocache *next;
} audiocache;


jack_nframes_t fillBuffer_cache(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames, 
				jack_nframes_t m_position, audiocache *cache, int *fadeedges )
{
	int i;
	int have_data = 0;
	jack_nframes_t written = 0;
	audiocache *cptr = cache;
	while (cptr && written < frames) {
		if ((cptr->state&1)!=1 ) { cptr=cptr->next; continue; }
		if (cptr->start <= (m_position + written) && cptr->end > (m_position+ written)) { 
//			printf("found one : (%i) %i - %i\n",(m_position+written),cptr->start,cptr->end);
			have_data = 1;
			jack_nframes_t len;
			jack_nframes_t off;
			len = min (frames-written, cptr->end-(m_position + written));
			off =  (m_position + written)-cptr->start;
//			printf("copy:  off:%i - len:%i\n",off,len);
			for (i=0;i<m_channels;i++) {
				int j,k;
      				for(k=written,j=off; j < off+len; j++, k++)
					bufferptrs[i][k]= cptr->d[i][j];
			}
			written += len;
			cptr = cache; continue; // restart cache search at new start pos.
		}
		cptr=cptr->next; 
	}

	if (*fadeedges < 0 ) return (written);

	// TODO if prev frame was padded - fade in .
	if (*fadeedges) {
		*fadeedges=0;
		int f; 
		for (f=0; f< min(written,FADELEN); f++)
			for (i=0;i<m_channels;i++) 
				bufferptrs[i][f]*=(f/FADELEN);
	}

	if (written < frames) {
//		printf("  padding %i zeros @%i\n", (frames-written),m_position);
		*fadeedges=1;
		for (i=0;i<m_channels;i++) 
			memset(bufferptrs[i]+written,0, sizeof (jack_default_audio_sample_t) * (frames-written));
	}

	if (*fadeedges) {
		int f; 
		for (f=0; f< min(written,FADELEN); f++)
			for (i=0;i<m_channels;i++) 
				bufferptrs[i][(written-f)]*=(f/FADELEN);
	}

	return (frames);
}

audiocache *newcl(audiocache *cache) {
	int i;
	audiocache *cl;

	cl=calloc(1,sizeof(audiocache));
	cl->next= NULL;
	cl->state=0;
//	cl->d = calloc(CACHE_S*m_channels,sizeof(jack_default_audio_sample_t));
	cl->d = calloc(m_channels,sizeof(jack_default_audio_sample_t*));
	for (i=0;i<m_channels;i++) 
		cl->d[i] = calloc(CACHE_S,sizeof(jack_default_audio_sample_t));

	// append node
	cl->state&=~1;
	audiocache *cptr = cache;
	while (cptr && cptr->next) cptr=cptr->next;
	if (cptr) cptr->next=cl;

	return (cl);
}

audiocache *getcl( audiocache *cache ) {
	audiocache *cptr = cache;
	while (cptr) {
		if (cptr->state == 0 ) return cptr;
		cptr=cptr->next;
	}
	return (newcl(cache));
}

void freecl(jack_nframes_t m_position, audiocache *cache ) {
	audiocache *cptr = cache;
	while (cptr) {
		if ((cptr->state&1)!=1 ) { cptr=cptr->next; continue; }
		if ((cptr->end + 5*m_samplerate ) <  m_position ) { // XXX : adjust total cache size 
			cptr->state&=~1;
		}
		cptr=cptr->next;
	}
}

void clearcache(audiocache *cache) { 
	audiocache *cptr = cache;
	while (cptr) {
		cptr->state=0;
		cptr=cptr->next;
	}
}

int testcls(jack_nframes_t p, audiocache *cache) { 
	audiocache *cptr = cache;
	while (cptr) {
		if ((cptr->state&1)!=1 ) { cptr=cptr->next; continue; }
		if (cptr->start <= p && cptr->end > p) { 
			return (1);
		}
		cptr=cptr->next;
	}
	return (0);
}


///////////////////////////////////////////////////
// FILE I/O 



/////// FFMPEG  ///////
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

AVFormatContext* m_formatContext;
AVCodecContext*  m_codecContext;
AVCodec*         m_codec;
AVPacket         m_packet;
int              m_audioStream;

void openfile_ff (char *filename) {

	av_register_all();
	avcodec_init();
	avcodec_register_all();

	AVInputFormat *file_iformat = av_find_input_format( filename );
	
	if ( av_open_input_file( &m_formatContext, filename, file_iformat, 0, NULL ) != 0 ) {
		fprintf(stderr, "This file can not be read by ffmpeg.\n" );
		return;
	}

	if ( av_find_stream_info( m_formatContext ) < 0 ) {
		fprintf(stderr, "av_find_stream_info failed\n" );
		return;
	}
	m_audioStream = -1;
	int i;
	for (i = 0; i < m_formatContext->nb_streams; i++ ) {
		if ( m_formatContext->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO ) {
			m_audioStream = i;
			break;
		}
	}
	if ( m_audioStream == -1 ) {
		fprintf(stderr, "No Audio Stream found in file\n" );
		return;
	}
	m_codecContext = m_formatContext->streams[m_audioStream]->codec;
	m_codec = avcodec_find_decoder( m_codecContext->codec_id );
	if ( m_codec == NULL ) {
		fprintf(stderr, "Codec not supported by ffmpeg\n" );
		return;
	}
	if ( avcodec_open( m_codecContext, m_codec ) < 0 ) {
		fprintf(stderr, "avcodec_open failed\n" );
		return;
	}

	printf("ffmpeg - audio tics: %i/%i [sec]\n",m_formatContext->streams[m_audioStream]->time_base.num,m_formatContext->streams[m_audioStream]->time_base.den);
	m_samplerate = m_codecContext->sample_rate;
	m_channels= m_codecContext->channels ;
	int64_t len = m_formatContext->duration - m_formatContext->start_time;
	m_length = (int64_t)( len * m_samplerate / AV_TIME_BASE );
	m_filetype = SFT_FFMPEG;
}

int16_t          m_tmpBuffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
int16_t*         m_tmpBufferStart = 0;
unsigned long    m_tmpBufferLen = 0;
int64_t 	 m_pts = 0;
double audio_clock = 0.0;
double ffjb_clock = 0.0;



void sampleseek_ff (jack_nframes_t sample) {
	// sequential only :(
	if (sample == 0) {
		av_seek_frame( m_formatContext, m_audioStream, 0, AVSEEK_FLAG_ANY| AVSEEK_FLAG_BACKWARD);
		m_tmpBufferLen = 0;
		audio_clock = 0.0;
		return;
	}

	m_tmpBufferLen = 0;
	int64_t timestamp = (int64_t) sample * (int64_t) m_formatContext->streams[m_audioStream]->time_base.den / (int64_t) m_formatContext->streams[m_audioStream]->time_base.num / (int64_t)m_samplerate;
	av_seek_frame( m_formatContext, m_audioStream, timestamp, AVSEEK_FLAG_BACKWARD);
	audio_clock = -1.0; // get from first next frame.
}

// TODO: dither samples.
void decode_int16_to_float(void * _in, jack_default_audio_sample_t ** out, int num_channels, int num_samples, int out_offset)
{
  int i, j;
  int16_t * in;
  for(i = 0; i < num_channels; i++)
    {
    if(out[i])
      {
      in = ((int16_t*)_in) + i;
      for(j = out_offset; j < out_offset+num_samples; j++)
        {	
		int16_t sval = (*in);
		if (sval>=0) out[i][j] = (float)  sval / 32768.0;
		else out[i][j] = (float) sval / 32767.0 ;
		in += num_channels;
        }
      }
    }
}


jack_nframes_t fillBuffer_ff ( jack_default_audio_sample_t ** bufferptrs, jack_nframes_t frames ) {
//	int i;

	unsigned long written = 0;
	int ret = 0;

	while ( ret >= 0 && written < frames ) {
		if ( m_tmpBufferLen > 0 ) {
			int s = min( m_tmpBufferLen / m_codecContext->channels, frames - written );
			decode_int16_to_float( m_tmpBufferStart, bufferptrs, m_codecContext->channels, s , written);
			written += s;
//			printf("DP: %i - %lu/%u\n",s,written,frames);
			s = s * m_codecContext->channels;
			m_tmpBufferStart += s;
			m_tmpBufferLen -= s;
			ret = 0;
		} else {
			ret = av_read_frame( m_formatContext, &m_packet );
			if (ret <0 ) { printf("reached end of file.\n"); break; }

			int len = m_packet.size;
			uint8_t *ptr = m_packet.data;

			int data_size;
			while ( ptr != NULL && ret >= 0 && m_packet.stream_index == m_audioStream && len > 0 ) {
				//ret = avcodec_decode_audio( m_codecContext, m_tmpBuffer, &data_size, ptr, len );
				ret = avcodec_decode_audio3( m_codecContext, m_tmpBuffer, &data_size, &m_packet );
				if ( ret < 0 ) {
					ret = 0;
					break;
				}
				len -= ret;
				ptr += ret;
//				printf("DATASIZE : %i ++ \n",data_size);

				/* if update the audio clock with the pts */
				if (audio_clock < 0.0 && m_packet.pts != AV_NOPTS_VALUE) {
					audio_clock = m_codecContext->sample_rate * av_q2d(m_formatContext->streams[m_audioStream]->time_base)*m_packet.pts;
				}
				//audio_clock += (double)data_size / (2.0 *  m_codecContext->channels * m_codecContext->sample_rate);
				audio_clock += (double)data_size / (2.0 *  m_codecContext->channels );
//				printf("CLK: %.0f %.2f\n",audio_clock,audio_clock/m_codecContext->sample_rate);


				if ( data_size > 0 ) {
					m_tmpBufferStart = m_tmpBuffer;
					m_tmpBufferLen = data_size / 2; // 16 bit words.  
				}
			}
			av_free_packet( &m_packet );
		}
	}
	ffjb_clock=audio_clock-(m_tmpBufferLen/ m_codecContext->channels);
//	printf("CLK: %.0f\n",ffjb_clock);
	return written;
}


/////// QUICKTIME ///////
#include <lqt.h>
#include <quicktime.h>

quicktime_t *m_qt = NULL;

void openfile_qt (char *lqt_sucks_filename) {

	if ( !quicktime_check_sig( lqt_sucks_filename ) ) {
		fprintf(stderr, "This is not a Quicktime audio file\n" );
		return;
	}
	m_qt = quicktime_open( lqt_sucks_filename, 1, 0 );

	if ( !m_qt ) {
		fprintf(stderr, "Could not open Quicktime file\n" );
		return;
	}
	if ( quicktime_audio_tracks( m_qt ) == 0 ) {
		fprintf(stderr, "This Quicktime file does not have a audio track\n" );
		return;
	}
	if ( !quicktime_supported_audio( m_qt, 0 ) ) {
		fprintf(stderr, "This Audio Codec is not supported\n" );
		return;
	}
	m_channels = quicktime_track_channels( m_qt, 0 );
	m_filetype = SFT_QUICKTIME;
//	fprintf(stderr, "long length? %li\n",quicktime_audio_length( m_qt, 0 ));

	m_length = quicktime_audio_length( m_qt, 0 );
	m_samplerate = quicktime_sample_rate ( m_qt, 0 );
	fprintf(stderr, "Sucessfully opened QT file len: %li trck: %i\n",quicktime_audio_length( m_qt, 0 ),
		quicktime_audio_tracks( m_qt ));
	
}
jack_nframes_t lqtpos;

void sampleseek_qt (jack_nframes_t sample) {
//	fprintf(stderr, "seek: %i\n",sample);
//	fprintf(stderr, "DDD : file is %li - seek to %u \n",quicktime_audio_position( m_qt, 0 ),sample);
//	if (m_qt) quicktime_set_audio_position( m_qt, sample, 0 );
	lqtpos=sample;
}

jack_nframes_t fillBuffer_qt( jack_default_audio_sample_t ** bufferptrs, jack_nframes_t frames ) {
	int i;
//	int64_t diff = lqt_last_audio_position( m_qt, 0 );
//	if (m_qt) lqt_decode_audio_track( m_qt, NULL, bufferptrs, frames, 0 );
//	printf("PRE : file is %li - we want @%u len: %u\n",quicktime_audio_position( m_qt, 0 ),lqtpos,frames);
		quicktime_set_audio_position( m_qt, lqtpos, 0 );
//	printf("SEEK : file is %li - we want @%u len: %u\n",quicktime_audio_position( m_qt, 0 ),lqtpos,frames);
	for (i=0;m_qt && i<m_channels;i++) {
		quicktime_set_audio_position( m_qt, lqtpos, 0 );
		quicktime_decode_audio( m_qt, NULL, bufferptrs[i], frames, i );
	}
	return (frames);
//	return ((jack_nframes_t) (lqt_last_audio_position( m_qt, 0 ) - diff));
}


//	memcpy(left_buffer,l_buffer,frames);
//	memcpy(right_buffer,r_buffer,sizeof (jack_default_audio_sample_t) * nframes);

/////// SNDFILE ///////
#include <sndfile.h>

SNDFILE * m_sndfile;
void openfile_sf (char *filename) {
	SF_INFO sfinfo;
	m_sndfile = sf_open( filename, SFM_READ, &sfinfo );	

	if ( SF_ERR_NO_ERROR != sf_error( m_sndfile ) ) {
		fprintf(stderr, "This is not a sndfile supported audio file format\n" );
		return;
	}
	if ( sfinfo.frames==0 ) {
		fprintf(stderr, "This is an empty audio file\n" );
		return;
	}
	m_channels = sfinfo.channels;
	m_samplerate = sfinfo.samplerate;
	m_length = (jack_nframes_t) sfinfo.frames;
	m_filetype = SFT_SNDFILE;
}

void sampleseek_sf (jack_nframes_t sample) {
	sf_seek( m_sndfile, sample, SEEK_SET );
}

float *interleaved = NULL;

void realloc_sf (jack_nframes_t buffersize) {
	if (interleaved) free(interleaved);
	interleaved=calloc(m_channels*buffersize, sizeof(float));
}

jack_nframes_t fillBuffer_sf( jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames ) {
	int i,ii;
	jack_nframes_t nframes;
	nframes=sf_readf_float( m_sndfile, interleaved, frames);

	for (i=0;i<nframes && i< frames;i++) 
		for (ii=0;ii<m_channels;ii++) 
			bufferptrs[ii][i]= interleaved[m_channels*i+ii];
		
	return nframes;
}

///////////////////////////////////////////////////
// FFMPEG via buffer 

// sequentially decodes audio ahead 
// and stores it in a rand access temporary cache. 
// used to play back ffmpeg files that can only
// be seeked to keyframes.

jack_nframes_t ffjb_position;
audiocache *ffjb_cache = NULL;
int ffjb_fadeedges = 0;  // fade in after padded frame - smooth edges - don't zero pad when buffering after resample
void *bufferthread(void *arg); 

/* ffmpeg buffer - read data into cache */
jack_nframes_t fillcl(audiocache *cache) {
	audiocache *cl;
	jack_nframes_t rv;
	
	cl= getcl(cache);
	cl->state&=~1; // TODO: make thread safe.
	rv=fillBuffer_ff(cl->d, CACHE_S);
	cl->end=ffjb_clock;
	cl->start=ffjb_clock;
	if (rv > ffjb_clock) { fprintf(stderr," invalid timestamp information !\n"); }
	else cl->start-= rv;
	cl->state|=1;
//	printf("read %i -> %i\n",cl->start,cl->end);
	return (rv);
}

jack_nframes_t fillBuffer_ffjb(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames )
{
	return(fillBuffer_cache(bufferptrs, frames, ffjb_position, ffjb_cache, &ffjb_fadeedges));
}

void sampleseek_ffjb (jack_nframes_t sample) {
	ffjb_position = sample;
}

void openfile_ffjb (char *filename) {
 	pthread_t pbufferthread;
	openfile_ff(filename);
	if (m_filetype == SFT_NONE) return;

	pthread_create(&pbufferthread,0,bufferthread,NULL);
}

// requires tweaking - until a proper semaphore implementation is in place :)
void *bufferthread(void *arg){ 
	int completed=0;
	sampleseek_ff(0);
	ffjb_cache = newcl(NULL);

	while (1) {
		usleep(1);
		// TODO option: never clear cache   
		freecl(ffjb_position, ffjb_cache);
		// FIXME: sleep if completed until backwards seek.
		if (completed && ffjb_position > completed) { /* printf("Zzzzz.\n"); */ usleep(100000); continue; }
		if (testcls(ffjb_position, ffjb_cache) && 
			testcls(ffjb_position + 10*CACHE_S, ffjb_cache)) { usleep(100000); continue; }
		// FIXME: don'T seek beyond EOF 
//		if (ffjb_clock < ffjb_position )
		// TODO option: never seek: reset to pos(0) and decode sequentially.
		if ((ffjb_clock-11*CACHE_S > ffjb_position ) ||
		    (ffjb_clock+11*CACHE_S < ffjb_position )) { 
//			printf("seek: from  %.0f to %i (req:%i)\n",ffjb_clock,((jack_nframes_t) ffjb_position/CACHE_S*CACHE_S) , ffjb_position);
			clearcache(ffjb_cache); // some files are just not reliably seekable :)
			sampleseek_ff((jack_nframes_t) ffjb_position/CACHE_S*CACHE_S);
			completed=0;
		}

		if (!completed)  {
			if (fillcl(ffjb_cache) == 0 ) completed=1+ffjb_position;
		} else {  usleep(10000); }
	}
	return (NULL);
}



///////////////////////////////////////////////////
// buffer resamled  

jack_nframes_t fillBuffer_resample(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames );

jack_nframes_t fillcl_resample(audiocache *cache, jack_nframes_t startclock) {
	audiocache *cl;
	jack_nframes_t rv;
	
	cl= getcl(cache);
	cl->state&=~1; 
	rv=fillBuffer_resample(cl->d, CACHE_S);  // TODO set rv to 0 if frame is zero padded !
	if (rv > 0 ) { 
		cl->start=startclock;
		cl->end=startclock;
		cl->end+= rv;
		cl->state|=1;
//		printf("read line %i - %i!\n",cl->start,cl->end);
	}
	return (rv);
}


///////////////////////////////////////////////////
// resample 

extern jack_nframes_t j_bufsiz;
extern jack_nframes_t j_srate;

#include<samplerate.h>
#include<math.h>

jack_default_audio_sample_t **j_buffer = NULL;

void jadio_realloc_resample(jack_nframes_t frames ) {	
	int i;
	printf("realloc buffer: %i frames\n",frames);
	if (j_buffer) {
		for (i=0;i<m_channels;i++) {
			free(j_buffer[i]);
		}
		free(j_buffer);
	}


	j_buffer=calloc(m_channels,sizeof(jack_default_audio_sample_t*));
	for (i=0;i<m_channels;i++) {
		j_buffer[i]=calloc((1.0+ceilf((float) frames/m_fResampleRatio)),sizeof(jack_default_audio_sample_t));
	}

	if (m_filetype== SFT_SNDFILE)
		realloc_sf((jack_nframes_t) (1.0+ceilf((float) frames/m_fResampleRatio)));
}

jack_nframes_t fillBuffer_resample(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames )
{

	SRC_DATA src_data;
	jack_nframes_t nread = (jack_nframes_t) ceilf((float)frames / m_fResampleRatio);
	jack_nframes_t rv = 0;

	switch (m_filetype) {
		case SFT_SNDFILE:
			rv=fillBuffer_sf(j_buffer, nread+1);
			break;
		case SFT_QUICKTIME:
			rv=fillBuffer_qt(j_buffer, nread+1);
			break;
		case SFT_FFMPEG:
			rv=fillBuffer_ffjb(j_buffer, nread+1);
			break;
	}
	if (rv!= nread+1) return (0);

	// Fill all resampler parameter data...
	src_data.input_frames  = nread+1;
	src_data.output_frames = frames;
	src_data.end_of_input  = (nread < 1);
	src_data.src_ratio     =  m_fResampleRatio;
	src_data.input_frames_used = 0;
	src_data.output_frames_gen = 0;

	int i;
	for (i=0;i< m_channels; i++ ) {
		src_data.data_in       = j_buffer[i];
		src_data.data_out      = bufferptrs[i];
		src_simple (&src_data, SRC_SINC_FASTEST, 1) ;
		if (p_phaseinvert) {
			int ii;
			for (ii=0; ii< frames;ii++) {
				bufferptrs[i][ii]*=-1;
			}
		}
	}

	//	fprintf(stderr," %li %li (%li - %f) \n", src_data.output_frames_gen , src_data.input_frames_used, nread, m_fResampleRatio );

	return (frames);
} 

int joffset=0;
extern jack_nframes_t j_latency;

void sampleseek_resample (jack_nframes_t sample) {
	if (sample > joffset) sample-=joffset+j_latency;//j_bufsiz; // seek ahead. 
	else sample=0;

	sample/=m_fResampleRatio; // TODO round ?!

	switch (m_filetype) {
		case SFT_SNDFILE:
			sampleseek_sf(sample); 
			break;
		case SFT_QUICKTIME:
			sampleseek_qt(sample); 
			break;
		case SFT_FFMPEG:
			sampleseek_ffjb(sample); 
			break;
	}
}


///////////////////////////////////////////////////
// jadio - callback functions called by jack

/* 
 * void jadio_realloc(jack_nframes_t frames )
 *  called if jack changes it's buffersize 
 *
 * void sampleseek (jack_nframes_t sample)
 *  called once before each fillBuffer() indicating the start position 
 *  in jack sample-rate frames.
 *
 * jack_nframes_t fillBuffer(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames );
 *  read (frames) samples at current position, and store them in jack buffers
 *
 */

#if 1 // buffer + resample

jack_nframes_t rs_position;
jack_nframes_t rs_clock;
audiocache *rs_cache = NULL;
int rs_fadeedges = 0;  // fade in after padded frame - smooth edges

void jadio_realloc(jack_nframes_t frames ) {	
	;
}

jack_nframes_t fillBuffer(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames ) {
	return(fillBuffer_cache(bufferptrs, frames, rs_position, rs_cache, &rs_fadeedges));
}

void sampleseek (jack_nframes_t sample) {
	rs_position = sample;
}

// FIXME this one got the same issues as bufferthread()
void *resamplethread(void *arg){ 
	jadio_realloc_resample(CACHE_S);
	sampleseek_resample(0);
	rs_cache = newcl(NULL);
	usleep(200000); // allow bufferthread to fill buffer...
	while (1) {
		usleep(10);
		freecl(rs_position, rs_cache);
		// FIXME: sleep if completed until backwards seek.
		if (testcls(rs_position, rs_cache) && 
			testcls(rs_position + 10*CACHE_S, rs_cache)) { usleep(100000); continue; }
 // FIXME: don'T seek beyond EOF 
//		if (rs_clock < rs_position )
		if ((rs_clock > rs_position +11*CACHE_S ) ||
		    (rs_clock+11*CACHE_S < rs_position )) { 
		        // printf("seek: from  %i to %i (req:%i)\n",rs_clock,((jack_nframes_t) rs_position/CACHE_S*CACHE_S) , rs_position);
			rs_clock= (jack_nframes_t) rs_position/CACHE_S*CACHE_S;
			clearcache(rs_cache);
		}
		sampleseek_resample(rs_clock);
		jack_nframes_t rv= fillcl_resample(rs_cache,rs_clock); 
		rs_clock+= rv;
		if (rv==0) usleep(1000); // wake up seek thread :X
	}
	return (NULL);
}


void initialize_decoder (void) {
	ffjb_fadeedges = -1;  // disable padding -> make ffmpeg-cache return a short frame if seek fails..
			 // else we will cache resampled-zero values..
 	pthread_t presamplethread;
	pthread_create(&presamplethread,0,resamplethread,NULL);
}


#elif 1 // resample evey frame JIT

void initialize_decoder (void) { ; }

void jadio_realloc(jack_nframes_t frames ) {	
	jadio_realloc_resample(frames);
};

jack_nframes_t fillBuffer(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames ) {
	return fillBuffer_resample( bufferptrs,frames);
}

void sampleseek (jack_nframes_t sample) {
	sampleseek_resample(sample);
}


#else //// no resample ////

void initialize_decoder (void) { ; }

void jadio_realloc(jack_nframes_t frames ) {	
	if (m_filetype== SFT_SNDFILE)
		realloc_sf(frames);
}

jack_nframes_t fillBuffer(jack_default_audio_sample_t **bufferptrs, jack_nframes_t frames )
{
	jack_nframes_t rv;
	// TODO: phase invert ?! if (p_phaseinvert) ...
	switch (m_filetype) {
		case SFT_SNDFILE:
			rv=fillBuffer_sf(bufferptrs, frames );
			break;
		case SFT_QUICKTIME:
			rv=fillBuffer_qt(bufferptrs, frames );
			break;
		case SFT_FFMPEG:
			rv=fillBuffer_ffjb(bufferptrs, frames );
			break;
	}
	return (rv);
} 

void sampleseek (jack_nframes_t sample) {
	switch (m_filetype) {
		case SFT_SNDFILE:
			sampleseek_sf(sample); 
			break;
		case SFT_QUICKTIME:
			sampleseek_qt(sample); 
			break;
		case SFT_FFMPEG:
			sampleseek_ffjb(sample); 
			break;
	}
}
#endif



/*
 * Main
 */

void openfile(char *filename) {
	//TODO: allow user to choose or autodetect format

	if (m_filetype == SFT_NONE)
		openfile_sf(filename);
//  if file ends with .mov: try libqt first
	if (m_filetype == SFT_NONE && strlen(filename)>4 && !strncasecmp(filename+(strlen(filename)-4),".mov",4) )
		openfile_qt(filename);
	if (m_filetype == SFT_NONE)
//		openfile_ff(filename);
		openfile_ffjb(filename);
	if (m_filetype == SFT_NONE)
		openfile_qt(filename);
	if (m_filetype == SFT_NONE) {
		fprintf(stderr, "could not play file.\n" );
		exit(1);
	}
}

#if 0
void mread (void) {
	char buf[16];
	int rx;
	if ((rx = fread(buf, sizeof(char),1,stdin)) > 0) {
		joffset++;
		printf("read: %i\n",rx);
	}
	printf("new offset: %i\n",joffset);
}
#include <sys/poll.h>
void loopthedebug(void) {
        int y=0;
        struct pollfd pfd;

        pfd.fd = 0;
        pfd.events = POLLIN | POLLPRI;
        while (1) {
       //        y=poll(&pfd,1,1000);
        //        if (y>0 && pfd.revents & (POLLIN | POLLPRI))
                  mread();
		printf("debugloop\n");
        }
}

void looptheCCCdebug(void) {
        fd_set fd;
        int max_fd=0;
        struct timeval tv = { 0, 0 };

	while (1) {
		tv.tv_sec = 1; tv.tv_usec = 0;

		FD_ZERO(&fd);
		FD_SET(0,&fd);
		max_fd=1;
		if (select(max_fd, &fd, NULL, NULL, &tv)) mread();
	}
}
#endif

int main (int argc, char **argv) {
	if (argc==3) {
		joffset=atoi(argv[2]);
	} else 
	if (argc!=2) {
		fprintf(stdout, "usage %s <file-name>\n",argv[0]);
		exit(0);
	}

	openfile(argv[1]);
	fprintf(stdout,"FILE-INFO: channels:%i srate:%i\n",m_channels,m_samplerate);	

	initialize_decoder();

	// once jack is started m_channls MUST NOT change!
	dothejack();
	loopthejack();
//	loopthedebug();

	return (0);
}
