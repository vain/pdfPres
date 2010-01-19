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
#include <unistd.h>
#include <sys/stat.h>

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
	GtkWidget *frame;

	GdkPixbuf *cache;

	gboolean isBeamer;
};

static GList *ports = NULL;

static PopplerDocument *doc;

static int doc_n_pages;
static int doc_page = 0;
static int doc_page_mark = 0;
static int doc_page_beamer = 0;

static gboolean beamer_active = TRUE;
static gboolean do_wrapping = FALSE;

static GdkColor col_current, col_marked, col_dim;

#define FIT_WIDTH 0
#define FIT_HEIGHT 1
#define FIT_PAGE 2
static int fitmode = FIT_PAGE;


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
	int mypage_i, myfitmode;
	double pw = 0, ph = 0;
	double w = 0, h = 0;
	double page_ratio = 1, screen_ratio = 1, scale = 1;
	GdkPixbuf *targetBuf = NULL;
	PopplerPage *page = NULL;
	gchar *title = NULL;

	/* no valid target size? */
	if (pp->width <= 0 || pp->height <= 0)
		return;

	/* decide which page to render - if any */
	if (pp->isBeamer == FALSE)
		mypage_i = doc_page + pp->offset;
	else
		mypage_i = doc_page_beamer + pp->offset;

	if (mypage_i < 0 || mypage_i >= doc_n_pages)
	{
		/* clear image and reset frame title */
		gtk_image_clear(GTK_IMAGE(pp->image));

		if (pp->frame != NULL)
			gtk_frame_set_label(GTK_FRAME(pp->frame), "X");

		return;
	}
	else
	{
		/* update frame title */
		if (pp->frame != NULL)
		{
			title = g_strdup_printf("Slide %d / %d", mypage_i + 1,
					doc_n_pages);
			gtk_frame_set_label(GTK_FRAME(pp->frame), title);
			g_free(title);
		}
	}

	/* pixbuf still cached? */
	if (pp->cache != NULL)
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(pp->image), pp->cache);
		return;
	}

	/* cache is empty. we have to render from scratch. */

	/* get this page and its ratio */
	page = poppler_document_get_page(doc, mypage_i);
	poppler_page_get_size(page, &pw, &ph);
	page_ratio = pw / ph;
	screen_ratio = (double)pp->width / (double)pp->height;

	/* select fit mode */
	if (fitmode == FIT_PAGE)
	{
		/* that's it: compare screen and page ratio. this
		 * will cover all 4 cases that could happen. */
		if (screen_ratio > page_ratio)
			myfitmode = FIT_HEIGHT;
		else
			myfitmode = FIT_WIDTH;
	}
	else
		myfitmode = fitmode;

	switch (myfitmode)
	{
		case FIT_HEIGHT:
			h = pp->height;
			w = h * page_ratio;
			scale = h / ph;
			break;

		case FIT_WIDTH:
			w = pp->width;
			h = w / page_ratio;
			scale = w / pw;
			break;
	}

	/* render to a pixbuf */
	targetBuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
	dieOnNull(targetBuf, __LINE__);

	poppler_page_render_to_pixbuf(page, 0, 0, w, h, scale, 0, targetBuf);
	gtk_image_set_from_pixbuf(GTK_IMAGE(pp->image), targetBuf);

	g_object_unref(G_OBJECT(page));

	/* cache the rendered pixbuf */
	pp->cache = targetBuf;
}

static void refreshFrames(void)
{
	struct viewport *pp = NULL;
	GList *it = ports;

	while (it)
	{
		pp = (struct viewport *)(it->data);

		/* the beamer has no frame */
		if (pp->isBeamer == FALSE)
		{
			/* reset background color */
			gtk_widget_modify_bg(pp->frame->parent, GTK_STATE_NORMAL, NULL);

			/* lock mode: highlight the saved/current page */
			if (beamer_active == FALSE)
			{
				if (doc_page + pp->offset == doc_page_mark)
				{
					gtk_widget_modify_bg(pp->frame->parent, GTK_STATE_NORMAL, &col_marked);
				}
				else if (pp->offset == 0)
				{
					gtk_widget_modify_bg(pp->frame->parent, GTK_STATE_NORMAL, &col_dim);
				}
			}
			/* normal mode: highlight the "current" frame */
			else
			{
				if (pp->offset == 0)
				{
					gtk_widget_modify_bg(pp->frame->parent, GTK_STATE_NORMAL, &col_current);
				}
			}
		}

		it = g_list_next(it);
	}
}

static void refreshPorts(void)
{
	struct viewport *pp = NULL;
	GList *it = ports;

	while (it)
	{
		pp = (struct viewport *)(it->data);
		renderToPixbuf(pp);
		it = g_list_next(it);
	}

	refreshFrames();
}

static void clearAllCaches(void)
{
	struct viewport *pp = NULL;
	GList *it = ports;

	while (it)
	{
		pp = (struct viewport *)(it->data);

		if (pp->cache != NULL)
			gdk_pixbuf_unref(pp->cache);

		pp->cache = NULL;

		it = g_list_next(it);
	}
}

static void current_fixate(void)
{
	/* skip if already fixated */
	if (beamer_active == FALSE)
		return;

	/* save current page */
	doc_page_mark = doc_page;

	/* deactivate refresh on beamer */
	beamer_active = FALSE;
}

static void current_release(gboolean jump)
{
	/* skip if not fixated */
	if (beamer_active == TRUE)
		return;

	/* reload saved page if we are not about to jump.
	 * otherwise, the currently selected slide will
	 * become active. */
	if (jump == FALSE)
	{
		doc_page = doc_page_mark;
	}

	/* re-activate beamer */
	beamer_active = TRUE;
	doc_page_beamer = doc_page;

	/* caches will most probably end up inconsistent */
	clearAllCaches();
}

static void nextSlide(void)
{
	/* a note on caching:
	 *
	 * every viewport holds a pointer to a pixbuf. if we
	 * change to another slide, those pointers will be
	 * exchanged so that already existing images won't
	 * get rendered again.
	 *
	 * we do NOT have to care about over- or underruns.
	 * every viewport will see for itself if it's about
	 * to render a valid page or not.
	 *
	 * so all we have to do is exchange the pointers and
	 * clear those which *must* be rendered.
	 */

	int i;
	GList *a = NULL, *b = NULL;
	struct viewport *aPort = NULL, *bPort = NULL;

	if (!do_wrapping)
	{
		if (doc_page == doc_n_pages - 1)
		{
			return;
		}
	}

	/* update global counter */
	doc_page++;
	doc_page %= doc_n_pages;

	/* update beamer counter if it's active */
	if (beamer_active == TRUE)
		doc_page_beamer = doc_page;

	/* important! unref unused pixbufs:
	 * - the very first frame to the left
	 * - the cache of the beamer port (only if active)
	 */
	a = g_list_first(ports);
	aPort = (struct viewport *)(a->data);
	if (aPort->cache != NULL)
		gdk_pixbuf_unref(aPort->cache);

	a = g_list_last(ports);
	aPort = (struct viewport *)(a->data);
	if (aPort->cache != NULL && beamer_active == TRUE)
		gdk_pixbuf_unref(aPort->cache);

	/* update cache - we expect the g_list to be ordered.
	 * hence, we can omit the very last item which is
	 * the beamer port. */
	for (i = 0; i < g_list_length(ports) - 2; i++)
	{
		a = g_list_nth(ports, i);
		b = g_list_nth(ports, i + 1);

		aPort = (struct viewport *)(a->data);
		bPort = (struct viewport *)(b->data);

		aPort->cache = bPort->cache;
	}

	/* clear the last two caches: the beamer port and
	 * the preview frame to the very right. */
	a = g_list_last(ports);
	aPort = (struct viewport *)(a->data);
	if (beamer_active == TRUE)
		aPort->cache = NULL;

	a = g_list_previous(a);
	aPort = (struct viewport *)(a->data);
	aPort->cache = NULL;
}

static void prevSlide(void)
{
	int i;
	GList *a = NULL, *b = NULL;
	struct viewport *aPort = NULL, *bPort = NULL;

	if (!do_wrapping)
	{
		if (doc_page == 0)
		{
			return;
		}
	}

	/* update global counter */
	doc_page--;
	doc_page = (doc_page < 0 ? doc_n_pages - 1 : doc_page);

	/* update beamer counter if it's active */
	if (beamer_active == TRUE)
		doc_page_beamer = doc_page;

	/* important! unref unused pixbufs:
	 * - the cache of the beamer port (only if active)
	 * - the very last frame to the right
	 */
	a = g_list_last(ports);
	aPort = (struct viewport *)(a->data);
	if (aPort->cache != NULL && beamer_active == TRUE)
		gdk_pixbuf_unref(aPort->cache);

	a = g_list_previous(a);
	aPort = (struct viewport *)(a->data);
	if (aPort->cache != NULL)
		gdk_pixbuf_unref(aPort->cache);

	/* update cache - we expect the g_list to be ordered.
	 * hence, we can omit the very last item which is
	 * the beamer port. */
	for (i = g_list_length(ports) - 3; i >= 0; i--)
	{
		a = g_list_nth(ports, i);
		b = g_list_nth(ports, i + 1);

		aPort = (struct viewport *)(a->data);
		bPort = (struct viewport *)(b->data);

		bPort->cache = aPort->cache;
	}

	/* clear cache of the first item */
	a = g_list_first(ports);
	aPort = (struct viewport *)(a->data);
	aPort->cache = NULL;

	/* clear the beamer cache if active */
	if (beamer_active == TRUE)
	{
		a = g_list_last(ports);
		aPort = (struct viewport *)(a->data);
		aPort->cache = NULL;
	}
}

static gboolean onKeyPressed(GtkWidget *widg, GdkEventKey *ev, gpointer user_data)
{
	gboolean changed = TRUE;

	switch (ev->keyval)
	{
		case GDK_Right:
		case GDK_Return:
		case GDK_space:
			nextSlide();
			break;

		case GDK_Left:
			prevSlide();
			break;

		case GDK_F5:
			clearAllCaches();
			break;

		case GDK_w:
			fitmode = FIT_WIDTH;
			clearAllCaches();
			break;

		case GDK_h:
			fitmode = FIT_HEIGHT;
			clearAllCaches();
			break;

		case GDK_p:
			fitmode = FIT_PAGE;
			clearAllCaches();
			break;

		case GDK_l:
			current_fixate();
			break;

		case GDK_L:
			current_release(FALSE);
			break;

		case GDK_J:
			current_release(TRUE);
			break;

		case GDK_Escape:
		case GDK_q:
			changed = FALSE;
			gtk_main_quit();
			break;

		default:
			changed = FALSE;
	}

	if (changed == TRUE)
	{
		refreshPorts();
	}

	return TRUE;
}

static gboolean onMouseReleased(GtkWidget *widg, GdkEventButton *ev, gpointer user_data)
{
	/* forward on left click, backward on right click */

	if (ev->type == GDK_BUTTON_RELEASE)
	{
		if (ev->button == 1)
		{
			nextSlide();
			refreshPorts();
		}
		else if (ev->button == 3)
		{
			prevSlide();
			refreshPorts();
		}
	}

	return TRUE;
}

static void onResize(GtkWidget *widg, GtkAllocation *al, struct viewport *port)
{
	int wOld = port->width;
	int hOld = port->height;

	port->width = al->width;
	port->height = al->height;

	/* if the new size differs from the old size, then
	 * re-render this particular viewport. */
	if (wOld != port->width || hOld != port->height)
	{
		/* clear cache */
		if (port->cache != NULL)
			gdk_pixbuf_unref(port->cache);

		port->cache = NULL;

		renderToPixbuf(port);
	}
}

static void usage(char *exe)
{
	fprintf(stderr, "Usage: %s [-s <slides>] [-w] <file>\n", exe);
}

int main(int argc, char **argv)
{
	int i = 0, transIndex = 0, numframes;
	char *filename;
	FILE *fp;
	struct stat statbuf;
	char *databuf;
	GtkWidget *hbox;
	GError *err = NULL;
	GtkWidget *image, *frame, *evbox, *outerevbox;
	GtkWidget *win_preview, *win_beamer;
	GdkColor black;
	struct viewport *thisport;

	gtk_init(&argc, &argv);


	/* defaults */
	filename = NULL;
	numframes = 3;

	/* get options via getopt */
	while ((i = getopt(argc, argv, "s:w")) != -1)
	{
		switch (i)
		{
			case 's':
				numframes = 2 * atoi(optarg) + 1;
				if (numframes <= 1)
				{
					fprintf(stderr, "Invalid slide count specified.\n");
					usage(argv[0]);
					exit(EXIT_FAILURE);
				}
				break;

			case 'w':
				do_wrapping = TRUE;
				break;

			case '?':
				exit(EXIT_FAILURE);
				break;
		}
	}

	/* retrieve file name via first non-option argument */
	if (optind < argc)
	{
		filename = argv[optind];
	}

	if (filename == NULL)
	{
		fprintf(stderr, "Invalid file path specified.\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}


	/* try to load the file */
	if (stat(filename, &statbuf) == -1)
	{
		perror("Could not stat file");
		exit(EXIT_FAILURE);
	}

	/* note: this buffer must not be freed, it'll be used by poppler
	 * later on. */
	databuf = (char *)malloc(statbuf.st_size);
	dieOnNull(databuf, __LINE__);

	fp = fopen(filename, "rb");
	if (!fp)
	{
		perror("Could not open file");
		exit(EXIT_FAILURE);
	}

	if (fread(databuf, 1, statbuf.st_size, fp) != statbuf.st_size)
	{
		fprintf(stderr, "Unexpected end of file.\n");
		exit(EXIT_FAILURE);
	}

	fclose(fp);

	/* get document from data */
	doc = poppler_document_new_from_data(databuf, statbuf.st_size,
			NULL, &err);
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
	if (gdk_color_parse("#BBFFBB", &col_current) != TRUE)
		fprintf(stderr, "Could not resolve color \"col_current\".\n");
	if (gdk_color_parse("#FFBBBB", &col_marked) != TRUE)
		fprintf(stderr, "Could not resolve color \"col_marked\".\n");
	if (gdk_color_parse("#BBBBBB", &col_dim) != TRUE)
		fprintf(stderr, "Could not resolve color \"col_dim\".\n");


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

	gtk_widget_add_events(win_beamer, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(win_beamer, GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(G_OBJECT(win_beamer), "button_release_event", G_CALLBACK(onMouseReleased), NULL);

	gtk_widget_add_events(win_preview, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(win_preview, GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(G_OBJECT(win_preview), "button_release_event", G_CALLBACK(onMouseReleased), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(win_preview), 10);
	gtk_container_set_border_width(GTK_CONTAINER(win_beamer),  0);

	gtk_widget_modify_bg(win_beamer, GTK_STATE_NORMAL, &black);


	/* init containers for "preview" */
	hbox = gtk_hbox_new(TRUE, 0);

	/* dynamically create all the frames */
	for (i = 0; i < numframes; i++)
	{
		/* calc the offset for this frame */
		transIndex = i - (int)((double)numframes / 2.0);

		/* create the widget - note that it is important not to
		 * set the title to NULL. this would cause a lot more
		 * redraws on startup because the frame will get re-
		 * allocated when the title changes. */
		frame = gtk_frame_new("");

		/* create a new drawing area - the pdf will be rendered in there */
		image = gtk_image_new();
		gtk_widget_set_size_request(image, 100, 100);

		/* add widgets to their parents. the image is placed in an eventbox,
		 * the box's size_allocate signal will be handled. so, we know the
		 * exact width/height we can render into. (placing the image into the
		 * frame would create the need of knowing the frame's border size...)
		 */
		evbox = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(evbox), image);
		gtk_container_add(GTK_CONTAINER(frame), evbox);

		/* every frame will be placed in another eventbox so we can set a
		 * background color */
		outerevbox = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(outerevbox), frame);
		gtk_box_pack_start(GTK_BOX(hbox), outerevbox, TRUE, TRUE, 5);

		/* make the eventbox "transparent" */
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(evbox), FALSE);

		/* show 'em all */
		gtk_widget_show(image);
		gtk_widget_show(evbox);
		gtk_widget_show(frame);
		gtk_widget_show(outerevbox);

		/* save info of this rendering port */
		thisport = (struct viewport *)malloc(sizeof(struct viewport));
		dieOnNull(thisport, __LINE__);
		thisport->offset = transIndex;
		thisport->image = image;
		thisport->frame = frame;
		thisport->cache = NULL;
		thisport->width = -1;
		thisport->height = -1;
		thisport->isBeamer = FALSE;
		ports = g_list_append(ports, thisport);

		/* resize callback */
		g_signal_connect(G_OBJECT(evbox), "size_allocate", G_CALLBACK(onResize), thisport);
	}

	gtk_container_add(GTK_CONTAINER(win_preview), hbox);
	gtk_widget_show(hbox);

	/* in order to set the initially highlighted frame */
	refreshFrames();


	/* add a rendering area to the beamer window */
	image = gtk_image_new();
	gtk_widget_set_size_request(image, 320, 240);

	gtk_container_add(GTK_CONTAINER(win_beamer), image);
	gtk_widget_show(image);

	/* save info of this rendering port */
	thisport = (struct viewport *)malloc(sizeof(struct viewport));
	dieOnNull(thisport, __LINE__);
	thisport->offset = 0;
	thisport->image = image;
	thisport->frame = NULL;
	thisport->cache = NULL;
	thisport->width = -1;
	thisport->height = -1;
	thisport->isBeamer = TRUE;
	ports = g_list_append(ports, thisport);

	/* connect the on-resize-callback directly to the window */
	g_signal_connect(G_OBJECT(win_beamer), "size_allocate", G_CALLBACK(onResize), thisport);


	/* show the windows */
	gtk_widget_show(win_preview);
	gtk_widget_show(win_beamer);

	gtk_main();
	exit(EXIT_SUCCESS);
}
