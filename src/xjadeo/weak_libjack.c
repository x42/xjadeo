/* xjadeo - weak dynamic JACK linking
 *
 * (C) 2014 Robin Gareus <robin@gareus.org>
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

#include "weak_libjack.h"

#ifndef USE_WEAK_JACK

int init_weak_jack() {
	return 0;
}

#else

#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern int want_quiet;
extern int want_debug;

static void* lib_open(const char* const so) {
#ifdef PLATFORM_WINDOWS
	return (void*) LoadLibraryA(so);
#else
	return dlopen(so, RTLD_NOW|RTLD_LOCAL);
#endif
}

static void* lib_symbol(void* const lib, const char* const sym) {
#ifdef PLATFORM_WINDOWS
	return (void*) GetProcAddress((HMODULE)lib, sym);
#else
	return dlsym(lib, sym);
#endif
}

static struct WeakJack {
	void * _client_open;
	void * _client_close;
	void * _get_client_name;

	void * _get_sample_rate;
	void * _frames_since_cycle_start;

	void * _set_graph_order_callback;
	void * _set_process_callback;
	void * _on_shutdown;

	void * _activate;
	void * _deactivate;

	void * _port_get_total_latency;
	void * _port_get_latency_range;
	void * _port_get_buffer;

	void * _port_name;
	void * _get_ports;
	void * _port_register;
	void * _connect;
	void * _free;

	void * _transport_locate;
	void * _transport_start;
	void * _transport_stop;
	void * _transport_query;

#ifdef HAVE_JACKMIDI
	void * _midi_get_event_count;
	void * _midi_event_get;
#endif
#ifdef JACK_SESSION
	void * _set_session_callback;
	void * _session_reply;
	void * _session_event_free;
#endif
} _j;


int init_weak_jack() {
	void* lib;
	memset(&_j, 0, sizeof(_j));

#ifdef PLATFORM_OSX
	lib = lib_open("libjack.dylib");
#elif (defined PLATFORM_WINDOWS)
	lib = lib_open("libjack.dll");
#else
	lib = lib_open("libjack.so.0");
#endif
	if (!lib) {
		return -1;
	}

#define MAPSYM(SYM, FAIL) _j._ ## SYM = lib_symbol(lib, "jack_" # SYM); \
	if (!_j._ ## SYM) err |= FAIL;\
	if (!_j._ ## SYM && !want_quiet) fprintf(stderr, "JACK symbol '%s' was not found.\n", "" #SYM); \
	if (_j._ ## SYM && want_debug) fprintf(stderr, "mapped JACK symbol '%s'.\n", "" #SYM);

	int err = 0;

	MAPSYM(client_open, 2)
	MAPSYM(client_close, 1)
	MAPSYM(get_client_name, 1)
	MAPSYM(get_sample_rate, 1)
	MAPSYM(frames_since_cycle_start, 1)
	MAPSYM(set_graph_order_callback, 1)
	MAPSYM(set_process_callback, 1)
	MAPSYM(on_shutdown,0)
	MAPSYM(activate, 1)
	MAPSYM(deactivate, 1)
	MAPSYM(port_get_total_latency, 0)
	MAPSYM(port_get_latency_range, 0)
	MAPSYM(port_get_buffer, 1)
	MAPSYM(port_name, 1)
	MAPSYM(get_ports, 1)
	MAPSYM(port_register, 1)
	MAPSYM(connect, 1)
	MAPSYM(free, 0)
	MAPSYM(transport_locate, 1)
	MAPSYM(transport_start, 1)
	MAPSYM(transport_stop, 1)
	MAPSYM(transport_query, 1)
#ifdef HAVE_JACKMIDI
	MAPSYM(midi_get_event_count, 1)
	MAPSYM(midi_event_get, 1)
#endif
#ifdef JACK_SESSION
	MAPSYM(set_session_callback, 0)
	MAPSYM(session_reply, 0)
	MAPSYM(session_event_free, 0)
#endif

	/* if some required symbol is not found, disable JACK completly */
	if (err) {
		_j._client_open = NULL;
	}

	return err;
}

/* abstraction for jack_client functions */
#define JCFUN(RTYPE, NAME, RVAL) \
	RTYPE WJACK_ ## NAME (jack_client_t *client) { \
		if (_j._ ## NAME) { \
			return ((RTYPE (*)(jack_client_t *client)) _j._ ## NAME)(client); \
		} else { \
			return RVAL; \
		} \
	}

/* abstraction for NOOP functions */
#define JPFUN(RTYPE, NAME, DEF, ARGS, RVAL) \
	RTYPE WJACK_ ## NAME DEF { \
		if (_j._ ## NAME) { \
			return ((RTYPE (*)DEF) _j._ ## NAME) ARGS; \
		} else { \
			return RVAL; \
		} \
	}

/* abstraction for functions with return-value-pointer args */
#define JXFUN(RTYPE, NAME, DEF, ARGS, CODE) \
	RTYPE WJACK_ ## NAME DEF { \
		if (_j._ ## NAME) { \
			return ((RTYPE (*)DEF) _j._ ## NAME) ARGS; \
		} else { \
			CODE \
		} \
	}

/* <jack/jack.h> */

/* expand ellipsis for jack-session */
jack_client_t * WJACK_client_open2 (const char *client_name, jack_options_t options, jack_status_t *status, char *uuid) {
	if (_j._client_open) {
		return ((jack_client_t* (*)(const char *client_name, jack_options_t options, jack_status_t *status, ...))_j._client_open)(client_name, options, status, uuid);
	} else {
		if (status) *status = 0;
		return NULL;
	}
}

jack_client_t * WJACK_client_open1 (const char *client_name, jack_options_t options, jack_status_t *status) {
	if (_j._client_open) {
		return ((jack_client_t* (*)(const char *client_name, jack_options_t options, jack_status_t *status, ...))_j._client_open)(client_name, options, status);
	} else {
		if (status) *status = 0;
		return NULL;
	}
}


JCFUN(int,   client_close, 0);
JCFUN(char*, get_client_name, NULL);

JCFUN(jack_nframes_t, get_sample_rate, 0);
JPFUN(jack_nframes_t, frames_since_cycle_start, (const jack_client_t *c), (c),  0);

JPFUN(int,  set_graph_order_callback, (jack_client_t *c, JackGraphOrderCallback g, void *a), (c,g,a), -1);
JPFUN(int,  set_process_callback, (jack_client_t *c, JackProcessCallback p, void *a), (c,p,a), -1);
JPFUN(void, on_shutdown, (jack_client_t *c, JackShutdownCallback s, void *a), (c,s,a), );

JCFUN(int, activate, -1);
JCFUN(int, deactivate, -1);

JPFUN(jack_nframes_t, port_get_total_latency, (jack_client_t *c, jack_port_t *p), (c,p), 0);
JXFUN(void,           port_get_latency_range, (jack_port_t *p, jack_latency_callback_mode_t m, jack_latency_range_t *r), (p,m,r), if (r) {r->min = r->max = 0;});
JPFUN(void*,          port_get_buffer, (jack_port_t *p, jack_nframes_t n), (p,n), NULL);

JPFUN(const char*,  port_name, (const jack_port_t *p), (p), NULL);
JPFUN(const char**, get_ports,(jack_client_t *c, const char *p, const char *t, unsigned long f), (c,p,t,f), NULL);
JPFUN(jack_port_t*, port_register, (jack_client_t *c, const char *n, const char *t, unsigned long f, unsigned long b), (c,n,t,f,b), NULL);
JPFUN(int,          connect, (jack_client_t *c, const char *s, const char *d), (c,s,d), -1);
JXFUN(void,         free, (void *p), (p), free(p););

JPFUN(int,  transport_locate, (jack_client_t *c, jack_nframes_t f), (c,f), 0);
JCFUN(void, transport_start, );
JCFUN(void, transport_stop, );
JXFUN(jack_transport_state_t, transport_query, (const jack_client_t *c, jack_position_t *p), (c,p), memset(p, 0, sizeof(jack_position_t)); return 0;);


#ifdef HAVE_JACKMIDI
/* <jack/midiport.h> */
JPFUN(uint32_t, midi_get_event_count, (void* p), (p), 0);
JPFUN(int, midi_event_get, (jack_midi_event_t *e, void *p, uint32_t i), (e,p,i), -1);
#endif

#ifdef JACK_SESSION
/* <jack/session.h> */
JPFUN(int, set_session_callback, (jack_client_t *c, JackSessionCallback s, void *a), (c,s,a), -1);
JPFUN(int, session_reply, (jack_client_t *c, jack_session_event_t *e), (c,e), -1);
JPFUN(void, session_event_free, (jack_session_event_t *e), (e), );
#endif

#endif
