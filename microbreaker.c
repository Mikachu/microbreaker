#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <sys/utsname.h>
#include <string.h>
#include <stdlib.h>

/* nicer gtk interface */
#include "gtkunion.h"

/* dockapp stuff */
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include "pixmaps/break.xpm"
#include "pixmaps/idle.xpm"

typedef enum {
  IDLE,
  BREAK,
  NUMICONS
} Break;

static Plug dockchild;
static GdkWindow *gdkdockapp;
static Image image;
static GdkPixmap *pixmap[NUMICONS];
static GdkBitmap *bitmap[NUMICONS];
static Break breaktime;
static gint delay = 300;
static guint timeout_id = 0;
static gchar *tooltip_text = NULL;

static void set_icon_state(Break on)
{
  breaktime = on;
  g_clear_pointer(&tooltip_text, g_free);
  gtk_tooltip_trigger_tooltip_query(gdk_display_get_default());  
  gtk_image_set_from_pixmap(image.i, pixmap[on], NULL);
  gdk_window_shape_combine_mask(gdkdockapp, bitmap[on], 0, 0);
}

static gboolean break_timeout(gpointer nodata)
{
  timeout_id = 0;
  set_icon_state(BREAK);
  /* Don't repeat timeout */
  return FALSE;
}

static void reset_break()
{
  g_clear_handle_id(&timeout_id, g_source_remove);
  timeout_id = g_timeout_add_seconds(delay, (GSourceFunc)break_timeout, &gdkdockapp);
  if (breaktime == BREAK)
    set_icon_state(IDLE);
}

static gboolean set_delay(Gtkwindow dialog, GdkEvent *event, Entry entry)
{
  delay = atoi(gtk_entry_get_text(entry.e));
  gtk_widget_hide(dialog.w);
  reset_break();
}

struct _GtkTooltip
{
  GObject parent_instance;
  GtkWidget *window;
  /* and a bunch of other gunk */
};

static gboolean query_tooltip(GtkWidget *widget, gint x, gint y,
                              gboolean keyboard_mode, GtkTooltip *tooltip,
                              gpointer nodata)
{
  if (!GTK_WIDGET_MAPPED(tooltip->window)) {
    g_free(tooltip_text);
    tooltip_text = NULL;
  }
  if (!tooltip_text) {
    if (breaktime == BREAK) {
      tooltip_text = g_strdup("Break time!");
    } else {
      GSource *src;
      if (timeout_id && (src = g_main_context_find_source_by_id(NULL, timeout_id))) {
        gint64 remaining_us = g_source_get_ready_time(src) - g_get_monotonic_time();
        gint remaining_s = (gint)(remaining_us / G_USEC_PER_SEC);
        if (remaining_s < 0) remaining_s = 0;
        tooltip_text = g_strdup_printf("%d:%02d until break", remaining_s / 60, remaining_s % 60);
      } else {
        tooltip_text = g_strdup("Break imminent");
      }
    }
  }
  gtk_tooltip_set_text(tooltip, tooltip_text);

  return TRUE;
}

static gboolean handle_dock_event(Plug dockchild, GdkEventButton *event, gpointer nodata)
{
  if (event->button == 1) {
    reset_break();
    return TRUE;
  } else if (event->button == 3) {
    static Gtkwindow dialog = { NULL };
    Entry entry;

    if (!dialog.w) {
      dialog.w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title(dialog.d, "Microbreaker");
      gtk_window_set_position(dialog.d, GTK_WIN_POS_CENTER);

      entry.w = gtk_entry_new();

      gtk_container_add(dialog.c, entry.w);

      g_signal_connect(dialog.o, "delete-event", G_CALLBACK(set_delay), entry.w);
    }

    gtk_widget_show_all(dialog.w);
  }

  return FALSE;
}

/* I'm sorry if this code offends anyone :) */
static void create_icon(int argc, char *argv[])
{
  Window dockapp;
  XWMHints *wm_hints;

  g_assert( idle_xpm[0][0] == '6' &&  idle_xpm[0][1] == '4' &&
            idle_xpm[0][3] == '2' &&  idle_xpm[0][4] == '4' &&
           break_xpm[0][0] == '6' && break_xpm[0][1] == '4' &&
           break_xpm[0][3] == '2' && break_xpm[0][4] == '4' &&
           "Size of pixmaps are wrong, should be 64x24.");
  dockapp = XCreateSimpleWindow(GDK_DISPLAY(), GDK_ROOT_WINDOW(), 0, 0, 64, 24, 0, 0, 0);
  wm_hints = XAllocWMHints();
  wm_hints->initial_state = WithdrawnState;
  wm_hints->icon_window = dockapp;
  wm_hints->window_group = dockapp;
  wm_hints->flags = StateHint | IconWindowHint;
  XSetWMHints(GDK_DISPLAY(), dockapp, wm_hints);
  XFree(wm_hints);
  XSetCommand(GDK_DISPLAY(), dockapp, argv, argc);

  gdkdockapp = gdk_window_foreign_new(dockapp);

  dockchild.w = gtk_plug_new(0);
  gtk_widget_add_events(dockchild.w, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  g_signal_connect(dockchild.w, "button-release-event", G_CALLBACK(handle_dock_event), NULL);
  g_object_set(dockchild.w, "has-tooltip", TRUE, NULL);
  g_signal_connect(dockchild.w, "query-tooltip", G_CALLBACK(query_tooltip), NULL);

  gtk_window_set_default_size(dockchild.d, 64, 24);

  gtk_widget_realize(dockchild.w);

  pixmap[IDLE]  = gdk_pixmap_create_from_xpm_d(dockchild.w->window, &bitmap[IDLE],  NULL, idle_xpm);
  pixmap[BREAK] = gdk_pixmap_create_from_xpm_d(dockchild.w->window, &bitmap[BREAK], NULL, break_xpm);

  image.w = gtk_image_new();
  set_icon_state(IDLE);
  gtk_container_add(dockchild.c, image.w);

  gtk_widget_show_all(dockchild.w);

  gdk_window_reparent(dockchild.w->window, gdkdockapp, 0, 0);
  gdk_window_show_unraised(dockchild.w->window);
  /* can't use gdk for this final map, or it will clear my carefully set dockapp hints */
  XMapWindow(GDK_DISPLAY(), dockapp);
}

int main(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

  create_icon(argc, argv);

  reset_break();

  gtk_main();

  return 0;
}

