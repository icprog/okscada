/* -*-c++-*-  vi: set ts=4 sw=4 :

  (C) Copyright 2006-2007, vitki.net. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  $Date$
  $Revision$
  $Source$

  Simple visualizer for GTK+.

*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "see-priv.h"

#if defined(HAVE_CONFIG_H)
#include <config.h>			/* for SOLARIS */
#endif /* HAVE_CONFIG_H */

#if defined(SOLARIS)
/* OpenWindows crashes when GTK+ attempts to change window icon. */
#define USE_WINDOW_ICON		0
#else
#define USE_WINDOW_ICON		1
#endif /* SOLARIS */


GtkWidget *osee_window = 0;
GtkWidget *osee_filesel_open = 0;
GtkWidget *osee_filesel_save = 0;
GtkWidget *command_entry = 0;

GtkCList *osee_table = 0;
GtkEntry *osee_entry = 0;
GtkCheckMenuItem *osee_hex_check = 0;
GtkToggleButton *osee_hex_btn = 0;
GtkToggleButton *osee_log_btn = 0;
GtkStatusbar *osee_statusbar = 0;
GtkWidget *osee_alive_bar = 0;
GtkWidget *osee_alive_combo = 0;

void
oseeConfirmActivate(GtkMenuItem * menuitem, gpointer user_data)
{
	bool_t  active = GTK_CHECK_MENU_ITEM(menuitem)->active;

	oseeSetDeletionConfirmation(active);
}

void
oseeHex1Activate(GtkMenuItem * menuitem, gpointer user_data)
{
	bool_t  active = GTK_CHECK_MENU_ITEM(menuitem)->active;

	oseeSetCurHex(active);
}

void
oseeHex2Activate(GtkMenuItem * menuitem, gpointer user_data)
{
	bool_t  active = GTK_CHECK_MENU_ITEM(menuitem)->active;

	oseeSetDefHex(active);
}

void
oseeDebugActivate(GtkMenuItem * menuitem, gpointer user_data)
{
	bool_t  active = GTK_CHECK_MENU_ITEM(menuitem)->active;

	oseeSetDebug(active);
}

void
oseeLoggingActivate(GtkMenuItem * menuitem, gpointer user_data)
{
	bool_t  active = GTK_CHECK_MENU_ITEM(menuitem)->active;

	oseeSetLogging(active);
}

GtkWidget *
oseeSetupWidget(GtkWidget * window, GtkWidget * widget,
				 const char *widget_name)
{
	gtk_widget_set_name(widget, widget_name);
	gtk_widget_ref(widget);
	gtk_object_set_data_full(GTK_OBJECT(window), widget_name, widget,
							 (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show(widget);
	return widget;
}

GtkWidget *
oseeAddSubmenu(GtkWidget * window, GtkWidget * menubar,
				const char *desc, const char *widget_name)
{
	GtkWidget *item, *submenu;

	item = gtk_menu_item_new_with_label(desc);
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	oseeSetupWidget(window, item, widget_name);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), item);
	return submenu;
}

GtkWidget *
oseeMenuAdd(GtkWidget * window, GtkWidget * menu,
			 bool_t is_toggle,
			 const char *label, const char *widget_name, void *func, void *data)
{
	GtkWidget *item;

	if (is_toggle)
		item = gtk_check_menu_item_new_with_label(label);
	else
		item = gtk_menu_item_new_with_label(label);
	gtk_menu_append(GTK_MENU(menu), item);
	if (func)
		gtk_signal_connect(GTK_OBJECT(item),
						   (is_toggle ? "toggled" : "activate"),
						   GTK_SIGNAL_FUNC(func), data);
	oseeSetupWidget(window, item, widget_name);
	return item;
}

GtkWidget *
oseeAddToolbarIcon(GtkWidget * window,
					GtkWidget * toolbar,
					bool_t is_toggle,
					const char *icon_name,
					const char *widget_name, void *func, void *data)
{
	GtkWidget *icon, *widget;

	icon = oseeCreatePixmap(window, icon_name);
	widget = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
										(is_toggle ?
										 GTK_TOOLBAR_CHILD_TOGGLEBUTTON
										 : GTK_TOOLBAR_CHILD_BUTTON),
										NULL,
										icon_name,
										NULL, NULL, icon, NULL, NULL);
	oseeSetupWidget(window, widget, widget_name);
	if (func)
		gtk_signal_connect(GTK_OBJECT(widget),
						   (is_toggle ? "toggled" : "clicked"),
						   GTK_SIGNAL_FUNC(func), data);
	return widget;
}


gboolean
oseeTableKeyHandler(GtkWidget * widget, GdkEventKey * event,
					 gpointer user_data)
{
	GtkCList *tabview1 = GTK_CLIST(widget);

	if (event->keyval == GDK_Delete) {
		gtk_clist_select_row(tabview1, tabview1->focus_row, 0);
		oseeRemove();
		return TRUE;
	}
	if ((event->keyval == GDK_h || event->keyval == GDK_x)
		&& (event->state & GDK_CONTROL_MASK) != 0) {
		gtk_clist_select_row(tabview1, tabview1->focus_row, 0);
		oseeToggleCurHex();
		return TRUE;
	}
	if (event->keyval == GDK_d && (event->state & GDK_CONTROL_MASK) != 0) {
		gtk_clist_select_row(tabview1, tabview1->focus_row, 0);
		oseeRemove();
		return TRUE;
	}
	if (event->keyval == GDK_u && (event->state & GDK_CONTROL_MASK) != 0) {
		oseeUnselectAll();
		return TRUE;
	}
	return FALSE;
}

void
oseeHexToggled(GtkToggleButton * togglebutton, gpointer user_data)
{
	bool_t  active = gtk_toggle_button_get_active(togglebutton);

	oseeSetCurHex(active);
}

GtkWidget *
oseeCreateMainWindow(void)
{
	GtkWidget *window1;
	GtkWidget *vbox1;
	GtkWidget *menubar1, *menu;
	GtkWidget *toolbar2;
	GtkWidget *tb_start;
	GtkWidget *tb_stop;
	GtkWidget *tb_log;
	GtkWidget *tb_remove;
	GtkWidget *tb_hex;
	GtkWidget *tb_clear;
	GtkWidget *tb_open;
	GtkWidget *tb_save;
	GtkWidget *tb_exit;
	GtkWidget *notebook1;
	GtkWidget *scrolledwindow1;
	GtkWidget *scrolledwindow2;
	GtkWidget *tabview1;
	GtkWidget *cvl_description;
	GtkWidget *cvl_value;
	GtkWidget *cvl_info;
	GtkWidget *tab_monitoring1;
	GtkWidget *hbox1;
	GtkWidget *command_label;
	GtkWidget *command_button;
	GtkWidget *alive_bar;
	GtkWidget *alive_combo;
	GtkWidget *statusbar1;
	GtkWidget *item;
	GdkPixmap *gdk_pixmap;
	GdkBitmap *gdk_bitmap;

	osee_window = window1 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_show(window1);
	gtk_widget_set_name(window1, "window1");
	gtk_object_set_data(GTK_OBJECT(window1), "window1", window1);
	gtk_window_set_title(GTK_WINDOW(window1), "Optikus See GTK+");
	gtk_pixmap_get(GTK_PIXMAP(oseeCreatePixmap(window1, "see")),
				   &gdk_pixmap, &gdk_bitmap);
#if USE_WINDOW_ICON
	gdk_window_set_icon(window1->window, window1->window, gdk_pixmap,
						gdk_bitmap);
#endif /* USE_WINDOW_ICON */
	vbox1 = gtk_vbox_new(FALSE, 0);
	oseeSetupWidget(window1, vbox1, "vbox1");
	gtk_container_add(GTK_CONTAINER(window1), vbox1);

	menubar1 = gtk_menu_bar_new();
	oseeSetupWidget(window1, menubar1, "menubar1");
	gtk_box_pack_start(GTK_BOX(vbox1), menubar1, FALSE, FALSE, 0);

	menu = oseeAddSubmenu(window1, menubar1, "File", "menu_file");
	item =
		oseeMenuAdd(window1, menu, FALSE, "New", "create1", oseeClear, NULL);
	item = oseeMenuAdd(window1, menu, FALSE, "Open", "open1", oseeOpen, NULL);
	item = oseeMenuAdd(window1, menu, FALSE, "Save", "save1", oseeSave, NULL);
	item =
		oseeMenuAdd(window1, menu, FALSE, "Save As...", "save_as1", oseeSave,
					 NULL);
	item =
		oseeMenuAdd(window1, menu, FALSE, "Exit", "exit1", oseeExit,
					 (gpointer) TRUE);
	menu = oseeAddSubmenu(window1, menubar1, "Edit", "menu_edit");
	item = oseeMenuAdd(window1, menu, FALSE, "Cut", "cut1", oseeRemove, NULL);
	item =
		oseeMenuAdd(window1, menu, FALSE, "Paste", "paste1", oseeAdd, NULL);
	item =
		oseeMenuAdd(window1, menu, FALSE, "Delete", "delete1", oseeRemove,
					 NULL);
	menu = oseeAddSubmenu(window1, menubar1, "View", "menu_view");
	item =
		oseeMenuAdd(window1, menu, TRUE, "Confirm deletions", "confirm1",
					 oseeConfirmActivate, NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	item =
		oseeMenuAdd(window1, menu, TRUE, "Hexadecimal", "hex1",
					 oseeHex1Activate, NULL);
	osee_hex_check = (GtkCheckMenuItem *) item;
	item =
		oseeMenuAdd(window1, menu, TRUE, "Default hexadecimal", "hex2",
					 oseeHex2Activate, NULL);
	item =
		oseeMenuAdd(window1, menu, TRUE, "Debugging", "debug1",
					 oseeDebugActivate, NULL);
	menu = oseeAddSubmenu(window1, menubar1, "Help", "menu_help");
	item =
		oseeMenuAdd(window1, menu, FALSE, "About", "about1", oseeAbout, NULL);

	toolbar2 = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	oseeSetupWidget(window1, toolbar2, "toolbar2");
	gtk_box_pack_start(GTK_BOX(vbox1), toolbar2, FALSE, FALSE, 0);

	tb_start =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "start", "tb_start",
							oseeStart, NULL);
	tb_stop =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "stop", "tb_stop",
							oseeStop, NULL);
	tb_log =
		oseeAddToolbarIcon(window1, toolbar2, TRUE, "log", "tb_log",
							oseeToggleLogging, NULL);
	osee_log_btn = (GtkToggleButton *) tb_log;
	tb_remove =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "remove", "tb_remove",
							oseeRemove, NULL);
	tb_hex =
		oseeAddToolbarIcon(window1, toolbar2, TRUE, "hex", "tb_hex",
							oseeHexToggled, NULL);
	osee_hex_btn = (GtkToggleButton *) tb_hex;
	tb_clear =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "clear", "tb_clear",
							oseeClear, NULL);
	tb_open =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "open", "tb_open",
							oseeOpen, NULL);
	tb_save =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "save", "tb_save",
							oseeSave, NULL);
	tb_exit =
		oseeAddToolbarIcon(window1, toolbar2, FALSE, "exit", "tb_exit",
							oseeExit, (gpointer) TRUE);

	notebook1 = gtk_notebook_new();
	oseeSetupWidget(window1, notebook1, "notebook1");
	gtk_box_pack_start(GTK_BOX(vbox1), notebook1, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	oseeSetupWidget(window1, scrolledwindow1, "scrolledwindow1");
	gtk_container_add(GTK_CONTAINER(notebook1), scrolledwindow1);

	tabview1 = gtk_clist_new(3);
	osee_table = (GtkCList *) tabview1;
	oseeSetupWidget(window1, tabview1, "tabview1");
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), tabview1);
	gtk_clist_set_column_width(GTK_CLIST(tabview1), 0, 180);
	gtk_clist_set_column_width(GTK_CLIST(tabview1), 1, 180);
	gtk_clist_set_column_width(GTK_CLIST(tabview1), 2, 40);
	gtk_clist_column_titles_show(GTK_CLIST(tabview1));
	gtk_signal_connect(GTK_OBJECT(tabview1), "key_press_event",
					   GTK_SIGNAL_FUNC(oseeTableKeyHandler), NULL);

	cvl_description = gtk_label_new("Description");
	oseeSetupWidget(window1, cvl_description, "cvl_description");
	gtk_clist_set_column_widget(GTK_CLIST(tabview1), 0, cvl_description);

	cvl_value = gtk_label_new("Value");
	oseeSetupWidget(window1, cvl_value, "cvl_value");
	gtk_clist_set_column_widget(GTK_CLIST(tabview1), 1, cvl_value);

	cvl_info = gtk_label_new("Info");
	oseeSetupWidget(window1, cvl_info, "cvl_info");
	gtk_clist_set_column_widget(GTK_CLIST(tabview1), 2, cvl_info);

	tab_monitoring1 = gtk_label_new("monitoring");
	oseeSetupWidget(window1, tab_monitoring1, "tab_monitoring1");
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook1),
							   gtk_notebook_get_nth_page(GTK_NOTEBOOK
														 (notebook1), 0),
							   tab_monitoring1);
	gtk_label_set_justify(GTK_LABEL(tab_monitoring1), GTK_JUSTIFY_LEFT);

	hbox1 = gtk_hbox_new(FALSE, 0);
	oseeSetupWidget(window1, hbox1, "hbox1");
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

	command_label = gtk_label_new("Command:");
	oseeSetupWidget(window1, command_label, "command_label");
	gtk_box_pack_start(GTK_BOX(hbox1), command_label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(command_label), GTK_JUSTIFY_RIGHT);

	command_entry = gtk_entry_new();
	osee_entry = (GtkEntry *) command_entry;
	oseeSetupWidget(window1, command_entry, "command_entry");
	gtk_box_pack_start(GTK_BOX(hbox1), command_entry, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(command_entry),
					   "activate", GTK_SIGNAL_FUNC(oseeAdd), NULL);

	command_button = oseeCreateButton(window1, "ok", 0, oseeAdd, NULL);
	oseeSetupWidget(window1, command_button, "command_button");
	gtk_box_pack_start(GTK_BOX(hbox1), command_button, FALSE, FALSE, 0);

	scrolledwindow2 = gtk_scrolled_window_new(NULL, NULL);
	oseeSetupWidget(window1, scrolledwindow2, "scrolledwindow2");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow2),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow2, FALSE, FALSE, 0);

	alive_bar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	osee_alive_bar = (GtkWidget *) alive_bar;
	oseeSetupWidget(window1, alive_bar, "alive_bar");
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwindow2),
										  alive_bar);

	alive_combo =
		oseeAddToolbarIcon(window1, alive_bar, FALSE, "not", "alive_combo",
							NULL, NULL);
	osee_alive_combo = (GtkWidget *) alive_combo;

	statusbar1 = gtk_statusbar_new();
	osee_statusbar = (GtkStatusbar *) statusbar1;
	oseeSetupWidget(window1, statusbar1, "statusbar1");
	gtk_box_pack_start(GTK_BOX(vbox1), statusbar1, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(statusbar1), 1);

	return window1;
}

GtkWidget *
oseeLookupWidget(const char *widget_name)
{
	GtkWidget *widget = osee_window;
	GtkWidget *parent, *found_widget;

	for (;;) {
		if (GTK_IS_MENU(widget))
			parent = gtk_menu_get_attach_widget(GTK_MENU(widget));
		else
			parent = widget->parent;
		if (parent == NULL)
			break;
		widget = parent;
	}
	found_widget = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(widget),
													 widget_name);
	if (!found_widget)
		printf("widget not found [%s]\n", widget_name);
	return found_widget;
}

GtkWidget *
oseeCreateOrChangePixmap(GtkWidget * widget,
						  GtkWidget * pixmap, const gchar * name)
{
	GdkColormap *colormap;
	GdkPixmap *gdkpixmap;
	GdkBitmap *mask;
	int     i;
	char  **xpm_data = NULL;

	if (!name)
		return NULL;
	for (i = 0; osee_icons[i].name; i++)
		if (!strcmp(osee_icons[i].name, name))
			break;
	if (!osee_icons[i].name) {
		printf("couldn't create pixmap \"%s\"\n", name);
		exit(1);
	}
	xpm_data = osee_icons[i].data;
	colormap = gtk_widget_get_colormap(widget);
	gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL, colormap, &mask,
													  NULL, xpm_data);
	if (pixmap)
		gtk_pixmap_set(GTK_PIXMAP(pixmap), gdkpixmap, mask);
	else
		pixmap = gtk_pixmap_new(gdkpixmap, mask);
	gdk_pixmap_unref(gdkpixmap);
	gdk_bitmap_unref(mask);
	return pixmap;
}

GtkWidget *
oseeCreatePixmap(GtkWidget * widget, const gchar * name)
{
	return oseeCreateOrChangePixmap(widget, NULL, name);
}

GtkWidget *
oseeCreateButton(GtkWidget * window,
				  const char *icon, const char *text, void *func, void *param)
{
	GtkWidget *button, *hbox, *pixmap, *label;

	button = gtk_button_new();
	gtk_widget_show(button);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(button), hbox);
	if (icon) {
		pixmap = oseeCreatePixmap(window, icon);
		gtk_widget_show(pixmap);
		gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 2);
	}
	if (text) {
		label = gtk_label_new(text);
		gtk_widget_show(label);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	}
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
					   GTK_SIGNAL_FUNC(func), param);
	return button;
}

oret_t
oseeChangeButton(GtkWidget * button, const char *icon, const char *text)
{
	GtkWidget *hbox, *pixmap, *label;
	GList  *children;

	hbox = GTK_BIN(button)->child;
	children = GTK_BOX(hbox)->children;
	if (icon) {
		pixmap = ((GtkBoxChild *) (children->data))->widget;
		oseeCreateOrChangePixmap(pixmap, pixmap, icon);
	}
	if (text) {
		label = ((GtkBoxChild *) (children->next->data))->widget;
		gtk_label_set_text(GTK_LABEL(label), text);
	}
	return OK;
}

#define OSEE_DIA_BUTTONS  10

typedef struct
{
	int     button_no;
	GtkWidget *buttons[OSEE_DIA_BUTTONS];
} OseeDia_t;

static void
oseeDialogClicked(GtkWidget * button, OseeDia_t * dia)
{
	int     i;

	dia->button_no = 0;
	for (i = 0; i < OSEE_DIA_BUTTONS; i++) {
		if (button == dia->buttons[i]) {
			dia->button_no = i + 1;
			break;
		}
	}
	gtk_main_quit();
}

int
oseeRunDialog(const char *icon, const char *message,
			   int button_num, const char *button_icons[],
			   const char *button_labels[])
{
	GtkWidget *dialog, *box1, *box2, *pixmap, *label, *button;
	int     i;
	OseeDia_t dia = { 0 };
	dialog = gtk_dialog_new();

	box1 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box1);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), box1);

	box2 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box2);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dialog)->action_area),
					 box2, FALSE, FALSE, 0);

	pixmap = oseeCreatePixmap(dialog, icon);
	if (pixmap)
		gtk_box_pack_start(GTK_BOX(box1), pixmap, FALSE, FALSE, 0);
	label = gtk_label_new(message);
	if (label)
		gtk_box_pack_end(GTK_BOX(box1), label, FALSE, FALSE, 0);
	for (i = 0; i < button_num; i++) {
		button = oseeCreateButton(dialog,
								   button_icons ? button_icons[i] : 0,
								   button_labels ? button_labels[i] : 0,
								   oseeDialogClicked, &dia);
		dia.buttons[i] = button;
		gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	}
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_widget_show_all(dialog);
	gtk_main();
	gtk_widget_destroy(dialog);
	return dia.button_no;
}

bool_t
oseeQuestion(const char *question)
{
	static const char *icons[2] = { "apply", "cancel" };
	static const char *labels[2] = { "Yes", "No" };
	int     answer = oseeRunDialog("question", question, 2, icons, labels);

	return answer == 1;
}

GtkWidget *
oseeCreateFileSelection(const char *title, void *func)
{
	GtkWidget *fs, *ok, *cancel;
	GtkWindow *window;

	fs = gtk_file_selection_new(title);
	gtk_object_set_data(GTK_OBJECT(fs), "fileselection1", fs);
	gtk_container_set_border_width(GTK_CONTAINER(fs), 10);

	ok = GTK_FILE_SELECTION(fs)->ok_button;
	gtk_object_set_data(GTK_OBJECT(fs), "ok", ok);
	gtk_widget_show(ok);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(func), fs);

	cancel = GTK_FILE_SELECTION(fs)->cancel_button;
	gtk_object_set_data(GTK_OBJECT(fs), "cancel", cancel);
	gtk_widget_show(cancel);
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);

	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
							  GTK_SIGNAL_FUNC(gtk_widget_destroy),
							  GTK_OBJECT(fs));
	gtk_widget_show(fs);
	//window = GTK_WINDOW (GTK_FILE_SELECTION(fs)->fileop_dialog);
	window = GTK_WINDOW(fs);
	gtk_window_set_position(window, GTK_WIN_POS_MOUSE);
	return fs;
}


oret_t
oseeSetStatus(const char *format, ...)
{
	char    buf[200];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	if (!osee_statusbar || !osee_window)
		return ERROR;
	gtk_statusbar_pop(osee_statusbar, 1);
	gtk_statusbar_push(osee_statusbar, 2, buf);
	return OK;
}


int
main(int argc, char *argv[])
{
	extern char osee_url[];
	int     i;

	if (argc >= 2 && !strcmp(argv[1], "-h")) {
		strcpy(osee_url, argv[2]);
		printf("connecting to %s.\n", osee_url);
		for (i = 2; i < argc; i++)
			argv[i - 1] = argv[i];
		argc--;
	}
	gtk_init(&argc, &argv);
	osee_window = oseeCreateMainWindow();
	gtk_widget_hide(osee_window);
	gtk_window_set_position(GTK_WINDOW(osee_window), GTK_WIN_POS_CENTER);
	gtk_signal_connect(GTK_OBJECT(osee_window), "destroy",
					   GTK_SIGNAL_FUNC(oseeExit), (gpointer) FALSE);
	gtk_widget_show(osee_window);
	gtk_window_set_default_size(GTK_WINDOW(osee_window), 300, 360);
	oseeInit();
	gtk_main();
	return 0;
}
