#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <sys/utsname.h>
#include <string.h>
#include <stdlib.h>

#include "gtkunion.h"

void cell_edited(GtkCellRendererText *cell, const gchar *path_string,
                 gchar *new_text, gpointer data)
{}

Treeviewcolumn new_column(const gchar *name, Liststore store)
{
  Treeviewcolumn column;
  Cellrenderer renderer;

  renderer.r = gtk_cell_renderer_text_new();
  g_object_set(renderer.o, "editable", TRUE, NULL);
  g_object_set_data(renderer.o, "column", GINT_TO_POINTER(0));
  g_signal_connect(renderer.o, "edited", G_CALLBACK(cell_edited), store.t);

  column.c = gtk_tree_view_column_new_with_attributes(name, renderer.r, "text", 0, NULL);
  g_object_set(column.o, "resizable", TRUE,
                         "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
                         "min-width", 50,
                         NULL);

  return column;
}

GtkWidget *create_settings()
{
  Vbox vbox;
  Scrolledwindow scroll;
  Treeview treeview;
  Liststore liststore;

  liststore.l = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

  treeview.w = gtk_tree_view_new_with_model(liststore.t);
  gtk_tree_view_set_rules_hint(treeview.t, TRUE);
  gtk_tree_view_set_headers_visible(treeview.t, TRUE);  

  gtk_tree_view_insert_column(treeview.t, new_column("Task", liststore).c, -1);
  gtk_tree_view_insert_column(treeview.t, new_column("Interval", liststore).c, -1);

  vbox.w = gtk_vbox_new(FALSE, 5);
  scroll.w = gtk_scrolled_window_new(NULL, NULL);

  gtk_scrolled_window_set_shadow_type(scroll.s, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(scroll.s,
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  gtk_container_add(scroll.c, treeview.w);
  gtk_box_pack_start(vbox.b, scroll.w, TRUE, TRUE, 0);

  return vbox.w;
}

Window create_dialog()
{
  Window dialog;

  dialog.d = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(dialog.w, "Reminder");
  g_signal_connect(dialog.d, "delete-event", G_CALLBACK(exit /* Some function that asks for saving any changes before closing */), 0);

  gtk_container_add(dialog.c, create_settings());

  return dialog;
}

int main(int argc, char *argv[])
{
  Window dialog;

  gtk_init(&argc, &argv);

  dialog = create_dialog();

  gtk_widget_show_all(dialog.d);
  gtk_main();

  return 0;
}

