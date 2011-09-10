#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "dive.h"
#include "display.h"
#include "divelist.h"

static GtkWidget *cylinder_description;

static gboolean match_cylinder(GtkTreeModel *model,
				GtkTreePath *path,
				GtkTreeIter *iter,
				gpointer data)
{
	const char *name;
	const char *desc = data;
	GValue value = {0, };

	gtk_tree_model_get_value(model, iter, 0, &value);
	name = g_value_get_string(&value);
	if (strcmp(desc, name))
		return FALSE;
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(cylinder_description), iter);
	return TRUE;
}

void show_dive_equipment(struct dive *dive)
{
	const char *desc = dive->cylinder[0].type.description;
	GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(cylinder_description));

	if (!desc)
		return;
	gtk_tree_model_foreach(model, match_cylinder, (gpointer)desc);
}

static GtkWidget *create_spinbutton(GtkWidget *vbox, const char *name)
{
	GtkWidget *frame, *button;

	frame = gtk_frame_new(name);
	gtk_container_add(GTK_CONTAINER(vbox), frame);

	button = gtk_spin_button_new_with_range( 1.0, 3000, 0.1);
	gtk_container_add(GTK_CONTAINER(frame), button);

	return button;
}

static int get_cylinder_details(const char *name, int *volume, int *pressure)
{
	int result;
	GtkWidget *dialog, *frame, *vbox;

	dialog = gtk_dialog_new_with_buttons("New Cylinder Type",
		GTK_WINDOW(main_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		NULL);

	frame = gtk_frame_new(name);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), frame);
	vbox = gtk_vbox_new(TRUE, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	create_spinbutton(vbox, "Size:");
	create_spinbutton(vbox, "Working pressure:");

	gtk_widget_show_all(dialog);
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	*volume = 0;
	*pressure = 0;
	return result == GTK_RESPONSE_ACCEPT;
}

static void fill_cylinder_info(cylinder_t *cyl, const char *desc, int mliter, int mbar)
{
	/*
	 * The code is currently too broken to actually set anything,
	 * so just print what we would set
	 */
	printf("Set cylinder to '%s': %.1f liter at %.1f bar working pressure\n",
		desc, mliter / 1000.0, mbar / 1000.);
#if 0
	cyl->type.description = desc;
	cyl->type.size.mliter = mliter;
	cyl->type.workingpressure.mbar = mbar;
#endif
}

void flush_dive_equipment_changes(struct dive *dive)
{
	GtkTreeIter iter;
	const gchar *desc;
	GtkComboBox *box = GTK_COMBO_BOX(cylinder_description);
	GtkTreeModel *model = gtk_combo_box_get_model(box);
	GtkListStore *store = GTK_LIST_STORE(model);
	GValue value1 = {0, }, value2 = {0,}, value3 = {0, };
	int volume, pressure;

	if (!gtk_combo_box_get_active_iter(box, &iter)) {
		desc = gtk_combo_box_get_active_text(box);
		if (!get_cylinder_details(desc, &volume, &pressure))
			return;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			0, desc,
			1, volume,
			2, pressure,
			-1);
	}

	gtk_tree_model_get_value(model, &iter, 0, &value1);
	desc = g_value_get_string(&value1);
	gtk_tree_model_get_value(model, &iter, 1, &value2);
	volume = g_value_get_int(&value2);
	gtk_tree_model_get_value(model, &iter, 2, &value3);
	pressure = g_value_get_int(&value3);
	fill_cylinder_info(dive->cylinder+0, desc, volume, pressure);
}

/* We should take these from the dive list instead */
static struct tank_info {
	const char *name;
	int size;	/* cuft or mliter depending on psi */
	int psi;	/* If zero, size is in mliter */
} tank_info[] = {
	{ "None", 0, 0 },
	{ "10.0 l", 10000 },
	{ "11.1 l", 11100 },
	{ "AL72", 72, 3000 },
	{ "AL80", 80, 3000 },
	{ "LP85", 85, 2400 },
	{ "LP95", 95, 2400 },
	{ "LP85+", 85, 2640 },
	{ "LP95+", 95, 2640 },
	{ "HP100", 100, 3442 },
	{ "HP119", 119, 3442 },
	{ NULL, }
};

static void fill_tank_list(GtkListStore *store)
{
	GtkTreeIter iter;

	struct tank_info *info = tank_info;

	while (info->name) {
		int size = info->size;
		int psi = info->psi;
		int mbar = 0, ml = size;

		/* Is it in cuft and psi? */
		if (psi) {
			double bar = 0.0689475729 * psi;
			double airvolume = 28316.8466 * size;
			double atm = bar / 1.01325;

			ml = airvolume / atm + 0.5;
			mbar = bar*1000 + 0.5;
		}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			0, info->name,
			1, ml,
			2, mbar,
			-1);
		info++;
	}
}

static void cylinder_widget(GtkWidget *box, int nr, GtkListStore *model)
{
	GtkWidget *frame, *hbox, *size;
	char buffer[80];

	snprintf(buffer, sizeof(buffer), "Cylinder %d", nr);
	frame = gtk_frame_new(buffer);
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(TRUE, 3);
	gtk_container_add(GTK_CONTAINER(frame), hbox);

	size = gtk_combo_box_entry_new_with_model(GTK_TREE_MODEL(model), 0);
	gtk_box_pack_start(GTK_BOX(hbox), size, FALSE, FALSE, 0);

	cylinder_description = size;
}

static GtkListStore *create_tank_size_model(void)
{
	GtkListStore *model;

	model = gtk_list_store_new(3,
		G_TYPE_STRING,		/* Tank name */
		G_TYPE_INT,		/* Tank size in mliter */
		G_TYPE_INT,		/* Tank working pressure in mbar */
		-1);

	fill_tank_list(model);
	return model;
}

GtkWidget *equipment_widget(void)
{
	GtkWidget *vbox;
	GtkListStore *model;

	vbox = gtk_vbox_new(FALSE, 3);

	model = create_tank_size_model();
	cylinder_widget(vbox, 0, model);

	return vbox;
}