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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/poppler.h>


struct viewport
{
	int offset;

	int width;
	int height;

	GtkWidget *image;
	GtkWidget *frame;

	GdkPixbuf *pixbuf;

	gboolean isBeamer;
};

struct cacheItem
{
	GdkPixbuf *pixbuf;
	int slidenum;
	double w;
	double h;
	double scale;
};


static GList *ports = NULL;
static GtkWidget *win_preview = NULL;
static GtkWidget *win_beamer = NULL;

static GList *cache = NULL;
/* sane number to start with. note that this value is adjusted later on
 * according to the number of visible preview ports. */
static int cache_max = 32;

static PopplerDocument *doc = NULL;

static int doc_n_pages;
static int doc_page = 0;
static int doc_page_mark = 0;
static int doc_page_beamer = 0;

static gboolean beamer_active = TRUE;
static gboolean do_wrapping = FALSE;
static gboolean do_notectrl = FALSE;

static gboolean isFullScreen = FALSE;
static gboolean isCurserVisible = FALSE;
static gboolean isInsideNotePad = FALSE;

static gboolean preQueued = FALSE;

static GTimer *timer = NULL;
static int timerMode = 0; /* 0 = stopped, 1 = running, 2 = paused */
static GtkWidget *startButton = NULL;
static GtkWidget *notePad = NULL, *notePadFrame = NULL;
static GtkTextBuffer *noteBuffer = NULL;
static gchar **notes = NULL;

static GdkColor col_current, col_marked, col_dim;

#define FIT_WIDTH 0
#define FIT_HEIGHT 1
#define FIT_PAGE 2
static int fitmode = FIT_PAGE;

#define FONT_SIZE 35


static void dieOnNull(void *ptr, int line)
{
	if (ptr == NULL)
	{
		fprintf(stderr, "Out of memory in line %d.\n", line);
		exit(EXIT_FAILURE);
	}
}

static void printNote(int slideNum)
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

static GdkPixbuf * getRenderedPixbuf(struct viewport *pp, int mypage_i)
{
	int myfitmode = -1;
	double pw = 0, ph = 0;
	double w = 0, h = 0;
	double page_ratio = 1, screen_ratio = 1, scale = 1;
	GdkPixbuf *targetBuf = NULL;
	PopplerPage *page = NULL;

	GList *it = NULL;
	struct cacheItem *ci = NULL;
	gboolean found = FALSE;

	/* limit boundaries of mypage_i -- just to be sure. */
	if (mypage_i < 0)
		mypage_i += doc_n_pages;
	else
		mypage_i %= doc_n_pages;

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

	/* check if already in cache. */
	it = cache;
	found = FALSE;
	while (it)
	{
		ci = (struct cacheItem *)(it->data);

		if (ci->slidenum == mypage_i && ci->w == w && ci->h == h
				&& ci->scale == scale)
		{
			/* cache hit. */
			found = TRUE;
			targetBuf = ci->pixbuf;

			/* we need to increase this item's "score", that is marking
			 * it as a "recent" item. we do so by placing it at the end
			 * of the list. */
			cache = g_list_remove(cache, ci);
			cache = g_list_append(cache, ci);

			/* now quit the loop. */
			break;
		}

		it = g_list_next(it);
	}

	if (!found)
	{
		/* cache miss, render to a pixbuf. */
		targetBuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
		dieOnNull(targetBuf, __LINE__);
		poppler_page_render_to_pixbuf(page, 0, 0, w, h, scale, 0,
				targetBuf);

		/* check if cache full. if so, kill the oldest item. */
		if (g_list_length(cache) + 1 > cache_max)
		{
			it = g_list_first(cache);
			if (it == NULL)
			{
				fprintf(stderr, "[Cache] No first item in list."
						" cache_max too small?\n");
			}
			else
			{
				/* unref pixbuf. */
				ci = (struct cacheItem *)(it->data);
				if (ci->pixbuf != NULL)
					gdk_pixbuf_unref(ci->pixbuf);

				/* free memory alloc'd for the struct. */
				free(ci);

				/* remove the pointer which is now invalid from the
				 * list.
				 */
				cache = g_list_remove(cache, ci);
			}
		}

		/* add new item to cache. */
		ci = (struct cacheItem *)malloc(sizeof(struct cacheItem));
		ci->slidenum = mypage_i;
		ci->w = w;
		ci->h = h;
		ci->scale = scale;
		ci->pixbuf = targetBuf;
		cache = g_list_append(cache, ci);
	}

	/* cleanup */
	g_object_unref(G_OBJECT(page));

	return targetBuf;
}

static int pagenumForPort(struct viewport *pp)
{
	if (pp->isBeamer == FALSE)
		return doc_page + pp->offset;
	else
		return doc_page_beamer + pp->offset;
}

static void updatePortPixbuf(struct viewport *pp)
{
	int mypage_i;
	gchar *title = NULL;

	/* no valid target size? */
	if (pp->width <= 0 || pp->height <= 0)
		return;

	/* decide which page to render - if any */
	mypage_i = pagenumForPort(pp);

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

	/* if note-control is active, print current page number if on
	 * "main" frame. (don't do this on the beamer because it could be
	 * locked.)
	 * this allows you to attach any kind of other program or script
	 * which can show notes for a specific slide. simply pipe the
	 * output of pdfPres to your other tool.
	 */
	if (pp->offset == 0 && !pp->isBeamer)
	{
		printNote(doc_page + 1);
		if (do_notectrl)
		{
			printf("%d\n", doc_page + 1);
			fflush(stdout);
		}
	}

	/* get a pixbuf for this viewport. caching is behind
	 * getRenderedPixbuf(). */
	pp->pixbuf = getRenderedPixbuf(pp, mypage_i);

	/* display the current page. */
	if (pp->pixbuf != NULL)
		gtk_image_set_from_pixbuf(GTK_IMAGE(pp->image), pp->pixbuf);
	else
		fprintf(stderr, "[Cache] Returned empty pixbuf."
				" You're doing something wrong.\n");
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
			gtk_widget_modify_bg(pp->frame->parent, GTK_STATE_NORMAL,
					NULL);

			/* lock mode: highlight the saved/current page */
			if (beamer_active == FALSE)
			{
				if (doc_page + pp->offset == doc_page_mark)
				{
					gtk_widget_modify_bg(pp->frame->parent,
							GTK_STATE_NORMAL, &col_marked);
				}
				else if (pp->offset == 0)
				{
					gtk_widget_modify_bg(pp->frame->parent,
							GTK_STATE_NORMAL, &col_dim);
				}
			}
			/* normal mode: highlight the "current" frame */
			else
			{
				if (pp->offset == 0)
				{
					gtk_widget_modify_bg(pp->frame->parent,
							GTK_STATE_NORMAL, &col_current);
				}
			}
		}

		it = g_list_next(it);
	}
}

static gboolean idleFillCaches(gpointer dummy)
{
	/* do prerendering of next slides. this will only happen when
	 * there's nothing else to do. */
	struct viewport *pp = NULL;
	GList *it = ports;
	int mypage_i = -1;

	while (it)
	{
		pp = (struct viewport *)(it->data);
		mypage_i = pagenumForPort(pp);

		/* trigger some prerendering. the pointers are irrelevant for
		 * now -- we just want the cache to be filled with all
		 * previous and next slides. */
		getRenderedPixbuf(pp, mypage_i + 1);
		getRenderedPixbuf(pp, mypage_i - 1);

		it = g_list_next(it);
	}

	/* save state. */
	preQueued = FALSE;

	/* do not call me again. */
	return FALSE;
}

static void refreshPorts(void)
{
	struct viewport *pp = NULL;
	GList *it = ports;

	/* display. */
	while (it)
	{
		pp = (struct viewport *)(it->data);
		updatePortPixbuf(pp);
		it = g_list_next(it);
	}

	refreshFrames();

	/* queue prerendering of next slides unless this has already been
	 * done.
	 *
	 * note: usually, it's not safe to use booleans for purposes like
	 * this. however, we're in a singlethreaded program, so no other
	 * thread can do concurrent modifications to this variable. hence
	 * it's okay. */
	if (!preQueued)
	{
		preQueued = TRUE;
		g_idle_add(idleFillCaches, NULL);
	}
}

static void clearCache(void)
{
	struct cacheItem *ci = NULL;
	GList *it = cache;

	while (it)
	{
		/* unref pixbuf. */
		ci = (struct cacheItem *)(it->data);
		if (ci->pixbuf != NULL)
			gdk_pixbuf_unref(ci->pixbuf);

		/* free memory alloc'd for the struct. */
		free(ci);

		it = g_list_next(it);
	}

	/* clear the list. */
	g_list_free(cache);
	cache = NULL;
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
}

static void saveCurrentNote(void)
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

static void nextSlide(void)
{
	/* stop if we're at the end and wrapping is disabled. */
	if (!do_wrapping)
	{
		if (doc_page == doc_n_pages - 1)
		{
			return;
		}
	}

	saveCurrentNote();

	/* update global counter */
	doc_page++;
	doc_page %= doc_n_pages;

	/* update beamer counter if it's active */
	if (beamer_active == TRUE)
		doc_page_beamer = doc_page;
}

static void prevSlide(void)
{
	/* stop if we're at the beginning and wrapping is disabled. */
	if (!do_wrapping)
	{
		if (doc_page == 0)
		{
			return;
		}
	}

	saveCurrentNote();

	/* update global counter */
	doc_page--;
	doc_page = (doc_page < 0 ? doc_n_pages - 1 : doc_page);

	/* update beamer counter if it's active */
	if (beamer_active == TRUE)
		doc_page_beamer = doc_page;
}

static void toggleCurserVisibility()
{
	/* Could happen right after startup ... dunno, better check it. */
	if (win_beamer == NULL)
		return;

	/* Toggle cursor visibility on beamer window. */
	if (isCurserVisible == FALSE)
	{
		gdk_window_set_cursor(gtk_widget_get_window(win_beamer),
				gdk_cursor_new(GDK_ARROW));
		isCurserVisible = TRUE;
	}
	else
	{
		gdk_window_set_cursor(gtk_widget_get_window(win_beamer),
				gdk_cursor_new(GDK_BLANK_CURSOR));
		isCurserVisible = FALSE;
	}
}

static void toggleFullScreen(void)
{
	/* Could happen right after startup ... dunno, better check it. */
	if (win_beamer == NULL)
		return;

	/* We have global reference to the beamer window, so we know exactly
	 * on which object fullscreen must be toggled. */
	if (isFullScreen == FALSE)
	{
		gdk_window_fullscreen(gtk_widget_get_window(win_beamer));
		isFullScreen = TRUE;
	}
	else
	{
		gdk_window_unfullscreen(gtk_widget_get_window(win_beamer));
		isFullScreen = FALSE;
	}
}

/* Starts, pauses  and continues the timer */
static void toggleTimer()
{
	switch (timerMode)
	{
		case 0:
			timer = g_timer_new();
			timerMode = 1;
			gtk_button_set_label(GTK_BUTTON(startButton), "Pause");
			break;
		case 1:
			g_timer_stop(timer);
			timerMode = 2;
			gtk_button_set_label(GTK_BUTTON(startButton), "Continue");
			break;
		case 2:
			g_timer_continue(timer);
			timerMode = 1;
			gtk_button_set_label(GTK_BUTTON(startButton), "Pause");
			break;
	}
}

static void resetTimer()
{
	if (timer != NULL)
	{
		g_timer_destroy(timer);
		timer = NULL;
	}
	timerMode = 0;
	gtk_button_set_label(GTK_BUTTON(startButton), "Start");
}

static gboolean printTimeElapsed(GtkWidget *timeElapsedLabel)
{
	int timeElapsed;
	gchar *timeToSet = NULL;
	gchar *textSize = NULL;

	if (timerMode > 0)
	{
		timeElapsed = (int)g_timer_elapsed(timer,NULL);

		int min = (int)timeElapsed / 60.0;
		int sec = timeElapsed % 60;
		timeToSet = g_strdup_printf("%02d:%02d", min, sec);

		textSize = g_markup_printf_escaped(
				"<span font=\"%d\">%s</span>", FONT_SIZE, timeToSet);
		gtk_label_set_markup(GTK_LABEL(timeElapsedLabel), textSize);
		g_free(textSize);
		g_free(timeToSet);
	}
	else
	{
		textSize = g_markup_printf_escaped(
				"<span font=\"%d\">%s</span>", FONT_SIZE, "00:00");
		gtk_label_set_markup(GTK_LABEL(timeElapsedLabel), textSize);
		g_free(textSize);
	}

	return TRUE;
}

static void initNotes(void)
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

static void readNotes(char *filename)
{
	char *databuf = NULL, *onlyText = NULL;
	gchar **splitNotes = NULL;
	struct stat statbuf;
	FILE *fp = NULL;
	int thatSlide = -1, i = 0, splitAt = 0;
	GtkWidget *dialog = NULL;

	/* try to load the file */
	if (stat(filename, &statbuf) == -1)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Could not stat file: %s.",
				g_strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	/* allocate one additional byte so that we can store a null
	 * terminator. */
	databuf = (char *)malloc(statbuf.st_size + 1);
	dieOnNull(databuf, __LINE__);

	fp = fopen(filename, "r");
	if (!fp)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Could not open file for reading: %s.",
				g_strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		free(databuf);
		return;
	}

	if (fread(databuf, 1, statbuf.st_size, fp) != statbuf.st_size)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Unexpected end of file.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		free(databuf);
		fclose(fp);
		return;
	}

	fclose(fp);

	/* terminate the string. otherwise, g_strsplit won't be able to
	 * determine where it ends. */
	databuf[statbuf.st_size] = 0;

	/* init notes with empty string */
	initNotes();

	/* split notes, parse slide numbers and replace entries */
	/* TODO: Spit out a warning when there are more notes than slides */
	splitNotes = g_strsplit(databuf, "-- ", 0);
	for (i = 0; i < doc_n_pages; i++)
	{
		for (splitAt = 0; splitAt < g_strv_length(splitNotes); splitAt++)
		{
			sscanf(splitNotes[splitAt], "%d\n", &thatSlide);
			if (thatSlide == (i + 1))
			{
				/* skip slide number and line break */
				/* FIXME: I bet there's a better way to do this. */
				onlyText = strstr(splitNotes[splitAt], "\n");

				if (onlyText != NULL)
				{
					/* if onlyText is NOT null, it'll point to the line
					 * break. that means we're safe to advance one
					 * character -- in the worst case, we'll end up on
					 * the null terminator.
					 *
					 * FIXME: I still bet there's a better way.
					 */
					onlyText += sizeof(char);

					/* replace note text */
					if (notes[i] != NULL)
						g_free(notes[i]);

					notes[i] = g_strdup(onlyText);

					/* quit inner for() */
					break;
				}
			}
		}
	}
	g_strfreev(splitNotes);

	/* print current note */
	printNote(doc_page + 1);

	/* that buffer isn't needed anymore: */
	free(databuf);
}

static void saveNotes(char *filename)
{
	int i;
	FILE *fp = NULL;
	GtkWidget *dialog = NULL;

	fp = fopen(filename, "w");
	if (!fp)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Could not open file for writing: %s.",
				g_strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	for (i = 0; i < doc_n_pages; i++)
	{
		/* if there's a note, write it to the file */
		if (notes[i] != NULL && g_strcmp0("", notes[i]) != 0)
		{
			if (fprintf(fp, "-- %d\n%s\n\n", (i + 1), notes[i]) < 0)
			{
				dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"Could not write to file.");
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);

				fclose(fp);
				return;
			}
		}
	}

	fclose(fp);

	dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"Notes saved.");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void onOpenClicked(GtkWidget *widget, gpointer data)
{
	GtkWidget *fileChooser = NULL;
	fileChooser = gtk_file_chooser_dialog_new("Open File", NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	if (gtk_dialog_run(GTK_DIALOG(fileChooser)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename = NULL;
		filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(fileChooser));
		readNotes(filename);
		g_free(filename);
	}
	gtk_widget_destroy(fileChooser);
}

static void onSaveClicked(GtkWidget *widget, gpointer data)
{
	GtkWidget *fileChooser = NULL;
	fileChooser = gtk_file_chooser_dialog_new("Save File", NULL,
			GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	if (gtk_dialog_run(GTK_DIALOG(fileChooser)) == GTK_RESPONSE_ACCEPT)
	{
		saveCurrentNote();

		char *filename = NULL;
		filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(fileChooser));
		saveNotes(filename);
		g_free(filename);
	}
	gtk_widget_destroy(fileChooser);
}

static void onEditToggled(GtkWidget *widget, gpointer data)
{
	if (gtk_toggle_tool_button_get_active(
				GTK_TOGGLE_TOOL_BUTTON(widget)))
	{
		isInsideNotePad = TRUE;
		gtk_text_view_set_editable(GTK_TEXT_VIEW(notePad), TRUE);
		gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(notePad), TRUE);
	}
	else
	{
		isInsideNotePad = FALSE;
		gtk_text_view_set_editable(GTK_TEXT_VIEW(notePad), FALSE);
		gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(notePad), FALSE);
	}
}

static gboolean onKeyPressed(GtkWidget *widg, GdkEventKey *ev,
		gpointer user_data)
{
	/* When inside the note pad, don't do anything here. */
	if (isInsideNotePad)
		return FALSE;

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
			/* this shall trigger a hard refresh, so empty the cache. */
			clearCache();
			break;

		case GDK_w:
			fitmode = FIT_WIDTH;
			break;

		case GDK_h:
			fitmode = FIT_HEIGHT;
			break;

		case GDK_p:
			fitmode = FIT_PAGE;
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

		case GDK_f:
			toggleFullScreen();
			break;

		case GDK_s:
			toggleTimer();
			break;

		case GDK_c:
			toggleCurserVisibility();
			break;

		case GDK_r:
			resetTimer();
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

static gboolean onMouseReleased(GtkWidget *widg, GdkEventButton *ev,
		gpointer user_data)
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

static void onResize(GtkWidget *widg, GtkAllocation *al,
		struct viewport *port)
{
	int wOld = port->width;
	int hOld = port->height;

	port->width = al->width;
	port->height = al->height;

	/* if the new size differs from the old size, then
	 * re-render this particular viewport. */
	if (wOld != port->width || hOld != port->height)
	{
		updatePortPixbuf(port);
	}
}

static void usage(char *exe)
{
	fprintf(stderr, "Usage: %s [-s <slides>] [-n] [-w] <file>\n", exe);
}

int main(int argc, char **argv)
{
	/* TODO: Split this function. */

	int i = 0, transIndex = 0, numframes;
	char *filename = NULL;
	FILE *fp = NULL;
	struct stat statbuf;
	char *databuf = NULL;
	GtkWidget *buttonBox = NULL,
			  *timeBox = NULL,
			  *notePadBox = NULL,
			  *notePadScroll = NULL,
			  *table = NULL;
	GError *err = NULL;
	GtkWidget *image = NULL,
			  *frame = NULL,
			  *evbox = NULL,
			  *outerevbox = NULL,
			  *timeFrame = NULL;
	GdkColor black;
	GtkWidget *timeElapsedLabel = NULL, *resetButton = NULL;
	gchar *textSize = NULL;

	GtkWidget *toolbar = NULL;
	GtkToolItem *openButton = NULL,
				*saveButton = NULL,
				*editButton = NULL;

	PangoFontDescription *font_desc = NULL;

	struct viewport *thisport = NULL;

	gtk_init(&argc, &argv);

	/* defaults */
	filename = NULL;
	numframes = 3;

	/* get options via getopt */
	while ((i = getopt(argc, argv, "s:wnc:")) != -1)
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

			case 'n':
				do_notectrl = TRUE;
				break;

			case 'c':
				/* don't care if that number is invalid. it'll get
				 * re-adjusted anyway if it's too small. */
				cache_max = atoi(optarg);
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

	/* for the cache to be useful, we'll need at least "some" items.
	 * that is 2 items (prev and next) per preview viewport and 2
	 * items for the beamer port.
	 *
	 * this means that switching to the previous and next slide will
	 * always be fast.
	 */
	if (cache_max < (numframes + 1) * 2)
		cache_max = (numframes + 1) * 2;

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

	/* now we know how many pages there are, so init our notes */
	initNotes();

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

	g_signal_connect(G_OBJECT(win_preview), "delete_event",
			G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_preview), "destroy",
			G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_beamer), "delete_event",
			G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(win_beamer), "destroy",
			G_CALLBACK(gtk_main_quit), NULL);

	g_signal_connect(G_OBJECT(win_preview), "key_press_event",
			G_CALLBACK(onKeyPressed), NULL);
	g_signal_connect(G_OBJECT(win_beamer), "key_press_event",
			G_CALLBACK(onKeyPressed), NULL);

	gtk_widget_add_events(win_beamer, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(win_beamer, GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(G_OBJECT(win_beamer), "button_release_event",
			G_CALLBACK(onMouseReleased), NULL);

	gtk_widget_add_events(win_preview, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(win_preview, GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(G_OBJECT(win_preview), "button_release_event",
			G_CALLBACK(onMouseReleased), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(win_preview), 10);
	gtk_container_set_border_width(GTK_CONTAINER(win_beamer), 0);

	gtk_widget_modify_bg(win_beamer, GTK_STATE_NORMAL, &black);

	/* create buttons */
	buttonBox = gtk_hbox_new(TRUE, 3);

	startButton = gtk_button_new();
	gtk_widget_set_size_request(startButton, 70, 30);
	gtk_button_set_label(GTK_BUTTON(startButton), "Start");
	g_signal_connect(G_OBJECT(startButton), "clicked",
			G_CALLBACK(toggleTimer), NULL);

	resetButton = gtk_button_new();
	gtk_widget_set_size_request(resetButton, 70, 30);
	gtk_button_set_label(GTK_BUTTON(resetButton), "Reset");
	g_signal_connect(G_OBJECT(resetButton), "clicked",
			G_CALLBACK(resetTimer), NULL);

	gtk_box_pack_start(GTK_BOX(buttonBox), startButton, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(buttonBox), resetButton, FALSE, FALSE, 5);

	/* setting text size for time label */
	textSize = g_markup_printf_escaped("<span font=\"%d\">%s</span>",
			FONT_SIZE, "00:00");
	timeElapsedLabel = gtk_label_new(NULL);
	gtk_label_set_markup (GTK_LABEL(timeElapsedLabel), textSize);
	g_free(textSize);

	/* create timer */
	timeBox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(timeBox), timeElapsedLabel,
			TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(timeBox), buttonBox,
			FALSE, FALSE, 5);

	timeFrame = gtk_frame_new("Timer");
	gtk_container_add(GTK_CONTAINER(timeFrame), timeBox);

	/* create note pad inside a scrolled window */
	notePadBox = gtk_vbox_new(FALSE, 2);
	notePadScroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(notePadScroll), 5);
	notePadFrame = gtk_frame_new("Notes for current slide");
	notePad = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(notePad), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(notePad), FALSE);
	gtk_container_add(GTK_CONTAINER(notePadScroll), notePad);
	gtk_scrolled_window_set_shadow_type(
			GTK_SCROLLED_WINDOW(notePadScroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(notePadScroll),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(notePadBox), notePadScroll, TRUE,
			TRUE, 2);

	/* set note pad font and margin */
	font_desc = pango_font_description_from_string("Sans 12");
	gtk_widget_modify_font(notePad, font_desc);
	pango_font_description_free(font_desc);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(notePad), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(notePad), 5);

	noteBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notePad));

	/* create toolbar */
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 5);

	openButton = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), openButton, -1);
	g_signal_connect(G_OBJECT(openButton), "clicked",
			G_CALLBACK(onOpenClicked), NULL);

	saveButton = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), saveButton, -1);
	g_signal_connect(G_OBJECT(saveButton), "clicked",
			G_CALLBACK(onSaveClicked), NULL);

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
			gtk_separator_tool_item_new(), -1);

	editButton = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_EDIT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), editButton, -1);
	g_signal_connect(G_OBJECT(editButton), "toggled",
			G_CALLBACK(onEditToggled), NULL);

	gtk_box_pack_start(GTK_BOX(notePadBox), toolbar, FALSE, FALSE, 2);
	gtk_container_add(GTK_CONTAINER(notePadFrame), notePadBox);

	/* init containers for "preview" */
	table = gtk_table_new(numframes, numframes + 1, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);

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

		/* create a new drawing area - the pdf will be rendered in
		 * there */
		image = gtk_image_new();
		gtk_widget_set_size_request(image, 100, 100);

		/* add widgets to their parents. the image is placed in an
		 * eventbox, the box's size_allocate signal will be handled. so,
		 * we know the exact width/height we can render into. (placing
		 * the image into the frame would create the need of knowing the
		 * frame's border size...)
		 */
		evbox = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(evbox), image);
		gtk_container_add(GTK_CONTAINER(frame), evbox);

		/* every frame will be placed in another eventbox so we can set a
		 * background color */
		outerevbox = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(outerevbox), frame);

		if (i == 0)
		{
			gtk_table_attach_defaults(GTK_TABLE(table), notePadFrame,
					0, 1, 0, numframes - 1);
			gtk_table_attach_defaults(GTK_TABLE(table), outerevbox,
					0, 1, numframes - 1, numframes);
		}
		else
		{
			if (i == numframes - 1)
			{
				gtk_table_attach_defaults(GTK_TABLE(table), outerevbox,
						numframes, numframes + 1,
						0, 1);
				gtk_table_attach_defaults(GTK_TABLE(table), timeFrame,
						numframes, numframes + 1,
						numframes - 1, numframes);
			}
			else
			{
				if (i == (int)(numframes / 2))
				{
					gtk_table_attach_defaults(GTK_TABLE(table),
							outerevbox, i, i + 2, 0, numframes);
				}
				else
				{
					if (i < (int)(numframes / 2))
					{
						gtk_table_attach_defaults(GTK_TABLE(table),
								outerevbox, i, i + 1,
								numframes - i - 1, numframes - i);
					}
					else
					{
						gtk_table_attach_defaults(GTK_TABLE(table),
								outerevbox, i + 1, i + 2,
								numframes - i - 1, numframes - i);
					}
				}
			}
		}

		/* make the eventbox "transparent" */
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(evbox), FALSE);

		/* save info of this rendering port */
		thisport = (struct viewport *)malloc(sizeof(struct viewport));
		dieOnNull(thisport, __LINE__);
		thisport->offset = transIndex;
		thisport->image = image;
		thisport->frame = frame;
		thisport->pixbuf = NULL;
		thisport->width = -1;
		thisport->height = -1;
		thisport->isBeamer = FALSE;
		ports = g_list_append(ports, thisport);

		/* resize callback */
		g_signal_connect(G_OBJECT(evbox), "size_allocate",
				G_CALLBACK(onResize), thisport);
	}

	gtk_container_add(GTK_CONTAINER(win_preview), table);

	/* in order to set the initially highlighted frame */
	refreshFrames();

	/* add a rendering area to the beamer window */
	image = gtk_image_new();
	gtk_widget_set_size_request(image, 320, 240);

	gtk_container_add(GTK_CONTAINER(win_beamer), image);

	/* save info of this rendering port */
	thisport = (struct viewport *)malloc(sizeof(struct viewport));
	dieOnNull(thisport, __LINE__);
	thisport->offset = 0;
	thisport->image = image;
	thisport->frame = NULL;
	thisport->pixbuf = NULL;
	thisport->width = -1;
	thisport->height = -1;
	thisport->isBeamer = TRUE;
	ports = g_list_append(ports, thisport);

	/* connect the on-resize-callback directly to the window */
	g_signal_connect(G_OBJECT(win_beamer), "size_allocate",
			G_CALLBACK(onResize), thisport);

	/* show the windows */
	gtk_widget_show_all(win_preview);
	gtk_widget_show_all(win_beamer);

	/* now, as the real gdk window exists, hide mouse cursor in the
	 * beamer window */
	gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(win_beamer)),
			gdk_cursor_new(GDK_BLANK_CURSOR));

	g_timeout_add(500, (GSourceFunc) printTimeElapsed,
			(gpointer) timeElapsedLabel);

	/* queue initial prerendering. */
	preQueued = TRUE;
	g_idle_add(idleFillCaches, NULL);

	gtk_main();
	exit(EXIT_SUCCESS);
}
