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
#include <libxml/parser.h>

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
	/* We're using g_strdup() here so we can use g_free() all the time.
	 */

	/* NOTE / FIXME: This is specific to unix. */
	prefspath = g_strdup_printf("%s/.config/pdfPres/config.xml",
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
				fprintf(stderr, "[dbg] Initial fit mode: %d\n", tmp_i);
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
				fprintf(stderr, "[dbg] Slide context: %d\n", tmp_i);
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
			fprintf(stderr, "[dbg] Do wrapping: %d\n", tmp_i);
			xmlFree(tmp);
		}

		if (cur_node->type == XML_ELEMENT_NODE
				&& !xmlStrcmp(cur_node->name, BAD_CAST "do_notectrl")
				&& (tmp = xmlGetProp(cur_node, BAD_CAST "v")) != NULL)
		{
			/* Do note control? */
			tmp_i = atoi((char *)tmp);
			prefs.do_notectrl = (tmp_i == 1 ? TRUE : FALSE);
			fprintf(stderr, "[dbg] Do notectrl: %d\n", tmp_i);
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
				fprintf(stderr, "[dbg] Cache max: %d\n", tmp_i);
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
			fprintf(stderr, "[dbg] Font notes: %s\n", prefs.font_notes);
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
			fprintf(stderr, "[dbg] Font timer: %s\n", prefs.font_timer);
			xmlFree(tmp);
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return;
}

void savePreferences(void)
{
	/* TODO: Create directory structure: ~/.config/pdfPres/ */
	/* TODO: Save settings to ~/.config/pdfPres/config.xml */
}
