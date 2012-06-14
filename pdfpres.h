/*
	Copyright 2009-2011 Peter Hofmann

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


#ifndef PDFPRES_H
#define PDFPRES_H

struct viewport
{
	int offset;

	int width;
	int height;

	GtkWidget *image;
	GtkWidget *frame;

	GdkPixbuf *pixbuf;

	gboolean isBeamer;
};

/* These preferences are initially loaded by prefs.c but they can be
 * overriden by command line parameters. We separate them from the other
 * preferences because command line parameters must not have any effect
 * on saved preferences.
 */
struct _runtimePreferences
{
	gboolean do_wrapping;
	gboolean do_notectrl;
	int fit_mode;
};
extern struct _runtimePreferences runpref;

extern int doc_n_pages;
extern int doc_page;
extern GtkWidget *win_preview;
extern GtkTextBuffer *noteBuffer;

#endif /* PDFPRES_H */
