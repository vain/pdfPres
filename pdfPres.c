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
	int width;
	int height;

	GtkWidget *image;
};

static GList *ports = NULL;

static PopplerDocument *doc;

static int doc_n_pages;
static int doc_page = 0;

#define FIT_WIDTH 0
#define FIT_HEIGHT 1
static int fitmode = FIT_WIDTH;

#define NUM_FRAMES 5


static void dieOnNull(void *ptr, int line)
{
	if (ptr == NULL)
	{
		fprintf(stderr, "Out of memory in line %d.\n", line);
		exit(EXIT_FAILURE);
	}
}

static void renderToPixbuf(struct viewport *pp)
{
	int mypage_i;
	double pw = 0, ph = 0;
	double w = 0, h = 0;
	double page_ratio = 1, scale = 1;
	GdkPixbuf *targetBuf = NULL;

	/* no valid target size? */
	if (pp->width <= 0 || pp->height <= 0)
		return;

	/* decide which page to render - if any */
	mypage_i = doc_page + pp->offset;
	if (mypage_i < 0 || mypage_i >= doc_n_pages)
	{
		/* TODO: Render empty page */
		return;
	}

	/* TODO: Render black background */

	/* get this page and its ratio */
	PopplerPage *page = poppler_document_get_page(doc, mypage_i);
	poppler_page_get_size(page, &pw, &ph);
	page_ratio = pw / ph;

	switch (fitmode)
	{
		case FIT_HEIGHT:
			/* fit height */
			h = pp->height;
			w = h * page_ratio;
			scale = h / ph;
			break;

		case FIT_WIDTH:
			/* fit width */
			w = pp->width;
			h = w / page_ratio;
			scale = w / pw;
			break;
	}

	printf("w = %lf, h = %lf, pw = %lf, ph = %lf, scale = %lf\n", w, h, pw, ph, scale);

	/* render to a pixbuf */
	targetBuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
	dieOnNull(targetBuf, __LINE__);

	poppler_page_render_to_pixbuf(page, 0, 0, pw, ph, scale, 0, targetBuf);
	gtk_image_set_from_pixbuf(GTK_IMAGE(pp->image), targetBuf);

	gdk_pixbuf_unref(targetBuf);
	g_object_unref(G_OBJECT(page));
}

static void refreshPorts(void)
{
	struct viewport *pp = NULL;
	GList *it = ports;

	printf("Refreshing...\n");
	while (it)
	{
		pp = (struct viewport *)(it->data);
		renderToPixbuf(pp);
		it = g_list_next(it);
	}
}

static gpointer onKeyPressed(GtkWidget *widg, gpointer user_data)
{
	GdkEventKey *ev = user_data;
	gboolean changed = TRUE;

	printf("Key pressed.\n");

	switch (ev->keyval)
	{
		case GDK_Right:
		case GDK_Return:
		case GDK_space:
			doc_page++;
			doc_page %= doc_n_pages;
			break;

		case GDK_Left:
			doc_page--;
			doc_page = (doc_page < 0 ? doc_n_pages - 1 : doc_page);
			break;

		case GDK_F5:
			/* do nothing -- especially do not clear the flag
			 * --> redraw */
			break;

		case GDK_w:
			fitmode = FIT_WIDTH;
			break;

		case GDK_h:
			fitmode = FIT_HEIGHT;
			break;

		case GDK_Escape:
		case GDK_q:
			gtk_main_quit();
			break;

		default:
			changed = FALSE;
	}

	if (changed == TRUE)
	{
		refreshPorts();
	}

	return NULL;
}

void af_resize_cb(GtkWidget *widg, GtkAllocation *al, struct viewport *port)
{
	int w, h;

	w = al->width;
	h = al->height;
	printf("NEW w = %d, h = %d\n", w, h);

	port->width = al->width;
	port->height = al->height;

	/* TODO: Find a way to auto-refresh on resize-events. This will
	 * include the very first resize-event --> initial rendering
	 * on startup*/
}

int main(int argc, char **argv)
{
	int i = 0, transIndex = 0;
	GtkWidget *hbox;
	GError* err = NULL;
	GtkWidget *image, *frame, *dummy;
	GtkWidget *win_preview, *win_beamer;
	GdkColor black, highlight;
	struct viewport *thisport;
	gchar *title;

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


	/* init colors */
	if (gdk_color_parse("#000000", &black) != TRUE)
		fprintf(stderr, "Could not resolve color \"black\".\n");
	if (gdk_color_parse("#FFFFFF", &highlight) != TRUE)
		fprintf(stderr, "Could not resolve color \"highlight\".\n");


	/* init our two windows */
	win_preview = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	win_beamer  = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title(GTK_WINDOW(win_preview), "pdfPres - Preview");
	gtk_window_set_title(GTK_WINDOW(win_beamer),  "pdfPres - Beamer");

	g_signal_connect(G_OBJECT(win_preview), "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_preview), "destroy",      G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_beamer),  "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_beamer),  "destroy",      G_CALLBACK(gtk_main_quit), NULL);

	g_signal_connect(G_OBJECT(win_preview), "key_press_event", G_CALLBACK(onKeyPressed), NULL);
	g_signal_connect(G_OBJECT(win_beamer),  "key_press_event", G_CALLBACK(onKeyPressed), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(win_preview), 10);
	gtk_container_set_border_width(GTK_CONTAINER(win_beamer),  0);

	gtk_widget_modify_bg(win_beamer, GTK_STATE_NORMAL, &black);


	/* init containers for "preview" */
	hbox = gtk_hbox_new(TRUE, 0);

	/* dynamically create all the frames */
	for (i = 0; i < NUM_FRAMES; i++)
	{
		/* calc the offset for this frame */
		transIndex = i - (int)((double)NUM_FRAMES / 2.0);

		/* create the widget */
		title = g_strdup_printf("Slide %d", transIndex);
		frame = gtk_frame_new(title);
		free(title);

		/* create a new drawing area - the pdf will be rendered in there */
		image = gtk_image_new();
		gtk_widget_set_size_request(image, 100, 100);

		/* add widgets to their parents */
		gtk_container_add(GTK_CONTAINER(frame), image);
		if (transIndex == 0)
		{
			/* the "current" frame will be place in an eventbox
			 * so we can set a background color */
			dummy = gtk_event_box_new();
			gtk_container_add(GTK_CONTAINER(dummy), frame);
			gtk_box_pack_start(GTK_BOX(hbox), dummy, TRUE, TRUE, 5);
			gtk_widget_show(dummy);

			gtk_widget_modify_bg(dummy, GTK_STATE_NORMAL, &highlight);
		}
		else
		{
			gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
		}

		gtk_widget_show(image);
		gtk_widget_show(frame);

		/* save info of this rendering port */
		thisport = (struct viewport *)malloc(sizeof(struct viewport));
		dieOnNull(thisport, __LINE__);
		thisport->offset = transIndex;
		thisport->image = image;
		ports = g_list_append(ports, thisport);

		/* resize callback */
		g_signal_connect(G_OBJECT(frame), "size_allocate", G_CALLBACK(af_resize_cb), thisport);
	}

	gtk_container_add(GTK_CONTAINER(win_preview), hbox);
	gtk_widget_show(hbox);


	/* add a rendering area in a frame to the beamer window */
	image = gtk_image_new();
	gtk_widget_set_size_request(image, 100, 100);

	gtk_container_add(GTK_CONTAINER(win_beamer), image);
	gtk_widget_show(image);

	/* save info of this rendering port */
	thisport = (struct viewport *)malloc(sizeof(struct viewport));
	dieOnNull(thisport, __LINE__);
	thisport->offset = 0;
	thisport->image = image;
	ports = g_list_append(ports, thisport);

	/* connect the on-resize-callback directly to the window */
	g_signal_connect(G_OBJECT(win_beamer), "size_allocate", G_CALLBACK(af_resize_cb), thisport);


	/* show the windows */
	gtk_widget_show(win_preview);
	gtk_widget_show(win_beamer);

	gtk_main();
	return 0;
}
