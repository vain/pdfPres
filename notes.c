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


struct noteItem
{
	int number;
	gchar *text;
};


static GList *notesList = NULL;


static void clearNotes(void)
{
	GList *it = notesList;
	struct noteItem *ni = NULL;

	while (it)
	{
		ni = (struct noteItem *)(it->data);

		/* Free text if any. */
		if (ni->text != NULL)
			g_free(ni->text);

		/* Free space alloc'd for the struct. */
		free(ni);

		it = g_list_next(it);
	}

	/* Free the whole list. */
	g_list_free(notesList);
	notesList = NULL;
}

static void setNote_strdup(int slideNum, char *text)
{
	GList *it = notesList;
	struct noteItem *ni = NULL;

	/* See if there's already an element for this slideNum. */
	while (it)
	{
		ni = (struct noteItem *)(it->data);

		if (ni->number == slideNum)
		{
			/* Replace note. */
			if (ni->text != NULL)
				g_free(ni->text);
			ni->text = g_strdup(text);
			return;
		}

		it = g_list_next(it);
	}

	/* slideNum not found. Create a new one. */
	ni = (struct noteItem *)malloc(sizeof(struct noteItem));
	ni->number = slideNum;
	ni->text = g_strdup(text);
	notesList = g_list_append(notesList, ni);
}

void printNote(int slideNum)
{
	GList *it = notesList;
	struct noteItem *ni = NULL;

	/* Search for this slideNum. */
	while (it)
	{
		ni = (struct noteItem *)(it->data);

		if (ni->number == slideNum && ni->text != NULL)
		{
			gtk_text_buffer_set_text(noteBuffer, ni->text,
					strlen(ni->text));
			return;
		}

		it = g_list_next(it);
	}

	/* Not found. */
	gtk_text_buffer_set_text(noteBuffer, "", 0);
}

void readNotes(char *filename)
{
	GtkWidget *dialog = NULL;
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;
	xmlChar *tmp = NULL;
	int slideNum = 0;

	/* Clear notes list. */
	clearNotes();

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
			xmlFree(tmp);

			if (slideNum <= 0)
				continue;

			tmp = xmlNodeGetContent(cur_node);
			if (tmp == NULL)
				continue;

			/* Replace note text. */
			setNote_strdup(slideNum, (char *)tmp);

			xmlFree(tmp);
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
}

void saveNotes(char *uri)
{
	GList *it = notesList;
	struct noteItem *ni = NULL;
	int rc;
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
	while (it)
	{
		ni = (struct noteItem *)(it->data);
		if (ni->text != NULL && g_strcmp0("", ni->text) != 0)
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
					BAD_CAST "number", "%d", ni->number);
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not write attribute \"number\""
						" for slide %d. %d.", ni->number, rc);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				xmlFreeTextWriter(writer);
				xmlCleanupParser();
				return;
			}

			/* Write note as element content. */
			rc = xmlTextWriterWriteFormatString(writer,
					"%s", ni->text);
			if (rc < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"xml: Could not write string"
						" for slide %d. %d.", ni->number, rc);
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

		it = g_list_next(it);
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
	setNote_strdup(doc_page + 1, content);

	g_free(content);
}
