/*
	Copyright 2009, 2010 Peter Hofmann

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


#ifndef NOTES_H
#define NOTES_H

void printNote(int slideNum);
gboolean readNotes(char *filename);
gboolean saveNotes(char *filename);
void saveCurrentNote(void);

#endif /* NOTES_H */
