/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundstr�m, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-glib.h>
#include <xmms/signal_xmms.h>

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

typedef struct {
	char *name;
	char *help;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv);
} cmds;

/**
 * Utils
 */

void
print_error (const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	
	va_start (ap, fmt);
	vsnprintf (buf, 1024, fmt, ap);
	va_end (ap);

	printf ("ERROR: %s\n", buf);

	exit (-1);
}

void
print_info (const char *fmt, ...)
{
	char buf[8096];
	va_list ap;
	
	va_start (ap, fmt);
	vsnprintf (buf, 8096, fmt, ap);
	va_end (ap);

	printf ("%s\n", buf);
}

/**
 * here comes all the cmd callbacks
 */

static void
cmd_add (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;

	if (argc < 3) {
		print_error ("Need a filename to add");
	}

	for (i = 2; argv[i]; i++) {
		char url[4096];
		xmmsc_result_t *res;
		char rpath[PATH_MAX];
		char *p;

		p = strchr (argv[i], ':');
		if (!(p && p[1] == '/' && p[2] == '/')) {
			/* OK, so this is NOT an valid URL */

			if (!realpath (argv[i], rpath))
				continue;

			if (!g_file_test (rpath, G_FILE_TEST_IS_REGULAR))
				continue;
			
			g_snprintf (url, 4096, "file://%s", rpath);
		} else {
			g_snprintf (url, 4096, "%s", argv[i]);
		}
		
		res = xmmsc_playlist_add (conn, xmmsc_encode_path (url));
		xmmsc_result_wait (res);
		if (xmmsc_result_iserror (res)) {
			printf ("something went wrong when adding it to the playlist\n");
			exit (-1);
		}
		print_info ("Adding %s", url);
		xmmsc_result_unref (res);
	}
}

static void
cmd_clear (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_playlist_clear (conn);
	xmmsc_result_unref (res);

}

static void
cmd_shuffle (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	
	res = xmmsc_playlist_shuffle (conn);
	xmmsc_result_unref (res);
	
}

static void
cmd_remove (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Remove needs a ID to be removed");
	}

	for (i = 2; argv[i]; i++) {
		res = xmmsc_playlist_remove (conn, atoi (argv[i]));
		xmmsc_result_wait (res);
		if (xmmsc_result_iserror (res)) {
			fprintf (stderr, "Couldn't remove %d (%s)\n", atoi (argv[i]), xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	}
}


static void
cmd_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	x_list_t *list;
	x_list_t *l;
	GError *err = NULL;
	int id;
	int r, w;

	list = xmmscs_playlist_list (conn);

	if (!list)
		return;

	id = xmmscs_playlist_current_id (conn);

	for (l = list; l; l = x_list_next (l)) {
		x_hash_t *tab;
		char line[80];
		unsigned int i = XPOINTER_TO_UINT (l->data);

		g_clear_error (&err);
		
		tab = xmmscs_playlist_get_mediainfo (conn, i);

		memset (line, '\0', 80);

		if (!x_hash_lookup (tab, "title")) {
			xmmsc_entry_format (line, sizeof(line)-1, "%f (%m:%s)", tab);
		} else {
			xmmsc_entry_format (line, sizeof(line)-1, "%a - %t (%m:%s)", tab);
		}

		if (id == i) {
			print_info ("->[%d] %s", i, 
					g_convert (line, -1, "ISO-8859-1", "UTF-8", &r, &w, &err));
		} else {
			print_info ("  [%d] %s", i, 
					g_convert (line, -1, "ISO-8859-1", "UTF-8", &r, &w, &err));
		}

		if (err) {
			print_info ("convert error %s", err->message);
		}
		x_hash_destroy (tab);	
	}
	free (list);
}
	
static void
cmd_play (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_start (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't start playback: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
cmd_stop (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_stop (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't stop playback: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

}

static void
cmd_pause (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_pause (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't pause playback: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

}

static void
cmd_next (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playlist_set_next (conn, 0, 1);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't advance in playlist: %s\n", xmmsc_result_get_error (res));
		return;
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_next (conn);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);

}

static void
cmd_prev (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_playlist_set_next (conn, 0, -1);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't advance in playlist: %s\n", xmmsc_result_get_error (res));
		return;
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_next (conn);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}

static void
cmd_seek (xmmsc_connection_t *conn, int argc, char **argv)
{
	int id,seconds,duration,cur_playtime;
	x_hash_t *lista;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("You need to specify a number of seconds. Usage:\n"
			     "xmms2 seek seconds  - will seek to seconds\n"
			     "xmms2 seek +seconds - will add seconds\n"
			     "xmms2 seek -seconds - will remove seconds");
	}
	
	id = xmmscs_playlist_current_id (conn);
	lista = xmmscs_playlist_get_mediainfo (conn, id);
	duration = atoi (x_hash_lookup (lista, "duration"));
	cur_playtime = xmmscs_playback_playtime (conn);
	x_hash_destroy (lista);

	if (!duration)
		duration = 0;

	if (argv[2][0] == '+') {
		
		seconds=(atoi (argv[2]+1) * 1000) + cur_playtime;

		if (seconds >= duration) {
			printf ("Trying to seek to a higher value then total_playtime, Skipping to next song\n");
			res = xmmsc_playback_next (conn);
			xmmsc_result_unref (res);
		} else {
			printf ("Adding %s seconds to stream and jumping to %d\n",argv[2]+1, seconds/1000);
		}
		
	} else if (argv[2][0] == '-') {
		seconds = cur_playtime - atoi (argv[2]+1) * 1000;
		
		if (seconds < 0) {
			printf ("Trying to seek to a non positive value, seeking to 0\n");
			seconds=0;
		} else {
			printf ("Removing %s seconds to stream and jumping to %d\n",argv[2]+1, seconds/1000);
		}
		
	} else {
		seconds = atoi (argv[2]) * 1000;
	}
	
	res = xmmsc_playback_seek_ms (conn, seconds);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't seek to that position: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

/* FIXME
static void
cmd_stats (xmmsc_connection_t *conn, int argc, char **argv)
{
	x_list_t *list;
	xmmsc_result_t *res;

	res = xmmsc_playback_statistics (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_get_stringlist (res, &list)) {
		x_list_t *n;
		for (n = list; n; n = x_list_next (n)) {
			printf ("%s\n", (char*)n->data);
		}
	}

	xmmsc_result_unref (res);
}

*/

static void
cmd_quit (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_quit (conn);
}

static void
cmd_config (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("You need to specify a configkey and a value");
	}

	res = xmmsc_configval_set (conn, argv[2], argv[3]);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't set config value: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
cmd_config_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	x_list_t *l;

	l = xmmscs_configval_list (conn);
	
	if (!l)
		print_error ("Ooops :%s", xmmsc_get_last_error (conn));
	
	print_info ("Config Values:");
	
	while (l) {
		print_info ("%s = %s", l->data, xmmscs_configval_get (conn, l->data));
		l = x_list_next (l);
	}
	
	x_list_free (l);
}


static void
cmd_jump (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("You'll need to specify a ID to jump to.");
	}

	res = xmmsc_playlist_set_next (conn, 1, atoi (argv[2]));
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't jump to that song: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_next (conn);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		fprintf (stderr, "Couldn't go to next song: %s\n", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

/* STATUS FUNCTIONS */

static gchar songname[60];
static gint curr_dur;

static void handle_mediainfo (xmmsc_result_t *res, void *userdata);

static void
handle_playtime (xmmsc_result_t *res, void *userdata)
{
	guint dur;
	GError *err = NULL;
	int r, w;
	
	if (xmmsc_result_iserror (res)) {
		print_error ("apan");
	}
	
	if (!xmmsc_result_get_uint (res, &dur)) {
		print_error ("korv");
	}
	
	printf ("\rPlaying: %s: %02d:%02d of %02d:%02d", g_convert (songname, -1, "ISO-8859-1", "UTF-8", &r, &w, &err) , dur / 60000, (dur/1000)%60, curr_dur/60000, (curr_dur/1000)%60);
	fflush (stdout);

	xmmsc_result_unref (xmmsc_result_restart (res));
	xmmsc_result_unref (res);
}

static void
handle_mediainfo (xmmsc_result_t *res, void *userdata)
{
	x_hash_t *hash;
	gchar *tmp;
	gint mid;
	guint id;
	xmmsc_connection_t *c = userdata;

	if (!xmmsc_result_get_uint (res, &id)) {
		printf ("no id!\n");
		return;
	}

	hash = xmmscs_playlist_get_mediainfo (c, id);

	if (!hash) {
		printf ("no mediainfo!\n");
	} else {
		tmp = x_hash_lookup (hash, "id");

		mid = GPOINTER_TO_UINT (tmp);

		if (id == mid) {
			memset (songname, 0, 60);
			printf ("\n");
			xmmsc_entry_format (songname, 60, "%a - %t", hash);
			tmp = x_hash_lookup (hash, "duration");
			if (tmp)
				curr_dur = atoi (tmp);
			else
				curr_dur = 0;
		}
		xmmsc_playlist_entry_free (hash);
	}

	xmmsc_result_unref (xmmsc_result_restart (res));

	xmmsc_result_unref (res);

}

static void
cmd_status (xmmsc_connection_t *conn, int argc, char **argv)
{
	GMainLoop *ml;
	
	ml = g_main_loop_new (NULL, FALSE);

	/* Setup onchange signal for mediainfo */
	XMMS_CALLBACK_SET (conn, xmmsc_playlist_current_id, handle_mediainfo, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_playback_playtime, handle_playtime, NULL);

	xmmsc_setup_with_gmain (conn, NULL);

	g_main_loop_run (ml);
}

static void
handle_plch (xmmsc_result_t *res, void *userdata)
{
	unsigned int type, id, arg;

	if (xmmsc_result_iserror (res)) {
		print_error ("Huston we have a problem %s", xmmsc_result_get_error (res));
	}

	if (!xmmsc_result_get_playlist_change (res, &type, &id, &arg)) {
		print_error ("Couldn't fetch plch message!");
	}

	switch (type) {
		case XMMSC_PLAYLIST_ADD:
			print_info ("Added to playlist id = %d", id);
			break;
		case XMMSC_PLAYLIST_CLEAR:
			print_info ("Playlist was cleared!");
			break;
		case XMMSC_PLAYLIST_REMOVE:
			print_info ("%d was removed from the playlist", id);
			break;
		case XMMSC_PLAYLIST_SORT:
			print_info ("Playlist was sorted");
			break;
		case XMMSC_PLAYLIST_SHUFFLE:
			print_info ("Playlist was shuffled");
			break;
		default:
			print_info ("Playlist was altered, but only apan knows how..");
			break;
	}
	
	xmmsc_result_unref (xmmsc_result_restart (res));
	xmmsc_result_unref (res);
}

static void
cmd_watchpl (xmmsc_connection_t *conn, int argc, char **argv)
{
	GMainLoop *ml;

	ml = g_main_loop_new (NULL, FALSE);
	XMMS_CALLBACK_SET (conn, xmmsc_playlist_changed, handle_plch, conn);

	xmmsc_setup_with_gmain (conn, NULL);
	g_main_loop_run (ml);
}

/**
 * Defines all available commands.
 */

cmds commands[] = {
	/* Playlist managment */
	{ "add", "adds a URL to the playlist", cmd_add },
	{ "clear", "clears the playlist and stops playback", cmd_clear },
	{ "shuffle", "shuffles the playlist", cmd_shuffle },
	{ "remove", "removes something from the playlist", cmd_remove },
	{ "list", "lists the playlist", cmd_list },
	
	/* Playback managment */
	{ "play", "starts playback", cmd_play },
	{ "stop", "stops playback", cmd_stop },
	{ "pause", "pause playback", cmd_pause },
	{ "next", "play next song", cmd_next },
	{ "prev", "play previous song", cmd_prev },
	{ "seek", "seek to a specific place in current song", cmd_seek },
	{ "jump", "take a leap in the playlist", cmd_jump },
//	{ "move", "move a entry in the playlist", cmd_move },


	{ "status", "go into status mode", cmd_status },
	{ "watchpl", "go into watch playlist mode", cmd_watchpl },
	{ "config", "set a config value", cmd_config },
	{ "configlist", "list all config values", cmd_config_list },
	/*{ "statistics", "get statistics from server", cmd_stats },
	 */
	{ "quit", "make the server quit", cmd_quit },

	{ NULL, NULL, NULL },
};


int
main (int argc, char **argv)
{
	xmmsc_connection_t *connection;
	char *path;
	char *dbuspath;
	int i;

	connection = xmmsc_init ();

	if (!connection) {
		print_error ("Could not init xmmsc_connection, this is a memory problem, fix your os!");
	}

	dbuspath = getenv ("DBUS_PATH");
	if (!dbuspath) {
		path = g_strdup_printf ("unix:path=/tmp/xmms-dbus-%s", g_get_user_name ());
	} else {
		path = dbuspath;
	}

	if (!xmmsc_connect (connection, path)) {
		print_error ("Could not connect to xmms2d: %s", xmmsc_get_last_error (connection));
	}

	if (argc < 2) {

		xmmsc_deinit (connection);


		print_info ("Available commands:");
		
		for (i = 0; commands[i].name; i++) {
			print_info ("  %s - %s", commands[i].name, commands[i].help);
		}
		

		exit (0);

	}

	for (i = 0; commands[i].name; i++) {

		if (g_strcasecmp (commands[i].name, argv[1]) == 0) {
			commands[i].func (connection, argc, argv);
			xmmsc_deinit (connection);
			exit (0);
		}

	}

	xmmsc_deinit (connection);
	
	print_error ("Could not find any command called %s", argv[1]);

	return -1;

}

