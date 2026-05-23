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

static void set_icon_state(Break on)
{
  breaktime = on;
  gtk_image_set_from_pixmap(image.i, pixmap[on], NULL);
  gdk_window_shape_combine_mask(gdkdockapp, bitmap[on], 0, 0);
}

static gboolean break_timeout(gpointer nodata)
{
  set_icon_state(BREAK);
  /* Don't repeat timeout */
  return FALSE;
}

static void reset_break()
{
  if (breaktime == BREAK)
    set_icon_state(IDLE);
  while (g_source_remove_by_user_data(&gdkdockapp));
  g_timeout_add_seconds(delay, (GSourceFunc)break_timeout, &gdkdockapp);
}

static gboolean set_delay(Gtkwindow dialog, GdkEvent *event, Entry entry)
{
  delay = atoi(gtk_entry_get_text(entry.e));
  gtk_widget_hide(dialog.w);
  reset_break();
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

