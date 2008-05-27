#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <sys/utsname.h>
#include <string.h>
#include <stdlib.h>

/* nicer gtk interface */
#include "gtkunion.h"

#include "dockapp.h"

/* dockapp stuff */
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include "pixmaps/alert.xpm"
#include "pixmaps/idle.xpm"

static Plug dockchild;
static GdkWindow *gdkdockapp;
static Image image;
static GdkPixmap *pixmap[2];
static GdkBitmap *bitmap[2];

void set_icon_alert(int on)
{
  gtk_image_set_from_pixmap(image.i, pixmap[on], NULL);
  gdk_window_shape_combine_mask(gdkdockapp, bitmap[on], 0, 0);
}

static gboolean check_actions(gpointer nodata)
{
  set_icon_alert(1);
  /* Don't repeat timeout */
  return FALSE;
}

static gboolean handle_dock_event(Plug dockchild, GdkEventButton *event, gpointer nodata)
{
  if (event->button == 1) {
    set_icon_alert(0);
    g_timeout_add_seconds(300, (GSourceFunc)check_actions, NULL);
    return TRUE;
  }
  return FALSE;
}

/* I'm sorry if this code offends anyone :) */
void create_icon(int argc, char *argv[])
{
  Window dockapp;
  XWMHints *wm_hints;

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

  pixmap[0] = gdk_pixmap_create_from_xpm_d(dockchild.w->window, &bitmap[0], NULL, idle_xpm);
  pixmap[1] = gdk_pixmap_create_from_xpm_d(dockchild.w->window, &bitmap[1], NULL, alert_xpm);

  image.w = gtk_image_new();
  set_icon_alert(FALSE);
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

  check_actions(NULL);

  gtk_main();

  return 0;
}

