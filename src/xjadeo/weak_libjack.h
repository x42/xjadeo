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
#ifndef XJADEO_WEAK_JACK_H
#define XJADEO_WEAK_JACK_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include<jack/jack.h>
#include <jack/transport.h>

#ifdef HAVE_JACKMIDI
#include <jack/midiport.h>
#endif

#ifdef JACK_SESSION
#include <jack/session.h>
#endif

#ifndef USE_WEAK_JACK

#define WJACK_client_open1 jack_client_open
#define WJACK_client_open2 jack_client_open
#define WJACK_client_close jack_client_close

#define WJACK_get_client_name jack_get_client_name
#define WJACK_get_sample_rate jack_get_sample_rate

#define WJACK_frames_since_cycle_start jack_frames_since_cycle_start
#define WJACK_set_process_callback jack_set_process_callback
#define WJACK_set_graph_order_callback jack_set_graph_order_callback
#define WJACK_on_shutdown jack_on_shutdown

#define WJACK_activate jack_activate
#define WJACK_deactivate jack_deactivate

#define WJACK_port_get_total_latency jack_port_get_total_latency
#define WJACK_port_get_latency_range jack_port_get_latency_range
#define WJACK_port_get_buffer jack_port_get_buffer

#define WJACK_port_name jack_port_name
#define WJACK_get_ports jack_get_ports
#define WJACK_port_register jack_port_register
#define WJACK_connect jack_connect
#define WJACK_free jack_free

#define WJACK_transport_locate jack_transport_locate
#define WJACK_transport_start jack_transport_start
#define WJACK_transport_stop jack_transport_stop
#define WJACK_transport_query jack_transport_query

#define WJACK_midi_get_event_count jack_midi_get_event_count
#define WJACK_midi_event_get jack_midi_event_get

#define WJACK_set_session_callback jack_set_session_callback
#define WJACK_session_reply jack_session_reply
#define WJACK_session_event_free jack_session_event_free

#else

/* <jack/jack.h> */
jack_client_t * WJACK_client_open2 (const char *client_name, jack_options_t options, jack_status_t *status, char *uuid);
jack_client_t * WJACK_client_open1 (const char *client_name, jack_options_t options, jack_status_t *status);

int WJACK_client_close (jack_client_t *client);
char * WJACK_get_client_name (jack_client_t *client);

jack_nframes_t WJACK_get_sample_rate (jack_client_t *client);
jack_nframes_t WJACK_frames_since_cycle_start (const jack_client_t *client);

int WJACK_set_graph_order_callback (jack_client_t *client, JackGraphOrderCallback graph_callback, void *arg);
int WJACK_set_process_callback (jack_client_t *client, JackProcessCallback process_callback, void *arg);
void WJACK_on_shutdown (jack_client_t *client, JackShutdownCallback shutdown_callback, void *arg);

int WJACK_activate (jack_client_t *client);
int WJACK_deactivate (jack_client_t *client);

#ifndef NEW_JACK_LATENCY_API
jack_nframes_t WJACK_port_get_total_latency (jack_client_t *client, jack_port_t *port);
#else
void WJACK_port_get_latency_range (jack_port_t *port, jack_latency_callback_mode_t mode, jack_latency_range_t *range);
#endif
void * WJACK_port_get_buffer (jack_port_t *port, jack_nframes_t unused);

const char * WJACK_port_name (const jack_port_t *port);
const char ** WJACK_get_ports (jack_client_t *client, const char *port_name_pattern, const char *type_name_pattern, unsigned long flags);
jack_port_t * WJACK_port_register (jack_client_t *client, const char *port_name, const char *port_type, unsigned long flags, unsigned long buffer_size);
int WJACK_connect (jack_client_t *client, const char *source_port, const char *destination_port);
void WJACK_free (void *ptr);

/* <jack/transport.h> */
int WJACK_transport_locate (jack_client_t *client, jack_nframes_t frame);
void WJACK_transport_start (jack_client_t *client);
void WJACK_transport_stop (jack_client_t *client);
jack_transport_state_t WJACK_transport_query (const jack_client_t *client, jack_position_t *pos);

#ifdef HAVE_JACKMIDI
/* <jack/midiport.h> */
uint32_t WJACK_midi_get_event_count(void* port_buffer);
int WJACK_midi_event_get(jack_midi_event_t *event, void *port_buffer, uint32_t event_index);
#endif

#ifdef JACK_SESSION
/* <jack/session.h> */
int WJACK_set_session_callback (jack_client_t *client, JackSessionCallback session_callback, void *arg);
int WJACK_session_reply (jack_client_t *client, jack_session_event_t *event);
void WJACK_session_event_free (jack_session_event_t *event);
#endif

#endif // USE_WEAK_JACK

#endif // XJADEO_WEAK_JACK_H
