/*
	Copyright 2010 Peter Hofmann

	This file is part of pdfPres.

	pdfPres is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	pdfPres is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pdfPres. If not, see <http://www.gnu.org/licenses/>.
*/


#include <gtk/gtk.h>

#include "prefs.h"


struct _prefs prefs;

void loadPreferences(void)
{
	/* Init default settings. */
	prefs.initial_fit_mode = FIT_PAGE;
	prefs.slide_context = 1;
	prefs.do_wrapping = FALSE;
	prefs.do_notectrl = FALSE;
	prefs.cache_max = 32;
	prefs.font_notes = g_strdup("Sans 12");
	prefs.font_timer = g_strdup("Sans 35");
	/* We're using g_strdup() here so we can use g_free() all the time.
	 */

	/* TODO: Try to load settings from file. */
}

void savePreferences(void)
{
	/* TODO: Create directory structure: ~/.config/pdfPres/ */
	/* TODO: Save settings to ~/.config/pdfPres/pdfPres.conf */
}
