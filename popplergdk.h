/*
	Copyright 2011 Peter Hofmann
	Copyright (C) 2005, Red Hat, Inc.

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


/* This is a temporary workaround. GDK-API has been removed from poppler
 * in poppler 0.17 (commit 149b7fe) but pdfpres relies on that API. So
 * for now, I just copied the old poppler code. Of course, this
 * workaround is likely to break.
 *
 * Unfortunately, I'm running out of free time. If you'd like to help me
 * improve pdfpres, please send me patches and/or fork the project on
 * GitHub. I'd very much appreciate it! :-)
 */


#ifndef POPPLERGDK_H
#define POPPLERGDK_H

#include <glib/poppler.h>
#if POPPLER_MINOR_VERSION > 16

#include <gdk/gdk.h>

void
poppler_page_render_to_pixbuf (PopplerPage *page,
                              int src_x, int src_y,
                              int src_width, int src_height,
                              double scale,
                              int rotation,
                              GdkPixbuf *pixbuf);

#endif /* POPPLER_MINOR_VERSION */

#endif /* POPPLERGDK_H */
