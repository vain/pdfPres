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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <libxml/xmlwriter.h>

#include "notes.h"
#include "pdfPres.h"


static gchar **notes = NULL;


void initNotes(void)
{
	int i;

	/* if there were some notes before, kill em. */
	if (notes != NULL)
		g_strfreev(notes);

	/* prepare notes array -- this can be free'd with g_strfreev */
	notes = (gchar **)malloc((doc_n_pages + 1) * sizeof(gchar *));
	for (i = 0; i < doc_n_pages; i++)
	{
		notes[i] = g_strdup("");
	}
	notes[doc_n_pages] = NULL;
}

void printNote(int slideNum)
{
	if (notes == NULL)
	{
		return;
	}

	slideNum--;
	if (slideNum < 0 || slideNum >= doc_n_pages)
	{
		return;
	}

	/* push text into buffer */
	gtk_text_buffer_set_text(noteBuffer, notes[slideNum],
			strlen(notes[slideNum]));
}

void readNotes(char *filename)
{
	/* TODO: Spit out a warning when there are more notes than slides */

	GtkWidget *dialog = NULL;
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;
	xmlChar *tmp = NULL;
	int slideNum = 0;

	/* Init notes with empty strings. */
	initNotes();

	/* Try to read the file. */
	doc = xmlReadFile(filename, NULL, 0);
	if (doc == NULL)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not read file.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlCleanupParser();
		return;
	}

	/* Get the root element. */
	root_element = xmlDocGetRootElement(doc);
	if (root_element == NULL)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not get root element.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return;
	}

	/* Traverse slide-elements. */
	for (cur_node = root_element->children; cur_node;
			cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "slide"))
		{
			/* Get slide number and content. */
			tmp = xmlGetProp(cur_node, BAD_CAST "number");
			if (tmp == NULL)
				continue;

			slideNum = atoi((char *)tmp);
			slideNum--;
			xmlFree(tmp);

			if (slideNum < 0)
				continue;

			tmp = xmlNodeGetContent(cur_node);
			if (tmp == NULL)
				continue;

			/* Replace note text. */
			if (notes[slideNum] != NULL)
				g_free(notes[slideNum]);
			notes[slideNum] = g_strdup((char *)tmp);

			xmlFree(tmp);
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
}

void saveNotes(char *uri)
{
	int i, rc;
	GtkWidget *dialog = NULL;
	xmlTextWriterPtr writer;

	/* Create a new XmlWriter for uri, with no compression. */
	writer = xmlNewTextWriterFilename(uri, 0);
	if (writer == NULL)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Error creating the xml writer.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlCleanupParser();
		return;
	}

	/* Start the document with the xml default for the version, encoding
	 * UTF-8 and the default for the standalone declaration. */
	rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
	if (rc < 0)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not start document. %d.", rc);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	/* Start root element. It's okay to use "BAD_CAST" since there
	 * are no non-ascii letters. */
	rc = xmlTextWriterStartElement(writer, BAD_CAST "notes");
	if (rc < 0)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not start element \"notes\". %d.", rc);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	/* Save all notes which do exist. Leave out empty slides. */
	for (i = 0; i < doc_n_pages; i++)
	{
		if (notes[i] != NULL && g_strcmp0("", notes[i]) != 0)
		{
			/* Start of "slide" element. */
			rc = xmlTextWriterStartElement(writer, BAD_CAST "slide");
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not start element \"slide\". %d.",
						rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* Write page number as attribute. */
			rc = xmlTextWriterWriteFormatAttribute(writer,
					BAD_CAST "number", "%d", (i + 1));
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not write attribute \"number\""
						" for slide %d. %d.", (i + 1), rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* Write note as element content. */
			rc = xmlTextWriterWriteFormatString(writer,
					"%s", notes[i]);
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not write string"
						" for slide %d. %d.", (i + 1), rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* End of "slide" element. */
			rc = xmlTextWriterEndElement(writer);
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not end element \"slide\". %d.",
						rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}
		}
	}

	/* Here we could close open elements using the function
	 * xmlTextWriterEndElement, but since we do not want to write any
	 * other elements, we simply call xmlTextWriterEndDocument, which
	 * will do all the work. */
	rc = xmlTextWriterEndDocument(writer);
	if (rc < 0)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"xml: Could not end document. %d.", rc);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	xmlFreeTextWriter(writer);
	xmlCleanupParser();

	/* Report success. */
	dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"Notes saved.");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void saveCurrentNote(void)
{
	GtkTextIter start, end;
	gchar *content = NULL;

	/* get text from the buffer */
	gtk_text_buffer_get_bounds(noteBuffer, &start, &end);
	content = gtk_text_buffer_get_text(noteBuffer, &start, &end, FALSE);

	/* replace previous content */
	if (notes[doc_page] != NULL)
		g_free(notes[doc_page]);

	notes[doc_page] = content;
}
