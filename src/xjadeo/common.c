/* xjadeo - common access functions
 *
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

#include "xjadeo.h"

/// here ??
extern int64_t frames;
extern int64_t userFrame;
extern int interaction_override;
extern int force_redraw;
extern int OSD_mode; // change via keystroke
extern int OSD_fx, OSD_fy;
extern int OSD_sx, OSD_sy;
extern int OSD_tx, OSD_ty;

void INT_sync_to_jack (int remote_msg) {
#ifdef HAVE_MIDI
	if (midi_connected()) midi_close();
#endif
#ifdef HAVE_LTC
	if (ltcjack_connected()) close_ltcjack();
#endif
	open_jack();
	if (remote_msg) {
		if (jack_connected())
			remote_printf (100,"connected to jack server.");
		else
			remote_printf (405,"failed to connect to jack server");
	}
}

void INT_sync_to_ltc (char *port, int remote_msg) {
	if (jack_connected()) close_jack();
#ifdef HAVE_MIDI
	if (midi_connected()) midi_close();
#endif
#ifdef HAVE_LTC
	if (!ltcjack_connected()) {
		open_ltcjack (port);
	}
	if (remote_msg) {
		if (ltcjack_connected())
			remote_printf (100,"opened LTC jack port.");
		else
			remote_printf (405,"failed to connect to jack server");
	}
#else
	if (remote_msg)
		remote_printf (499,"LTC-jack is not available.");
#endif
}

void ui_sync_none () {
	if (interaction_override&OVR_MENUSYNC) return;
	if (jack_connected()) close_jack();
#ifdef HAVE_MIDI
	if (midi_connected()) midi_close();
#endif
#ifdef HAVE_LTC
	if (ltcjack_connected()) close_ltcjack();
#endif
}

void ui_sync_manual (float percent) {
	if (interaction_override&OVR_MENUSYNC) return;
	if (frames < 1) return;
	ui_sync_none();
	if (percent <= 0.f) percent = 0.f;
	if (percent >= 100.f) percent = 100.f;
	userFrame = rint((frames - 1.f) * percent / 100.f);
}

void ui_sync_to_jack () {
	if (interaction_override&OVR_MENUSYNC) return;
	INT_sync_to_jack (0);
}

void ui_sync_to_ltc () {
	if (interaction_override&OVR_MENUSYNC) return;
	INT_sync_to_ltc (NULL, 0);
}

static void ui_sync_to_mtc (const char *driver) {
	if (interaction_override&OVR_MENUSYNC) return;
	if (jack_connected()) close_jack();
#ifdef HAVE_LTC
	if (ltcjack_connected()) close_ltcjack();
#endif
#ifdef HAVE_MIDI
	if (midi_connected() && strcmp (midi_driver_name(), driver)) {
		midi_close();
	}
	if (!midi_connected()) {
		midi_choose_driver (driver);
		midi_open ("-1");
	}
#endif
}

void ui_sync_to_mtc_jack () {
	ui_sync_to_mtc ("JACK-MIDI");
}
void ui_sync_to_mtc_portmidi () {
	ui_sync_to_mtc ("PORTMIDI");
}
void ui_sync_to_mtc_alsaraw () {
	ui_sync_to_mtc ("ALSA-RAW-MIDI");
}
void ui_sync_to_mtc_alsaseq () {
	ui_sync_to_mtc ("ALSA-Sequencer");
}

enum SyncSource ui_syncsource() {
	if (jack_connected()) {
		return SYNC_JACK;
	}
#ifdef HAVE_MIDI
	else if (ltcjack_connected()) {
		return SYNC_LTC;
	}
#endif
#ifdef HAVE_MIDI
	else if (midi_connected() && !strcmp (midi_driver_name(), "PORTMIDI")) {
		return SYNC_MTC_PORTMIDI;
	}
	else if (midi_connected() && !strcmp (midi_driver_name(), "JACK-MIDI")) {
		return SYNC_MTC_JACK;
	}
	else if (midi_connected() && !strcmp (midi_driver_name(), "ALSA-RAW-MIDI")) {
		return SYNC_MTC_ALSARAW;
	}
	else if (midi_connected() && !strcmp (midi_driver_name(), "ALSA-Sequencer")) {
		return SYNC_MTC_ALSASEQ;
	}
#endif
	else {
	}
	return SYNC_NONE;
}

void ui_osd_clear () {
	OSD_mode = OSD_BOX; // XXX retain message when indexing or file closed?
	force_redraw = 1;
}

void ui_osd_offset_cycle () {
	OSD_mode &= ~(OSD_NFO | OSD_GEO);
	if (OSD_mode & OSD_OFFF) {
		OSD_mode &= ~(OSD_OFFF | OSD_OFFS);
	}
	else if (OSD_mode & OSD_OFFS) {
		OSD_mode &= ~OSD_OFFS;
		OSD_mode |= OSD_OFFF;
	} else {
		OSD_mode &= ~OSD_OFFF;
		OSD_mode |= OSD_OFFS;
	}
	force_redraw = 1;
}

void ui_osd_offset_tc () {
	OSD_mode &= ~(OSD_OFFF | OSD_NFO | OSD_GEO);
	OSD_mode |= OSD_OFFS;
	force_redraw = 1;
}

void ui_osd_offset_fn () {
	OSD_mode &= ~(OSD_OFFS | OSD_NFO | OSD_GEO);
	OSD_mode |= OSD_OFFF;
	force_redraw = 1;
}

void ui_osd_offset_none () {
	OSD_mode &= ~(OSD_OFFF | OSD_OFFS);
	force_redraw = 1;
}

void ui_osd_tc () {
	OSD_mode ^= OSD_SMPTE;
	force_redraw = 1;
}

void ui_osd_fn () {
	if (OSD_mode & OSD_FRAME) {
		OSD_mode &= ~(OSD_VTC | OSD_FRAME);
	} else if (OSD_mode & OSD_VTC) {
		OSD_mode &= ~OSD_VTC;
		OSD_mode |= OSD_FRAME;
	} else {
		OSD_mode &= ~OSD_FRAME;
		OSD_mode |= OSD_VTC;
	}
	force_redraw = 1;
}

void ui_osd_vtc_fn () {
	OSD_mode &= ~OSD_VTC;
	OSD_mode |= OSD_FRAME;
	force_redraw = 1;
}

void ui_osd_vtc_tc () {
	OSD_mode &= ~OSD_FRAME;
	OSD_mode |= OSD_VTC;
	force_redraw = 1;
}

void ui_osd_vtc_off () {
	OSD_mode &= ~(OSD_VTC | OSD_FRAME);
	force_redraw = 1;
}

void ui_osd_box () {
	OSD_mode ^= OSD_BOX;
	force_redraw = 1;
}

void ui_osd_geo () {
	ui_osd_offset_none();
	OSD_mode &= ~OSD_NFO;
	OSD_mode ^= OSD_GEO;
	force_redraw = 1;
}

void ui_osd_fileinfo () {
	ui_osd_offset_none();
	OSD_mode &= ~OSD_GEO;
	OSD_mode ^= OSD_NFO;
	force_redraw = 1;
}

void ui_osd_pos () {
	OSD_mode ^= OSD_POS;
	force_redraw = 1;
}

void ui_osd_outofrange () {
	OSD_mode ^= OSD_VTCOOR;
	if (OSD_mode & OSD_VTCOOR) {
		OSD_mode |= OSD_VTC;
		OSD_mode &= ~OSD_FRAME;
	}
	force_redraw = 1;
}

void ui_osd_permute () {
	const int t1 = OSD_sy;
	OSD_sy = OSD_fy;
	OSD_fy = t1;
	force_redraw = 1;
}
