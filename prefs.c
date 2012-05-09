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


#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>

#include "prefs.h"


struct _prefs prefs;

void loadPreferences(void)
{
	char *prefspath = NULL;
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;
	xmlChar *tmp = NULL;
	int tmp_i = 0;

	/* Init default settings. */
	prefs.initial_fit_mode = FIT_PAGE;
	prefs.slide_context = 1;
	prefs.do_wrapping = FALSE;
	prefs.do_notectrl = FALSE;
	prefs.cache_max = 32;
	prefs.font_notes = g_strdup("Sans 12");
	prefs.font_timer = g_strdup("Sans 35");
	prefs.q_exits_fullscreen = FALSE;
	prefs.timer_is_clock = FALSE;
	prefs.stop_timer_on_fs = FALSE;
	/* We're using g_strdup() here so we can use g_free() all the time.
	 */

	/* NOTE / FIXME: This is specific to unix. */
	prefspath = g_strdup_printf("%s/.config/pdfpres/config.xml",
			getenv("HOME"));

	/* Try to read the file. */
	doc = xmlReadFile(prefspath, NULL, 0);
	g_free(prefspath);
	if (doc == NULL)
	{
		fprintf(stderr, "[prefs] Config could not be read.\n");
		xmlCleanupParser();
		return;
	}

	/* Get the root element. */
	root_element = xmlDocGetRootElement(doc);
	if (root_element == NULL)
	{
		fprintf(stderr, "[prefs] Config has no root element.\n");
		xmlFreeDoc(doc);
		xmlCleanupParser();
		return;
	}

	/* Traverse elements. */
	/* TODO: Isn't there something like "get child with name XYZ"? */
	for (cur_node = root_element->children; cur_node;
			cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "initial_fit_mode")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Fit mode? Restrict to valid values. */
			tmp_i = atoi((char *)tmp);
			if (tmp_i == FIT_WIDTH
					|| tmp_i == FIT_HEIGHT
					|| tmp_i == FIT_PAGE)
			{
				prefs.initial_fit_mode = tmp_i;
			}
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "slide_context")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Slide context? Restrict to sane values. */
			tmp_i = atoi((char *)tmp);
			if (tmp_i > 0 && tmp_i < 30)
			{
				prefs.slide_context = tmp_i;
			}
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "do_wrapping")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Do wrapping? */
			tmp_i = atoi((char *)tmp);
			prefs.do_wrapping = (tmp_i == 1 ? TRUE : FALSE);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "do_notectrl")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Do note control? */
			tmp_i = atoi((char *)tmp);
			prefs.do_notectrl = (tmp_i == 1 ? TRUE : FALSE);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "cache_max")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Cache maximum? */
			tmp_i = atoi((char *)tmp);
			if (tmp_i > 0)
			{
				prefs.cache_max = tmp_i;
			}
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "font_notes")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Font for notes? */
			if (prefs.font_notes != NULL)
				g_free(prefs.font_notes);

			prefs.font_notes = g_strdup((char *)tmp);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "font_timer")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Font for timer? */
			if (prefs.font_timer != NULL)
				g_free(prefs.font_timer);

			prefs.font_timer = g_strdup((char *)tmp);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "q_exits_fullscreen")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* When in fullscreen, does Q/Esc quit the program (0) or
			 * does it exit fullscreen mode (1)? */
			tmp_i = atoi((char *)tmp);
			prefs.q_exits_fullscreen = (tmp_i == 1 ? TRUE : FALSE);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "timer_is_clock")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* If set to 1, show the current time instead of a timer.
			 * Furthermore, no buttons to control the timer shall be
			 * visible. */
			tmp_i = atoi((char *)tmp);
			prefs.timer_is_clock = (tmp_i == 1 ? TRUE : FALSE);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "stop_timer_on_fs")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* When in fullscreen, does Q/Esc quit the program (0) or
			 * does it exit fullscreen mode (1)? */
			tmp_i = atoi((char *)tmp);
			prefs.stop_timer_on_fs = (tmp_i == 1 ? TRUE : FALSE);
			xmlFree(tmp);
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return;
}

static gboolean checkdir(char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) == -1)
	{
		if (mkdir(path, S_IRWXU) == -1)
		{
			fprintf(stderr, "[prefs] Could not create directory:"
					"\n\t`%s'\n", path);
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean writeRootElement(xmlTextWriterPtr writer)
{
	int rc = 0;

	/* Start of element. */
	rc = xmlTextWriterStartElement(writer, BAD_CAST "config");
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not start element `config'.\n");
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return FALSE;
	}

	return TRUE;
}

static gboolean writeElement(xmlTextWriterPtr writer, char *ele, char *v)
{
	int rc = 0;

	/* Start of element. */
	rc = xmlTextWriterStartElement(writer, BAD_CAST ele);
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not start element `%s'.\n", ele);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return FALSE;
	}

	/* Write page number as attribute. */
	rc = xmlTextWriterWriteFormatAttribute(writer,
			BAD_CAST "v", "%s", v);
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not write value `%s' "
				"for element `%s'.\n", v, ele);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return FALSE;
	}

	/* End of "slide" element. */
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not end element `%s'.\n", ele);
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return FALSE;
	}

	return TRUE;
}

static gboolean writeElementInt(xmlTextWriterPtr writer, char *ele, int v)
{
	char *v_str = NULL;
	gboolean result = FALSE;

	v_str = g_strdup_printf("%d", v);
	result = writeElement(writer, ele, v_str);
	g_free(v_str);

	return result;
}

static gboolean writeElementBoolean(xmlTextWriterPtr writer, char *ele,
		gboolean v)
{
	return writeElement(writer, ele, (v ? "1" : "0"));
}

void savePreferences(void)
{
	char *prefspath = NULL;
	xmlTextWriterPtr writer;
	gboolean result = FALSE;
	int rc = 0;

	/* Ready up dirs. */
	/* NOTE / FIXME: This is specific to unix. */
	prefspath = g_strdup_printf("%s/.config", getenv("HOME"));
	result = checkdir(prefspath);
	g_free(prefspath);

	if (!result)
		return;

	prefspath = g_strdup_printf("%s/.config/pdfpres", getenv("HOME"));
	result = checkdir(prefspath);
	g_free(prefspath);

	if (!result)
		return;

	/* Directories ready. */
	prefspath = g_strdup_printf("%s/.config/pdfpres/config.xml",
			getenv("HOME"));

	/* Create a new XmlWriter for uri, with no compression. */
	writer = xmlNewTextWriterFilename(prefspath, 0);
	g_free(prefspath);
	if (writer == NULL)
	{
		fprintf(stderr, "[prefs] Can't write to config.xml.\n");
		xmlCleanupParser();
		return;
	}

	/* Activate indentation. An error is not fatal at this point. */
	rc = xmlTextWriterSetIndent(writer, 1);
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not activate xml indentation.\n");
	}

	/* Start the document with the xml default for the version, encoding
	 * UTF-8 and the default for the standalone declaration. */
	rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not start document.\n");
		xmlFreeTextWriter(writer);
		xmlCleanupParser();
		return;
	}

	/* Write all preferences. Note: On failure, these functions clean up
	 * the xml stuff by themselves. */
	if (!writeRootElement(writer))
		return;

	if (!writeElementInt(writer, "initial_fit_mode",
				prefs.initial_fit_mode))
		return;

	if (!writeElementInt(writer, "slide_context", prefs.slide_context))
		return;

	if (!writeElementBoolean(writer, "do_wrapping", prefs.do_wrapping))
		return;

	if (!writeElementBoolean(writer, "do_notectrl", prefs.do_notectrl))
		return;

	if (!writeElementInt(writer, "cache_max", prefs.cache_max))
		return;

	if (!writeElement(writer, "font_notes", prefs.font_notes))
		return;

	if (!writeElement(writer, "font_timer", prefs.font_timer))
		return;

	if (!writeElementBoolean(writer, "q_exits_fullscreen",
				prefs.q_exits_fullscreen))
		return;

	if (!writeElementBoolean(writer, "stop_timer_on_fs",
				prefs.stop_timer_on_fs))
		return;

	if (!writeElementBoolean(writer, "timer_is_clock", prefs.timer_is_clock))
		return;

	/* Finish. */
	rc = xmlTextWriterEndDocument(writer);
	if (rc < 0)
	{
		fprintf(stderr, "[prefs] Could not end document.\n");
	}

	xmlFreeTextWriter(writer);
	xmlCleanupParser();
}
