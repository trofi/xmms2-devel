#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

#include "mlibupdater.h"

static void
quit (void *data)
{
	GMainLoop *ml = data;
	ERR ("xmms2d disconnected us");
	g_main_loop_quit (ml);
}

static void
handle_file_add (xmonitor_t *mon, gchar *filename)
{
	if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		monitor_add_dir (mon, filename);
		mon->dir_list = g_list_append (mon->dir_list, filename);
		DBG ("New directory: %s", filename);
	} else if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		gchar tmp[MON_FILENAME_MAX];
		g_snprintf (tmp, MON_FILENAME_MAX, "file://%s", filename);
		xmmsc_medialib_add_entry (mon->conn, tmp);
		DBG ("Adding %s to medialib!", tmp);
	}
}

static void
handle_remove_from_mlib (xmmsc_result_t *res, void *userdata)
{
	xmonitor_t *mon = userdata;
	xmmsc_result_t *res2;

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		guint32 id;
		if (!xmmsc_result_get_dict_entry_uint32 (res, "id", &id)) {
			ERR ("Failed to get entry id from hash!");
			continue;
		}

		if (id == 0) {
			DBG ("Entry not in db!");
			continue;
		}

		DBG ("Removing %d", id);

		res2 = xmmsc_medialib_remove_entry (mon->conn, id);
		xmmsc_result_unref (res2);
	}
	xmmsc_result_unref (res);
}

static void
handle_file_del (xmonitor_t *mon, gchar *filename)
{
	xmmsc_result_t *res;
	gchar tmp[MON_FILENAME_MAX];
	gchar *query;

	g_snprintf (tmp, MON_FILENAME_MAX, "file://%s", filename);

	query = g_strdup_printf ("SELECT id FROM Media WHERE key='url' AND value LIKE '%s%%'", tmp);
	DBG ("running %s", query);
	res = xmmsc_medialib_select (mon->conn, query);
	g_free (query);
	xmmsc_result_notifier_set (res, handle_remove_from_mlib, mon);
	xmmsc_result_unref (res);
}

static void
handle_mlib_update (xmmsc_result_t *res, void *userdata)
{
	xmonitor_t *mon = userdata;
	xmmsc_result_t *res2;
	guint32 id;

	if (!xmmsc_result_get_uint (res, &id)) {
		ERR ("Failed to get id for entry!");
		xmmsc_result_unref (res);
		return;
	}

	if (id == 0) {
		DBG ("Entry not in db!");
		return;
	}

	res2 = xmmsc_medialib_rehash (mon->conn, id);
	xmmsc_result_unref (res2);
	xmmsc_result_unref (res);
}

static void
handle_file_changed (xmonitor_t *mon, gchar *filename)
{
	xmmsc_result_t *res;
	gchar tmp[MON_FILENAME_MAX];

	g_snprintf (tmp, MON_FILENAME_MAX, "file://%s", filename);

	res = xmmsc_medialib_get_id (mon->conn, tmp);
	xmmsc_result_notifier_set (res, handle_mlib_update, mon);
	xmmsc_result_unref (res);
	DBG ("update file in medialib");
}

gboolean
s_callback (GIOChannel *s, GIOCondition cond, gpointer data)
{
	xmonitor_t *mon = data;
	xevent_t event;

	while (monitor_process (mon, &event)) {
		switch (event.code) {
			case MON_DIR_CHANGED:
				DBG ("got changed signal for %s", event.filename);
				handle_file_changed (mon, event.filename);
				break;
			case MON_DIR_DELETED:
				handle_file_del (mon, event.filename);
				break;
			case MON_DIR_CREATED:
				handle_file_add (mon, event.filename);
				break;
			default:
				break;
		}
	}

	return TRUE;
}

static void
process_dir (xmonitor_t *mon, gchar *dirpath)
{
	GDir *dir;
	const gchar *tmp;
	gchar *path;

	dir = g_dir_open (dirpath, 0, NULL);
	if (!dir) {
		ERR ("Error when opening %s", dirpath);
		return;
	}

	while ((tmp = g_dir_read_name (dir))) {
		path = g_strdup_printf ("%s/%s", dirpath, tmp);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			if (!monitor_add_dir (mon, path)) {
				ERR ("Couldn't watch directory: %s!", path);
				g_free (path);
				continue;
			}
			DBG ("Now watching dir %s", path);
			mon->dir_list = g_list_append (mon->dir_list, path);
			process_dir (mon, path);
		}
	}

	g_main_context_iteration (NULL, FALSE);
	
	g_dir_close (dir);
}

static void
handle_addpath (xmmsc_result_t *res, void *data)
{
	xmonitor_t *mon = data;

	if (!monitor_add_dir (mon, mon->watch_dir)) {
		ERR ("Couldn't watch directory!");
		return;
	}
	mon->dir_list = g_list_append (mon->dir_list, mon->watch_dir);

	process_dir (mon, mon->watch_dir);

	DBG ("Watching %d dirs", g_list_length (mon->dir_list));

	xmmsc_result_unref (res);
}

static void
do_watch_dir (xmonitor_t *mon, gchar *dirs)
{
	xmmsc_result_t *res;
	GList *n;

	DBG ("We are going to watch '%s'", dirs);

	for (n = mon->dir_list; n; n = g_list_next (n)) {
		monitor_del_dir (mon, n->data);
		g_free (n->data);
	}

	if (mon->dir_list) {
		g_list_free (mon->dir_list);
		mon->dir_list = NULL;
	}

	if (strlen (dirs) < 1) {
		mon->watch_dir = NULL;
		return;
	} else {
		mon->watch_dir = g_strdup (dirs);
	}

	/* Make sure that nothing changed while we where away! */
	res = xmmsc_medialib_path_import (mon->conn, mon->watch_dir);
	xmmsc_result_notifier_set (res, handle_addpath, mon);
	xmmsc_result_unref (res);
}

static void
handle_config_changed (xmmsc_result_t *res, void *data)
{
	xmonitor_t *mon = data;
	gchar *key;
	gchar *val;

	
	if (!xmmsc_result_get_string (res, &key)) {
		ERR ("Config changed has invalid result!");
		return;
	}

	xmmsc_result_list_next (res);

	if (!xmmsc_result_get_string (res, &val)) {
		ERR ("Config changed has invalid result!");
		return;
	}

	if (g_strcasecmp (key, "clients.mlibupdater.watch_dirs") == 0) {
		do_watch_dir (mon, val);
	}

}

static void
handle_watch_dirs (xmmsc_result_t *res, void *data)
{
	xmonitor_t *mon = data;
	gchar *dirs;

	if (!xmmsc_result_get_string (res, &dirs)) {
		ERR ("Couldn't get configvalue from server!");
		return;
	}

	do_watch_dir (mon, dirs);

	xmmsc_result_unref (res);

}

static void
handle_configval (xmmsc_result_t *res, void *data)
{
	xmmsc_result_t *res2;
	xmonitor_t *mon = data;
	gchar *val;

	if (!xmmsc_result_get_string (res, &val)) {
		ERR ("Couldn't register value in server!");
		return;
	}

	res2 = xmmsc_configval_get (mon->conn, val);
	xmmsc_result_notifier_set (res2, handle_watch_dirs, mon);
	xmmsc_result_unref (res2);
	xmmsc_result_unref (res);
}

static void
message_handler (const gchar *log_domain, GLogLevelFlags log_level,
		 const gchar *message, gpointer user_data)
{

}

int 
main (int argc, char **argv)
{
	GIOChannel *gio;
	GMainLoop *ml;
	gchar *path;
	gchar *tmp;
	xmmsc_connection_t *conn;
	xmmsc_result_t *res;
	xmonitor_t *mon;
	gint fd;

	conn = xmmsc_init ("XMMS MLib Updater " VERSION);
	path = getenv ("XMMS_PATH");
	if (!xmmsc_connect (conn, path)) {
		ERR ("Could not connect to xmms2d %s", xmmsc_get_last_error (conn));
		return EXIT_FAILURE;
	}

	ml = g_main_loop_new (NULL, FALSE);
	xmmsc_setup_with_gmain (conn);
	xmmsc_disconnect_callback_set (conn, quit, ml);

	mon = g_new0 (xmonitor_t, 1);
	fd = monitor_init (mon);
	mon->conn = conn;

	if (fd == -1) {
		ERR ("Couldn't initalize monitor");
		return EXIT_FAILURE;
	}


	tmp = getenv("XMMS_DEBUG");
	if (!tmp) {
		g_log_set_handler (NULL, G_LOG_LEVEL_MESSAGE | G_LOG_FLAG_RECURSION, 
				   message_handler, NULL);
	}

	gio = g_io_channel_unix_new (fd);
	g_io_add_watch (gio, G_IO_IN, s_callback, mon);

	res = xmmsc_configval_register (conn, "mlibupdater.watch_dirs", "");
	xmmsc_result_notifier_set (res, handle_configval, mon);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_configval_changed (conn);
	xmmsc_result_notifier_set (res, handle_config_changed, mon);
	xmmsc_result_unref (res);

	g_main_loop_run (ml);

	return EXIT_SUCCESS;
}

