/*
	Copyright 2009-2012 Peter Hofmann

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/poppler.h>

#include "pdfpres.h"
#include "popplergdk.h"
#include "prefs.h"
#include "notes.h"

#ifndef PDFPRES_VERSION
/* If no version has been defined yet, show next milestone. */
#define PDFPRES_VERSION "pdfpres-0.3-pre (non-git)"
#endif


/* TODO: Clean up all that stuff. */

struct _runtimePreferences runpref;

static GList *ports = NULL;
GtkWidget *win_preview = NULL;
static GtkWidget *win_beamer = NULL;
static GtkWidget *mainStatusbar = NULL;

static GList *cache = NULL;

static PopplerDocument *doc = NULL;

int doc_n_pages = 0;
int doc_page = 0;
static int doc_page_mark = 0;
static int doc_page_beamer = 0;
static int target_page = -1;

static gboolean beamer_active = TRUE;

static gboolean isFullScreen = FALSE;
static gboolean isCurserVisible = FALSE;
static gboolean isInsideNotePad = FALSE;
static gboolean isUserAction = FALSE;
static gboolean isSaved = TRUE;
static gboolean isBlank = FALSE;
static char *savedAsFilename = NULL;
static char *lastFolder = NULL;

static gboolean preQueued = FALSE;

static GTimer *timer = NULL;
static int timerMode = 0; /* 0 = stopped, 1 = running, 2 = paused */
static GtkToolItem *saveButton = NULL,
				   *editButton = NULL,
				   *startButton = NULL,
				   *resetButton = NULL;
static GtkWidget *notePad = NULL, *notePadFrame = NULL;
static GtkWidget *timeElapsedLabel = NULL;
GtkTextBuffer *noteBuffer = NULL;

static GdkColor col_current, col_marked, col_dim;


static void onSaveClicked(GtkWidget *widget, gpointer data);
static void onSaveAsClicked(GtkWidget *widget, gpointer data);


static void dieOnNull(void *ptr, int line)
{
	if (ptr == NULL)
	{
		fprintf(stderr, "Out of memory in line %d.\n", line);
		exit(EXIT_FAILURE);
	}
}

void setStatusText_strdup(gchar *msg)
{
	static gchar *curMsg = NULL;

	if (mainStatusbar == NULL)
		return;

	/* Remove current message with context id 0. */
	gtk_statusbar_pop(GTK_STATUSBAR(mainStatusbar), 0);

	/* Free last message if any -- and store pointer to new message. */
	if (curMsg != NULL)
		g_free(curMsg);

	curMsg = g_strdup(msg);

	/* Set new message. */
	gtk_statusbar_push(GTK_STATUSBAR(mainStatusbar), 0, curMsg);
}

static void saveLastFolderFrom(GtkWidget *widget)
{
	if (lastFolder != NULL)
		g_free(lastFolder);

	lastFolder = gtk_file_chooser_get_current_folder(
			GTK_FILE_CHOOSER(widget));
}

static void setLastFolderOn(GtkWidget *widget)
{
	if (lastFolder == NULL)
		return;

	gtk_file_chooser_set_current_folder(
			GTK_FILE_CHOOSER(widget), lastFolder);
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
	if (runpref.fit_mode == FIT_PAGE)
	{
		/* that's it: compare screen and page ratio. this
		 * will cover all 4 cases that could happen. */
		if (screen_ratio > page_ratio)
			myfitmode = FIT_HEIGHT;
		else
			myfitmode = FIT_WIDTH;
	}
	else
		myfitmode = runpref.fit_mode;

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
		if (g_list_length(cache) + 1 > runpref.cache_max)
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
					g_object_unref(ci->pixbuf);

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
	 * output of pdfpres to your other tool.
	 */
	if (pp->offset == 0 && !pp->isBeamer)
	{
		printNote(doc_page + 1);
		if (runpref.do_notectrl)
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
	/* Unused parameters. */
	(void)dummy;

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
			g_object_unref(ci->pixbuf);

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

static void nextSlide(void)
{
	/* stop if we're at the end and wrapping is disabled. */
	if (!runpref.do_wrapping)
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
	if (!runpref.do_wrapping)
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

static void moveBeamerToMouseMonitor(void)
{
	GdkDisplay *dpy = NULL;
	GdkScreen *scr = NULL;
	GdkRectangle rect;
	int mx = -1, my = -1, mon = -1;

	/* Open default display. Then get the current position of the mouse
	 * cursor and the screen that cursor is on. */
	dpy = gdk_display_get_default();
	if (dpy == NULL)
	{
		/* Actually, this should not happen because we are already able
		 * to create windows on the default screen. */
		fprintf(stderr, "Could not get default display.\n");
		return;
	}
	gdk_display_get_pointer(dpy, &scr, &mx, &my, NULL);

	/* Get the number of the monitor at the current mouse position, as
	 * well as the geometry (offset, size) of that monitor. */
	mon = gdk_screen_get_monitor_at_point(scr, mx, my);
	gdk_screen_get_monitor_geometry(scr, mon, &rect);

	/* Move the beamer window to the upper left corner of the current
	 * monitor. */
	gtk_window_move(GTK_WINDOW(win_beamer), rect.x, rect.y);
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
		moveBeamerToMouseMonitor();
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
	if (prefs.timer_is_clock)
		return;

	switch (timerMode)
	{
		case 0:
			timer = g_timer_new();
			timerMode = 1;
			gtk_widget_set_sensitive(GTK_WIDGET(resetButton), FALSE);
			gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(startButton),
					GTK_STOCK_MEDIA_PAUSE);
			break;
		case 1:
			g_timer_stop(timer);
			timerMode = 2;
			gtk_widget_set_sensitive(GTK_WIDGET(resetButton), TRUE);
			gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(startButton),
					GTK_STOCK_MEDIA_PLAY);
			break;
		case 2:
			g_timer_continue(timer);
			timerMode = 1;
			gtk_widget_set_sensitive(GTK_WIDGET(resetButton), FALSE);
			gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(startButton),
					GTK_STOCK_MEDIA_PAUSE);
			break;
	}
}


static void toggleBlankBeamer()
{
	struct viewport *pp = NULL;
	GList *it = ports;

	while (it)
	{
		pp = (struct viewport *)(it->data);

		if (pp->isBeamer)
		{
			if (isBlank)
			{
				gtk_widget_show(pp->image);
			}
			else
			{
				gtk_widget_hide(pp->image);
			}

			isBlank = !isBlank;
			return;
		}

		it = g_list_next(it);
	}
}

static void resetTimer()
{
	if (prefs.timer_is_clock)
		return;

	if (timerMode == 1)
		return;

	if (timer != NULL)
	{
		g_timer_destroy(timer);
		timer = NULL;
	}
	timerMode = 0;
}

static gboolean printCurrentTime(GtkWidget *timeElapsedLabel)
{
	char nowFmt[6] = "";
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	if (tmp == NULL)
	{
		perror("localtime");
		return TRUE;
	}
	if (strftime(nowFmt, sizeof(nowFmt), "%R", tmp) == 0)
	{
		fprintf(stderr, "strftime failed: 0\n");
		return TRUE;
	}
	gtk_label_set_text(GTK_LABEL(timeElapsedLabel), nowFmt);

	return TRUE;
}

static gboolean printTimeElapsed(GtkWidget *timeElapsedLabel)
{
	int timeElapsed;
	gchar *timeToSet = NULL;

	if (timerMode > 0)
	{
		timeElapsed = (int)g_timer_elapsed(timer, NULL);

		int min = (int)timeElapsed / 60.0;
		int sec = timeElapsed % 60;
		timeToSet = g_strdup_printf("%02d:%02d", min, sec);

		gtk_label_set_text(GTK_LABEL(timeElapsedLabel), timeToSet);
		g_free(timeToSet);
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(timeElapsedLabel), "00:00");
	}

	return TRUE;
}

static gboolean handleUnsavedNotes()
{
	GtkWidget *dialog = NULL;
	gint response = 0;

	/* See if there are unsaved changes. */
	if (!isSaved)
	{
		/* What to do? */
		if (savedAsFilename == NULL)
		{
			dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					"There are unsaved notes. Save them now?");
		}
		else
		{
			dialog = gtk_message_dialog_new(GTK_WINDOW(win_preview),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					"There are unsaved notes. Save them now?\n"
					"\n"
					"They will be saved to: `%s'.",
					savedAsFilename);
		}
		gtk_dialog_add_buttons(GTK_DIALOG(dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_NO, GTK_RESPONSE_NO,
				GTK_STOCK_YES, GTK_RESPONSE_YES,
				NULL);

		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(GTK_WIDGET(dialog));

		/* Just abort. */
		if (response == GTK_RESPONSE_CANCEL)
			return FALSE;

		/* Abandon notes. Consider them saved and get out of here. */
		else if (response == GTK_RESPONSE_NO)
		{
			isSaved = TRUE;
			return TRUE;
		}

		/* Give the user the opportunity to save them. */
		if (savedAsFilename == NULL)
			onSaveAsClicked(NULL, NULL);
		else
			onSaveClicked(NULL, NULL);

		/* Are they saved now? That is, don't quit if he cancelled. */
		if (!isSaved)
			return FALSE;
	}

	return TRUE;
}

static gboolean onQuit(GtkWidget *widget, GdkEvent *ev, gpointer dummy)
{
	/* Unused parameters. */
	(void)widget;
	(void)ev;
	(void)dummy;

	/* When there are unsaved notes, the user may chose not to quit. */
	if (!handleUnsavedNotes())
		return TRUE;

	/* Save preferences. */
	savePreferences();

	gtk_main_quit();
	return FALSE;
}

static void showNotesFromFile(gchar *notefile)
{
	gchar *msg = NULL;

	if (savedAsFilename != NULL)
		g_free(savedAsFilename);

	savedAsFilename = notefile;

	if (readNotes(notefile))
	{
		msg = g_strdup_printf("Notes read from '%s'.", notefile);
		setStatusText_strdup(msg);
		g_free(msg);

		isSaved = TRUE;
		gtk_widget_set_sensitive(GTK_WIDGET(saveButton), FALSE);

		printNote(doc_page + 1);
	}
}

static void onOpenClicked(GtkWidget *widget, gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

	GtkWidget *fileChooser = NULL;
	gchar *filename;

	if (!handleUnsavedNotes())
		return;

	fileChooser = gtk_file_chooser_dialog_new("Open File", NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	setLastFolderOn(fileChooser);

	if (gtk_dialog_run(GTK_DIALOG(fileChooser)) == GTK_RESPONSE_ACCEPT)
	{
		saveLastFolderFrom(fileChooser);

		filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(fileChooser));

		showNotesFromFile(filename);
	}
	gtk_widget_destroy(fileChooser);
}

static void onSaveAsClicked(GtkWidget *widget, gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

	gchar *msg = NULL;
	GtkWidget *fileChooser = NULL;

	fileChooser = gtk_file_chooser_dialog_new("Save File", NULL,
			GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_do_overwrite_confirmation(
			GTK_FILE_CHOOSER(fileChooser), TRUE);

	setLastFolderOn(fileChooser);

	if (gtk_dialog_run(GTK_DIALOG(fileChooser)) == GTK_RESPONSE_ACCEPT)
	{
		saveCurrentNote();

		if (savedAsFilename != NULL)
			g_free(savedAsFilename);

		savedAsFilename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(fileChooser));

		if (saveNotes(savedAsFilename))
		{
			saveLastFolderFrom(fileChooser);

			isSaved = TRUE;
			gtk_widget_set_sensitive(GTK_WIDGET(saveButton), FALSE);

			msg = g_strdup_printf("Notes saved as '%s'.",
					savedAsFilename);
			setStatusText_strdup(msg);
			g_free(msg);
		}
	}
	gtk_widget_destroy(fileChooser);
}

static void onSaveClicked(GtkWidget *widget, gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

	gchar *msg = NULL;

	if (savedAsFilename == NULL)
		return;

	saveCurrentNote();
	if (!saveNotes(savedAsFilename))
		return;

	msg = g_strdup_printf("Notes saved: '%s'.", savedAsFilename);
	setStatusText_strdup(msg);
	g_free(msg);

	isSaved = TRUE;
	gtk_widget_set_sensitive(GTK_WIDGET(saveButton), FALSE);
}

static void onFontSelectClick(GtkWidget *widget, gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

	GtkWidget *fontChooser = NULL;
	PangoFontDescription *font_desc = NULL;

	fontChooser = gtk_font_selection_dialog_new("Select Notes Font");
	gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(fontChooser), prefs.font_notes);

	if (gtk_dialog_run(GTK_DIALOG(fontChooser)) == GTK_RESPONSE_OK)
	{
		if (prefs.font_notes != NULL)
			g_free(prefs.font_notes);

		prefs.font_notes = gtk_font_selection_dialog_get_font_name(
				GTK_FONT_SELECTION_DIALOG(fontChooser));
		font_desc = pango_font_description_from_string(prefs.font_notes);
		gtk_widget_modify_font(notePad, font_desc);
		pango_font_description_free(font_desc);
	}

	gtk_widget_destroy(fontChooser);
}

static void onTimerFontSelectClick(GtkWidget *widget, gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

	GtkWidget *fontChooser = NULL;
	PangoFontDescription *font_desc = NULL;

	fontChooser = gtk_font_selection_dialog_new("Select Timer Font");
	gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(fontChooser), prefs.font_timer);

	if (gtk_dialog_run(GTK_DIALOG(fontChooser)) == GTK_RESPONSE_OK)
	{
		if (prefs.font_timer != NULL)
			g_free(prefs.font_timer);

		prefs.font_timer = gtk_font_selection_dialog_get_font_name(
				GTK_FONT_SELECTION_DIALOG(fontChooser));
		font_desc = pango_font_description_from_string(prefs.font_timer);
		gtk_widget_modify_font(timeElapsedLabel, font_desc);
		pango_font_description_free(font_desc);
	}

	gtk_widget_destroy(fontChooser);
}

static void setEditingState(gboolean state)
{
	isInsideNotePad = state;
	gtk_text_view_set_editable(GTK_TEXT_VIEW(notePad), state);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(notePad), state);
	gtk_toggle_tool_button_set_active(
			GTK_TOGGLE_TOOL_BUTTON(editButton), state);

	if (state)
	{
		gtk_widget_grab_focus(notePad);
	}
}

static void onEditToggled(GtkWidget *widget, gpointer data)
{
	/* Unused parameters. */
	(void)data;

	gboolean newState = FALSE;

	if (gtk_toggle_tool_button_get_active(
				GTK_TOGGLE_TOOL_BUTTON(widget)))
	{
		newState = TRUE;
	}
	else
	{
		newState = FALSE;
	}

	/* Don't do anything if there's no change. */
	if (newState != isInsideNotePad)
		setEditingState(newState);
}

static void onBeginUserAction(GtkTextBuffer *buf, gpointer dummy)
{
	/* Unused parameters. */
	(void)buf;
	(void)dummy;

	isUserAction = TRUE;
}

static void onEndUserAction(GtkTextBuffer *buf, gpointer dummy)
{
	/* Unused parameters. */
	(void)buf;
	(void)dummy;

	isUserAction = FALSE;
}

static void onEditing(GtkTextBuffer *buf, gpointer dummy)
{
	/* Unused parameters. */
	(void)buf;
	(void)dummy;

	if (isUserAction)
	{
		isSaved = FALSE;

		if (savedAsFilename != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(saveButton), TRUE);
	}
}

static gboolean onPadKeyPressed(GtkWidget *widget, GdkEventKey *ev,
		gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

	switch (ev->keyval)
	{
		case GDK_Escape:
			setEditingState(FALSE);
			break;
	}

	return FALSE;
}

static int executeJump(void)
{
	/*
	 * 0 = No jump pending, nothing done.
	 * 1 = Jump succeeded.
	 * 2 = Jump was pending, but target page invalid.
	 */
	int retval = 0;

	/* Jump? */
	if (target_page >= 0)
	{
		target_page--;

		/* Restrict to valid range. */
		if (target_page >= 0 && target_page < doc_n_pages)
		{
			doc_page = target_page;
			doc_page_beamer = target_page;
			setStatusText_strdup("Ready.");
			retval = 1;
		}
		else
		{
			setStatusText_strdup("Invalid page.");
			retval = 2;
		}

		/* Reset value to: "no jump pending". */
		target_page = -1;
	}

	return retval;
}

static gboolean onKeyPressed(GtkWidget *widget, GdkEventKey *ev,
		gpointer data)
{
	/* Unused parameters. */
	(void)data;

	guint key = ev->keyval;
	gchar *msg = NULL;

	/* When inside the note pad, don't do anything here. */
	if (isInsideNotePad)
		return FALSE;

	/* Jump command?
	 *
	 * Note: This works as long as the values of GDK keysyms satisfy:
	 *   1)  GDK_0 < GDK_1 < GDK_2 < ... < GDK_9
	 *   2)  All of them must be >= 0.
	 */
	key -= GDK_0;
	if (key <= 9)
	{
		/* The initial value is -1, so we have to reset this on the
		 * first key stroke. */
		if (target_page < 0)
			target_page = 0;

		/* Do a "decimal left shift" and add the given value. */
		target_page *= 10;
		target_page += (int)key;

		/* Catch overflow and announce what would happen. */
		if (target_page < 0)
		{
			target_page = -1;
			setStatusText_strdup("Invalid page.");
		}
		else
		{
			msg = g_strdup_printf("Jump to page: %d", target_page);
			setStatusText_strdup(msg);
			g_free(msg);
		}

		return FALSE;
	}

	gboolean changed = TRUE;
	saveCurrentNote();

	switch (ev->keyval)
	{
		case GDK_Right:
		case GDK_Down:
		case GDK_Page_Down:
		case GDK_space:
			nextSlide();
			break;

		case GDK_Left:
		case GDK_Up:
		case GDK_Page_Up:
			prevSlide();
			break;

		case GDK_F5:
			/* Switch to fullscreen (if needed) and start the timer
			 * (unless it's already running). */
			if (!isFullScreen)
				toggleFullScreen();
			if (timerMode != 1)
				toggleTimer();
			break;

		case GDK_F6:
			/* this shall trigger a hard refresh, so empty the cache. */
			clearCache();
			break;

		case GDK_w:
			runpref.fit_mode = FIT_WIDTH;
			break;

		case GDK_h:
			runpref.fit_mode = FIT_HEIGHT;
			break;

		case GDK_p:
			runpref.fit_mode = FIT_PAGE;
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
			changed = FALSE;
			break;

		case GDK_c:
			toggleCurserVisibility();
			break;

		case GDK_r:
			resetTimer();
			changed = FALSE;
			break;

		case GDK_Escape:
		case GDK_q:
			if (prefs.q_exits_fullscreen && isFullScreen)
			{
				toggleFullScreen();
				if (prefs.stop_timer_on_fs)
				{
					toggleTimer();
				}
			}
			else
			{
				changed = FALSE;
				onQuit(NULL, NULL, NULL);
			}
			break;

		case GDK_i:
			/* This must not work when we're on the beamer window. */
			if (widget != win_beamer)
				setEditingState(TRUE);

			changed = FALSE;
			break;

		case GDK_Return:
			if (executeJump() == 0)
				nextSlide();
			break;

		case GDK_G:
			executeJump();
			break;

		case GDK_period:
		case GDK_b:
			toggleBlankBeamer();
			changed = FALSE;
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

static gboolean onMouseReleased(GtkWidget *widget, GdkEventButton *ev,
		gpointer data)
{
	/* Unused parameters. */
	(void)widget;
	(void)data;

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

static void onResize(GtkWidget *widget, GtkAllocation *al,
		struct viewport *port)
{
	/* Unused parameters. */
	(void)widget;

	int wOld = port->width;
	int hOld = port->height;

	port->width = al->width;
	port->height = al->height;

	/* if the new size differs from the old size, then
	 * re-render this particular viewport. */
	if (wOld != port->width || hOld != port->height)
	{
		/* be sure to save the current notes because the following
		 * update will trigger a re-print of them. */
		saveCurrentNote();

		updatePortPixbuf(port);
	}
}

static void usage(char *exe)
{
	fprintf(stderr,
			"Usage: %s [-c <cache items>] [-s <slides>] [-n] "
			"[-N <note file>] [-w] <file>\n", exe);
}

static void initGUI(int numframes, gchar *notefile)
{
	int i = 0, transIndex = 0;
	GtkWidget *timeBox = NULL,
			  *notePadBox = NULL,
			  *notePadScroll = NULL,
			  *table = NULL;
	GtkWidget *image = NULL,
			  *frame = NULL,
			  *evbox = NULL,
			  *outerevbox = NULL,
			  *timeFrame = NULL;
	GtkWidget *mainVBox = NULL;
	GdkColor black;

	GtkWidget *toolbar = NULL, *timeToolbar = NULL;
	GtkToolItem *openButton = NULL,
				*saveAsButton = NULL,
				*fontSelectButton = NULL,
				*timeFontSelectButton = NULL;

	PangoFontDescription *font_desc = NULL;

	struct viewport *thisport = NULL;

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

	gtk_window_set_title(GTK_WINDOW(win_preview), "pdfpres - Preview");
	gtk_window_set_title(GTK_WINDOW(win_beamer),  "pdfpres - Beamer");

	g_signal_connect(G_OBJECT(win_preview), "delete_event",
			G_CALLBACK(onQuit), NULL);
	g_signal_connect(G_OBJECT(win_preview), "destroy",
			G_CALLBACK(onQuit), NULL);
	g_signal_connect(G_OBJECT(win_beamer), "delete_event",
			G_CALLBACK(onQuit), NULL);
	g_signal_connect(G_OBJECT(win_beamer), "destroy",
			G_CALLBACK(onQuit), NULL);

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

	gtk_container_set_border_width(GTK_CONTAINER(win_preview), 0);
	gtk_container_set_border_width(GTK_CONTAINER(win_beamer), 0);

	gtk_widget_modify_bg(win_beamer, GTK_STATE_NORMAL, &black);

	/* create buttons */
	timeToolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(timeToolbar), GTK_TOOLBAR_ICONS);
	gtk_container_set_border_width(GTK_CONTAINER(timeToolbar), 5);

	if (!prefs.timer_is_clock)
	{
		startButton = gtk_tool_button_new_from_stock(
				GTK_STOCK_MEDIA_PLAY);
		g_signal_connect(G_OBJECT(startButton), "clicked",
				G_CALLBACK(toggleTimer), NULL);
		gtk_toolbar_insert(GTK_TOOLBAR(timeToolbar), startButton, -1);

		resetButton = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_REWIND);
		g_signal_connect(G_OBJECT(resetButton), "clicked",
				G_CALLBACK(resetTimer), NULL);
		gtk_toolbar_insert(GTK_TOOLBAR(timeToolbar), resetButton, -1);

		gtk_toolbar_insert(GTK_TOOLBAR(timeToolbar),
				gtk_separator_tool_item_new(), -1);
	}

	timeFontSelectButton =
		gtk_tool_button_new_from_stock(GTK_STOCK_SELECT_FONT);
	gtk_toolbar_insert(GTK_TOOLBAR(timeToolbar),
			timeFontSelectButton, -1);
	g_signal_connect(G_OBJECT(timeFontSelectButton), "clicked",
			G_CALLBACK(onTimerFontSelectClick), NULL);

	/* setting text size for time label */
	timeElapsedLabel = gtk_label_new(NULL);
	font_desc = pango_font_description_from_string(prefs.font_timer);
	gtk_widget_modify_font(GTK_WIDGET(timeElapsedLabel), font_desc);
	pango_font_description_free(font_desc);
	if (prefs.timer_is_clock)
	{
		printCurrentTime(timeElapsedLabel);
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(timeElapsedLabel), "00:00");
	}

	/* Add timer label to another event box so we can set a nice border.
	 */
	evbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(evbox), timeElapsedLabel);
	gtk_container_set_border_width(GTK_CONTAINER(evbox), 10);

	/* create timer */
	timeBox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(timeBox), evbox,
			TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(timeBox), timeToolbar,
			FALSE, FALSE, 5);

	if (prefs.timer_is_clock)
	{
		timeFrame = gtk_frame_new("Clock");
	}
	else
	{
		timeFrame = gtk_frame_new("Timer");
	}
	gtk_container_add(GTK_CONTAINER(timeFrame), timeBox);

	/* create note pad inside a scrolled window */
	notePadBox = gtk_vbox_new(FALSE, 2);
	notePadScroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(notePadScroll), 5);
	notePadFrame = gtk_frame_new("Notes for current slide");
	notePad = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(notePad), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(notePad), FALSE);
	g_signal_connect(G_OBJECT(notePad), "key_press_event",
			G_CALLBACK(onPadKeyPressed), NULL);
	gtk_widget_set_size_request(notePad, 200, 50);

	/* Remarks:
	 *
	 * - The note pad uses word wrapping. If that's not enough, it also
	 *   uses wrapping on a per character basis.
	 * - The note pad is placed into a GtkScrolledWindow. This window
	 *   allows vertical scrolling but no horizontal scrolling.
	 */
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(notePad),
			GTK_WRAP_WORD_CHAR);
	gtk_container_add(GTK_CONTAINER(notePadScroll), notePad);
	gtk_scrolled_window_set_shadow_type(
			GTK_SCROLLED_WINDOW(notePadScroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(notePadScroll),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(notePadBox), notePadScroll, TRUE,
			TRUE, 2);

	/* set note pad font and margin */
	font_desc = pango_font_description_from_string(prefs.font_notes);
	gtk_widget_modify_font(notePad, font_desc);
	pango_font_description_free(font_desc);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(notePad), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(notePad), 5);

	noteBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notePad));

	/* We detect changes of the notes by catching the "changed" signal.
	 * As this signal is also emitted when we change the buffer
	 * programmatically, we first have a look if there was a
	 * "begin_user_action" signal. If so, the user has changed the
	 * buffer.
	 */
	g_signal_connect(G_OBJECT(noteBuffer), "changed",
			G_CALLBACK(onEditing), NULL);
	g_signal_connect(G_OBJECT(noteBuffer), "begin_user_action",
			G_CALLBACK(onBeginUserAction), NULL);
	g_signal_connect(G_OBJECT(noteBuffer), "end_user_action",
			G_CALLBACK(onEndUserAction), NULL);

	/* create toolbar */
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 5);

	openButton = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), openButton, -1);
	g_signal_connect(G_OBJECT(openButton), "clicked",
			G_CALLBACK(onOpenClicked), NULL);

	/* TODO: Tooltips?! */
	saveButton = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), saveButton, -1);
	gtk_widget_set_sensitive(GTK_WIDGET(saveButton), FALSE);
	g_signal_connect(G_OBJECT(saveButton), "clicked",
			G_CALLBACK(onSaveClicked), NULL);

	saveAsButton = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE_AS);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), saveAsButton, -1);
	g_signal_connect(G_OBJECT(saveAsButton), "clicked",
			G_CALLBACK(onSaveAsClicked), NULL);

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
			gtk_separator_tool_item_new(), -1);

	editButton = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_EDIT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), editButton, -1);
	g_signal_connect(G_OBJECT(editButton), "toggled",
			G_CALLBACK(onEditToggled), NULL);

	fontSelectButton =
		gtk_tool_button_new_from_stock(GTK_STOCK_SELECT_FONT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), fontSelectButton, -1);
	g_signal_connect(G_OBJECT(fontSelectButton), "clicked",
			G_CALLBACK(onFontSelectClick), NULL);

	gtk_box_pack_start(GTK_BOX(notePadBox), toolbar, FALSE, FALSE, 2);
	gtk_container_add(GTK_CONTAINER(notePadFrame), notePadBox);

	/* init containers for "preview" */
	table = gtk_table_new(numframes, numframes + 1, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 10);

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

	/* Add main content and a status bar to preview window.
	 *
	 * Note: It's important to use gtk_box_pack_* to add the statusbar
	 * because gtk_container_add will pick unappropriate defaults. */
	mainVBox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(mainVBox), table);

	mainStatusbar = gtk_statusbar_new();
	gtk_box_pack_end(GTK_BOX(mainVBox), mainStatusbar,
			FALSE, FALSE, 0);

	setStatusText_strdup("Ready.");

	gtk_container_add(GTK_CONTAINER(win_preview), mainVBox);

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

	/* load notes if requested */
	if (notefile)
	{
		showNotesFromFile(notefile);
	}

	/* show the windows */
	gtk_widget_show_all(win_preview);
	gtk_widget_show_all(win_beamer);

	/* now, as the real gdk window exists, hide mouse cursor in the
	 * beamer window */
	gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(win_beamer)),
			gdk_cursor_new(GDK_BLANK_CURSOR));

	/* Show a clock or a timer? */
	if (prefs.timer_is_clock)
	{
		g_timeout_add(500, (GSourceFunc)printCurrentTime,
				(gpointer)timeElapsedLabel);
	}
	else
	{
		g_timeout_add(500, (GSourceFunc)printTimeElapsed,
				(gpointer)timeElapsedLabel);
	}
}

int main(int argc, char **argv)
{
	int i = 0, numframes;
	char *filename = NULL;
	gchar *notefile = NULL;
	FILE *fp = NULL;
	struct stat statbuf;
	char *databuf = NULL;
	GError *err = NULL;

	gtk_init(&argc, &argv);

	/* Load preferences first. Command line options will override those
	 * preferences. */
	loadPreferences();

	/* Read defaults from preferences. */
	filename = NULL;
	numframes = 2 * prefs.slide_context + 1;
	runpref.do_wrapping = prefs.do_wrapping;
	runpref.do_notectrl = prefs.do_notectrl;
	runpref.cache_max = prefs.cache_max;
	runpref.fit_mode = prefs.initial_fit_mode;

	/* get options via getopt */
	while ((i = getopt(argc, argv, "s:wnc:N:CTv")) != -1)
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
				runpref.do_wrapping = TRUE;
				break;

			case 'n':
				runpref.do_notectrl = TRUE;
				break;

			case 'c':
				/* don't care if that number is invalid. it'll get
				 * re-adjusted anyway if it's too small. */
				runpref.cache_max = atoi(optarg);
				break;

			case 'N':
				notefile = g_strdup(optarg);
				break;

			case 'C':
				/* Force the timer to be a clock. */
				prefs.timer_is_clock = TRUE;
				break;

			case 'T':
				/* Force the timer to be a timer (not a clock). */
				prefs.timer_is_clock = FALSE;
				break;

			case 'v':
				printf("pdfpres version: %s\n", PDFPRES_VERSION);
				exit(EXIT_SUCCESS);
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
	 *
	 * note: numframes is not negative (see above), so that cast is okay.
	 */
	if (runpref.cache_max < (guint)((numframes + 1) * 2))
		runpref.cache_max = (guint)((numframes + 1) * 2);

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

	/* Read 1 element of size "statbuf.st_size". fread() returns the
	 * number of items successfully read. Thus, a return value of "1"
	 * means "success" and anything else is an error. */
	if (fread(databuf, statbuf.st_size, 1, fp) != 1)
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

	initGUI(numframes, notefile);

	/* queue initial prerendering. */
	preQueued = TRUE;
	g_idle_add(idleFillCaches, NULL);

	gtk_main();
	exit(EXIT_SUCCESS);
}
