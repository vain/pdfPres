/*
	Copyright 2009 Peter Hofmann

	This file is part of pdfPres.

	pdfPres is free software: you can redistribute it and/or modify it under
	the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	pdfPres is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
	details.

	You should have received a copy of the GNU General Public License along
	with pdfPres. If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/poppler.h>


struct viewport
{
	int offset;
	GtkWidget *aspectFrame;
	GtkWidget *image;
};

GList *ports = NULL;
GList *frames = NULL;

static GtkWidget *win_preview;
static GtkWidget *win_beamer;

static PopplerDocument *doc;

static int doc_n_pages;
static int doc_page = 0;


void dieOnNull(void * ptr, int line)
{
	if (ptr == NULL)
	{
		fprintf(stderr, "Out of memory in line %d.\n", line);
		exit(EXIT_FAILURE);
	}
}

void dumpPorts(void)
{
	GList *it = ports;
	while (it)
	{
		printf("Offset: %d\n", ((struct viewport *)(it->data))->offset);
		printf("Aspect: %p\n", ((struct viewport *)(it->data))->aspectFrame);
		printf("Image : %p\n", ((struct viewport *)(it->data))->image);

		it = g_list_next(it);
	}
}

void renderToPixbuf(struct viewport *pp)
{
	int mypage_i;
	double w = 0, h = 0, ratio = 1;
	GdkPixbuf *targetBuf = NULL;

	printf("%d: %p, %p\n", pp->offset, pp->aspectFrame, pp->image);

	/* decide which page to render - if any */
	mypage_i = doc_page + pp->offset;
	if (mypage_i < 0 || mypage_i >= doc_n_pages)
		return;

	/* get this page and its ratio */
	PopplerPage *page = poppler_document_get_page(doc, mypage_i);
	poppler_page_get_size(page, &w, &h);
	ratio = w / h;

	/* adjust the widgets height to fit this aspect ratio */
	gtk_aspect_frame_set(GTK_ASPECT_FRAME(pp->aspectFrame), 0.0, 0.0, ratio, FALSE);

	/* allocate a pixbuf */
	w = 640;
	h = w / ratio;

	targetBuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, (int)w, (int)h);
	dieOnNull(targetBuf, __LINE__);

	/* render it */
	poppler_page_render_to_pixbuf(page, 0, 0, (int)w, (int)h, 1.0, 0, targetBuf);
	gtk_image_set_from_pixbuf(GTK_IMAGE(pp->image), targetBuf);

	/* cleanup */
	gdk_pixbuf_unref(targetBuf);
}

void refreshPorts(void)
{
	struct viewport *pp = NULL;
	GList *it = ports;
	while (it)
	{
		pp = (struct viewport *)(it->data);
		renderToPixbuf(pp);
		it = g_list_next(it);
	}
}

int main(int argc, char **argv)
{
	int i = 0;
	GtkWidget *hbox;
	GError* err = NULL;
	GtkWidget *widg, *widg2;
	GList *it;
	struct viewport *thisport;

	gtk_init(&argc, &argv);


	/* try to load the file */
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <file-uri>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	doc = poppler_document_new_from_file(argv[1], NULL, &err);
	if (!doc)
	{
		fprintf(stderr, "%s\n", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}

	doc_n_pages = poppler_document_get_n_pages(doc);
	if (doc_n_pages <= 0)
	{
		fprintf(stderr, "Huh, no pages in that document.\n");
		exit(EXIT_FAILURE);
	}


	/* init our two windows */
	win_preview = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	win_beamer  = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title(GTK_WINDOW(win_preview), "pdfPres - Preview");
	gtk_window_set_title(GTK_WINDOW(win_beamer),  "pdfPres - Beamer");

	g_signal_connect(G_OBJECT(win_preview), "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_preview), "destroy",      G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_beamer),  "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_beamer),  "destroy",      G_CALLBACK(gtk_main_quit), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(win_preview), 10);
	gtk_container_set_border_width(GTK_CONTAINER(win_beamer),  0);


	/* init containers for "preview" */
	hbox = gtk_hbox_new(TRUE, 0);

	/* xalign: 0.0, 0.5, 1.0 controls their sizes when there's
	 * not enough space available. */
	widg = gtk_aspect_frame_new("Previous", 0.0, 0.0, 1.0, FALSE);
	frames = g_list_append(frames, widg);

	widg = gtk_aspect_frame_new("Current",  0.5, 0.0, 1.0, FALSE);
	frames = g_list_append(frames, widg);

	widg = gtk_aspect_frame_new("Next",     1.0, 0.0, 1.0, FALSE);
	frames = g_list_append(frames, widg);

	i = 0;
	for (it = frames; it; it = g_list_next(it))
	{
		widg2 = (GtkWidget *)(it->data);

		/* create a new drawing area - the pdf will be rendered in there */
		widg = gtk_image_new();
		gtk_widget_set_size_request(widg, 200, 200);

		/* add widgets to their parents */
		gtk_container_add(GTK_CONTAINER(widg2), widg);
		gtk_box_pack_start(GTK_BOX(hbox), widg2, TRUE, TRUE, 5);

		gtk_widget_show(widg);
		gtk_widget_show(widg2);

		/* save info of this rendering port */
		thisport = (struct viewport *)malloc(sizeof(struct viewport));
		dieOnNull(thisport, __LINE__);
		thisport->offset = i - 1; /* TODO: allow more than 3 frames */
		thisport->aspectFrame = widg2;
		thisport->image = widg;
		ports = g_list_append(ports, thisport);

		i++;
	}

	gtk_container_add(GTK_CONTAINER(win_preview), hbox);
	gtk_widget_show(hbox);


	/* add a rendering area in an aspect frame to the beamer window */
	widg = gtk_image_new();
	gtk_widget_set_size_request(widg, 200, 200);

	widg2 = gtk_aspect_frame_new(NULL, 0.0, 0.0, 1.0, FALSE);
	gtk_container_add(GTK_CONTAINER(widg2), widg);
	gtk_container_add(GTK_CONTAINER(win_beamer), widg2);
	gtk_widget_show(widg);
	gtk_widget_show(widg2);

	/* save info of this rendering port */
	thisport = (struct viewport *)malloc(sizeof(struct viewport));
	dieOnNull(thisport, __LINE__);
	thisport->offset = 0;
	thisport->aspectFrame = widg2;
	thisport->image = widg;
	ports = g_list_append(ports, thisport);


	dumpPorts();
	refreshPorts();


	/* show the windows */
	gtk_widget_show(win_preview);
	gtk_widget_show(win_beamer);

	gtk_main();
	return 0;
}
