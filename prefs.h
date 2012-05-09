/*
	Copyright 2010-2012 Peter Hofmann

	This file is part of pdfpres.

	pdfpres is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	pdfpres is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pdfpres. If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef PREFS_H
#define PREFS_H


#define FIT_WIDTH 0
#define FIT_HEIGHT 1
#define FIT_PAGE 2


struct _prefs
{
	int initial_fit_mode;
	int slide_context;
	gboolean do_wrapping;
	gboolean do_notectrl;
	guint cache_max;
	char *font_notes;
	char *font_timer;
	gboolean q_exits_fullscreen;
	gboolean timer_is_clock;
	gboolean stop_timer_on_fs;
};
extern struct _prefs prefs;


void loadPreferences(void);
void savePreferences(void);


#endif /* PREFS_H */
