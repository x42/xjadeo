/* xjadeo - common jack abstraction
 *
 * (C) 2010-2014 Robin Gareus <robin@gareus.org>
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
#include "xjadeo.h"

extern int jack_autostart;
extern int interaction_override;
extern int want_quiet;
extern int want_debug;
extern int want_verbose;

static jack_client_t *xj_jack_client = NULL;

#ifdef JACK_SESSION
#include <jack/session.h>
extern char *jack_uuid;
extern int	loop_flag;

static void jack_session_cb (jack_session_event_t *event, void *arg) {
	char filename[256];
	char command[256];
	if (interaction_override&OVR_JSESSION) {
		/* DO NOT SAVE SESSION
		 * e.g. if xjadeo will be restored by wrapper-program
		 * f.i. ardour3+videotimeline
		 */
		WJACK_session_reply (xj_jack_client, event);
		WJACK_session_event_free (event);
		return;
	}

	snprintf(filename, sizeof(filename), "%sxjadeo.state", event->session_dir);
	snprintf(command,  sizeof(command),  "xjadeo -U %s --rc ${SESSION_DIR}xjadeo.state", event->client_uuid);

	saveconfig (filename);

	event->command_line = strdup (command);
	WJACK_session_reply (xj_jack_client, event);
	if (event->type == JackSessionSaveAndQuit)
		loop_flag=0;
	WJACK_session_event_free (event);
}
#endif

void xj_shutdown_jack () {
	xj_jack_client = NULL;
}

void xj_close_jack (void *client_pointer) {
	jack_client_t **client = (jack_client_t**) client_pointer;
	xj_jack_client = NULL;
	if (*client) {
		WJACK_deactivate (*client);
		WJACK_client_close (*client);
	}
	*client = NULL;
}

int xj_init_jack (void *client_pointer, const char *client_name) {
	jack_client_t **client = (jack_client_t**) client_pointer;
	jack_status_t status;
	jack_options_t options = jack_autostart ? JackNullOption : JackNoStartServer;
	*client = NULL;
	xj_jack_client = NULL;

#ifdef JACK_SESSION
	if (jack_uuid)
		*client = WJACK_client_open2 (client_name, options|JackSessionID, &status, jack_uuid);
	else
#endif
		*client = WJACK_client_open1 (client_name, options, &status);

	if (!*client) {
		if (!want_quiet) {
			fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
		}
		return -1;
	}

	xj_jack_client = *client;

	if (want_verbose && (status & JackServerStarted)) {
		fprintf (stdout, "JACK server was started.\n");
	}
	if (want_verbose && (status & JackNameNotUnique)) {
		fprintf (stdout, "JACK client name was not unique\n");
	}
	if (want_verbose) {
		client_name = WJACK_get_client_name (*client);
		fprintf (stdout, "JACK client name: `%s'\n", client_name);
	}

#ifdef JACK_SESSION
	WJACK_set_session_callback (*client, jack_session_cb, NULL);
#endif
	return 0;
}

const char *xj_jack_client_name() {
	if (!xj_jack_client) return "N/A";
	return WJACK_get_client_name(xj_jack_client);
}
