/*
	Copyright 2009, 2010 Peter Hofmann

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


#ifndef PDFPRES_H
#define PDFPRES_H

extern int doc_n_pages;
extern int doc_page;
extern GtkWidget *win_preview;
extern GtkTextBuffer *noteBuffer;

void dieOnNull(void *ptr, int line);
void setStatusText_strdup(char *msg);

#endif /* PDFPRES_H */
