/* xjadeo -  lash interface
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
 *  Luis Garrido <luisgarrido@users.sourceforge.net>
 *
 * $Rev: $
 * $Rev  $
 */

#include "xjadeo.h"

#ifdef HAVE_LASH

//------------------------------------------------
// extern Globals (main.c)
//------------------------------------------------
extern int       loop_flag;
/* Option flags and variables */
extern char *current_file;
extern long	ts_offset;
extern long	userFrame;
extern long	dispFrame;
extern int want_quiet;
extern int want_debug;
extern int want_verbose;
extern int remote_en;
extern int remote_mode;

#ifdef HAVE_MIDI
extern int midi_clkconvert;
#endif

extern double 		delay;
extern double 		filefps;
extern int		videomode;
extern int 		seekflags;

// On screen display
extern char OSD_fontfile[1024]; 
extern char OSD_text[128];
extern int OSD_mode;

extern int OSD_fx, OSD_tx, OSD_sx, OSD_fy, OSD_sy, OSD_ty;

extern char jackid[16];

extern lash_client_t *lash_client;

void xapi_open(void *d); // command to open movie - TODO: better do movie_open, initbuffer... (remote replies)

long int lash_config_get_value_long (const lash_config_t * config) {
	const void *data = lash_config_get_value(config);
	return(*((long*) data));
}

/*************************
 * private lash functions
 */
void handle_event(lash_event_t* ev) {
	int type = lash_event_get_type(ev);
	const char*	str = lash_event_get_string(ev);
	
	
	if (type == LASH_Restore_Data_Set) {
		// FIXME - send this AFTER recv. config
		if (!want_quiet)
			printf("LASH restore data set\n");
		//lash_send_event(lash_client, lash_event_new_with_type(LASH_Restore_Data_Set));
	} else if (type == LASH_Save_Data_Set) {
		if (!want_quiet)
			printf("LASH saving data set\n");
		lash_send_event(lash_client, lash_event_new_with_type(LASH_Save_Data_Set));
	} else if (type == LASH_Quit) {
		loop_flag=0;
	} else 
		if (want_debug)
			printf ("WARNING: unhandled LASH Event t:%i s:'%s'\n",type,str);
}

void handle_config(lash_config_t* conf) {
	const char*    key      = NULL;

	key      = lash_config_get_key(conf);
	//val      = lash_config_get_value(conf);
	if (!strcmp(key,"current_file")) {
		printf("LASH config: open movie\n");
	   	xapi_open((char *) lash_config_get_value_string (conf));
	} else if (!strcmp(key,"window_size")) {
	//	printf("LASH config: window size %ix%i\n", (lash_config_get_value_int(conf)>>16)&0xffff,lash_config_get_value_int(conf)&0xffff);
		Xresize((lash_config_get_value_int(conf)>>16)&0xffff,lash_config_get_value_int(conf)&0xffff);
	} else if (!strcmp(key,"ts_offset")) {
		ts_offset= lash_config_get_value_long(conf);
	//	printf("LASH config: change offset to: %li\n",ts_offset);
	} else {
		unsigned long val_size = lash_config_get_value_size(conf);
	//	if (want_debug)
		printf ("WARNING: unhandled LASH Config.  Key = %s size: %ld\n",key,val_size);
	}
}


/*************************
 *  public lash functions
 */

void lash_setup() {
	lash_event_t *event;
	//lash_config_t *lc;
	event = lash_event_new_with_type(LASH_Client_Name);
	lash_event_set_string(event, "Xjadeo");
	lash_send_event(lash_client, event);
/*
	lc = lash_config_new_with_key("current_file");
	lash_config_set_value_string (lc, current_file);
	lash_send_config(lash_client, lc);

	lc = lash_config_new_with_key("ts_offset");
	lash_config_set_value_int (lc, ts_offset);
	lash_send_config(lash_client, lc);

	lc = lash_config_new_with_key("fps");
	lash_config_set_value_double (lc, filefps);
	lash_send_config(lash_client, lc);
*/
}
#endif

void lash_process() {
#ifdef HAVE_LASH
	lash_event_t*  ev = NULL;
	lash_config_t* conf = NULL;
	while ((ev = lash_get_event(lash_client)) != NULL) {
		handle_event(ev);
		lash_event_destroy(ev);
	}
	while ((conf = lash_get_config(lash_client)) != NULL) {
		handle_config(conf);
		lash_config_destroy(conf);
	}
#endif
}


void lcs_str(char *key, char *value) {
#ifdef HAVE_LASH
	lash_config_t *lc = lash_config_new_with_key(key);
	lash_config_set_value_string (lc, value);
	lash_send_config(lash_client, lc);
#endif
}

void lcs_long(char *key, long int value) {
#ifdef HAVE_LASH
	lash_config_t *lc;
	lc = lash_config_new_with_key(key);
	lash_config_set_value (lc, (void*) &value, sizeof(long int));
	lash_send_config(lash_client, lc);
#endif
}
void lcs_int(char *key, int value) {
#ifdef HAVE_LASH
	lash_config_t *lc;
	lc = lash_config_new_with_key(key);
	lash_config_set_value_int (lc, value);
	lash_send_config(lash_client, lc);
	//printf("DEBUG - LASH config int %s -> %i\n",key,value);
#endif
}

void lcs_dbl(char *key, double value) {
#ifdef HAVE_LASH
	lash_config_t *lc;
	lc = lash_config_new_with_key(key);
	lash_config_set_value_double (lc, value);
	lash_send_config(lash_client, lc);
	//printf("DEBUG - LASH config dbl %s -> %i\n",key,value);
#endif
}

