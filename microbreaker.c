#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>

/* nicer gtk interface */
#include "gtkunion.h"

/* dockapp stuff */
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include "pixmaps/break.xpm"
#include "pixmaps/idle.xpm"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

typedef enum {
  IDLE,
  BREAK,
  NUMICONS
} Break;

static GdkWindow *gdkdockapp;
static Image image;
static GdkPixmap *pixmap[NUMICONS];
static GdkBitmap *bitmap[NUMICONS];
static Break breaktime;
#define DEFAULT_DELAY 300
#define DEFAULT_DELAY_STR TOSTRING(DEFAULT_DELAY)
static gint delay = DEFAULT_DELAY;
static guint timeout_id = 0;
static Label tooltip_label = { .l = NULL };
static gboolean tooltip_stale = TRUE;

static void set_icon_state(Break on)
{
  breaktime = on;
  gtk_tooltip_trigger_tooltip_query(gdk_display_get_default());
  gtk_image_set_from_pixmap(image.i, pixmap[on], NULL);
  gdk_window_shape_combine_mask(gdkdockapp, bitmap[on], 0, 0);
}

static gboolean break_timeout(gpointer nodata)
{
  timeout_id = 0;
  set_icon_state(BREAK);
  tooltip_stale = TRUE;
  /* Don't repeat timeout */
  return FALSE;
}

static void reset_break()
{
  g_clear_handle_id(&timeout_id, g_source_remove);
  timeout_id = g_timeout_add_seconds(delay, (GSourceFunc)break_timeout, &gdkdockapp);
  tooltip_stale = TRUE;
  if (breaktime == BREAK)
    set_icon_state(IDLE);
}

static void set_delay(Gtkwindow dialog, Entry entry)
{
  delay = atoi(gtk_entry_get_text(entry.e));
  gtk_widget_hide(dialog.w);
  reset_break();
}

static gboolean dialog_delete_cb(Gtkwindow dialog, GdkEvent *event, Entry entry)
{
  set_delay(dialog, entry);
  return TRUE;
}

static void entry_activate_cb(Entry entry, Gtkwindow dialog)
{
  set_delay(dialog, entry);
}

static gboolean entry_keypress_cb(Entry entry, GdkEventKey *event, Gtkwindow dialog)
{
  if (event->keyval == GDK_Escape) {
    gtk_widget_hide(dialog.w);
    return TRUE;
  }
  return FALSE;
}

static gboolean query_tooltip(GtkWidget *widget, gint x, gint y,
                              gboolean keyboard_mode, GtkTooltip *tooltip,
                              gpointer nodata)
{
  /* custom widget is unparented by gtk_tooltip_window_hide when tooltip hides */
  if (tooltip_stale || !gtk_widget_get_parent(tooltip_label.w)) {
    tooltip_stale = FALSE;
    if (breaktime == BREAK) {
      gtk_label_set_text(tooltip_label.l, "Break time!");
    } else {
      GSource *src;
      if (timeout_id && (src = g_main_context_find_source_by_id(NULL, timeout_id))) {
        char text[32];
        gint64 remaining_us = g_source_get_ready_time(src) - g_get_monotonic_time();
        gint remaining_s = (gint)(remaining_us / G_USEC_PER_SEC);
        if (remaining_s < 0) remaining_s = 0;
        snprintf(text, sizeof(text), "%d:%02d until break", remaining_s / 60, remaining_s % 60);
        gtk_label_set_text(tooltip_label.l, text);
      } else {
        gtk_label_set_text(tooltip_label.l, "Break imminent");
      }
    }
  }

  gtk_tooltip_set_custom(tooltip, tooltip_label.w);
  return TRUE;
}

static gboolean handle_dock_event(Plug dockchild, GdkEventButton *event, gpointer nodata)
{
  if (event->button == 1) {
    reset_break();
    gtk_tooltip_trigger_tooltip_query(gdk_display_get_default());
    return TRUE;
  } else if (event->button == 3) {
    static Gtkwindow dialog = { NULL };

    if (!dialog.w) {
      Entry entry;
      dialog.w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title(dialog.d, "Microbreaker");
      gtk_window_set_position(dialog.d, GTK_WIN_POS_CENTER);

      entry.w = gtk_entry_new();
      gtk_entry_set_text(entry.e, DEFAULT_DELAY_STR);

      gtk_container_add(dialog.c, entry.w);

      g_signal_connect(dialog.o, "delete-event", G_CALLBACK(dialog_delete_cb), entry.w);
      g_signal_connect(entry.o, "activate", G_CALLBACK(entry_activate_cb), dialog.w);
      g_signal_connect(entry.o, "key-press-event", G_CALLBACK(entry_keypress_cb), dialog.w);
    }

    gtk_widget_show_all(dialog.w);
  }

  return FALSE;
}

/* I'm sorry if this code offends anyone :) */
static void create_icon(int argc, char *argv[])
{
  Window dockapp;
  static Plug dockchild;
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

  tooltip_label.w = gtk_label_new(NULL);
  g_object_ref_sink(tooltip_label.w);

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

