/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003, 2004 Peter Alm, Tobias Rundstr�m, Anders Gustafsson
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsc/xmmsc_idnumbers.h>

#include <ruby.h>
#include <xmmsc/xmmsc_stdbool.h>

#include "rb_xmmsclient.h"
#include "rb_result.h"

#define CHECK_DELETED(xmms) \
	if (xmms->deleted) \
		rb_raise (eDisconnectedError, "client deleted");

#define METHOD_ADD_HANDLER(name, type) \
	RbXmmsClient *xmms = NULL; \
	xmmsc_result_t *res; \
\
	Data_Get_Struct (self, RbXmmsClient, xmms); \
\
	CHECK_DELETED (xmms); \
\
	res = xmmsc_##name (xmms->real); \
\
	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_##type);

static VALUE eXmmsClientError, eDisconnectedError;

static void c_mark (RbXmmsClient *xmms)
{
	rb_gc_mark (xmms->results);

	if (!NIL_P (xmms->disconnect_cb))
		rb_gc_mark (xmms->disconnect_cb);
}

static void c_free (RbXmmsClient *xmms)
{
	if (!xmms->deleted)
		xmmsc_unref (xmms->real);

	free (xmms);
}

static VALUE c_alloc (VALUE klass)
{
	RbXmmsClient *xmms;

	return Data_Make_Struct (klass, RbXmmsClient,
	                         c_mark, c_free, xmms);
}

/*
 * call-seq:
 *  XmmsClient::XmmsClient.new(name) -> xc
 *
 * Creates an XmmsClient::XmmsClient object.
 */
static VALUE c_init (VALUE self, VALUE name)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	if (!(xmms->real = xmmsc_init (StringValuePtr (name)))) {
		rb_raise (rb_eNoMemError, "failed to allocate memory");
		return Qnil;
	}

	xmms->deleted = false;
	xmms->results = rb_ary_new ();
	xmms->disconnect_cb = Qnil;

	return self;
}

/*
 * call-seq:
 *  xc.connect([path]) -> self
 *
 * Connects _xc_ to the XMMS2 daemon listening at _path_.
 * If _path_ isn't given, the default path is used.
 */
static VALUE c_connect (int argc, VALUE *argv, VALUE self)
{
	VALUE path;
	RbXmmsClient *xmms = NULL;
	char *p = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	rb_scan_args (argc, argv, "01", &path);

	if (!NIL_P (path))
		p = StringValuePtr (path);

	if (!xmmsc_connect (xmms->real, p))
		rb_raise (eXmmsClientError, "cannot connect to daemon");

	return self;
}

static VALUE c_delete (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_unref (xmms->real);
	xmms->deleted = true;

	return Qnil;
}

static void on_disconnect (void *data)
{
	VALUE self = (VALUE) data;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	rb_funcall (xmms->disconnect_cb, rb_intern ("call"), 0);
}

/*
 * call-seq:
 *  xc.on_disconnect { } -> self
 *
 * Sets the block that's executed when _xc_ is disconnected from the
 * XMMS2 daemon.
 */
static VALUE c_on_disconnect (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	if (!rb_block_given_p ())
		return Qnil;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmms->disconnect_cb = rb_block_proc ();

	xmmsc_disconnect_callback_set (xmms->real,
	                               on_disconnect, (void *) self);

	return self;
}

/*
 * call-seq:
 *  xc.last_error -> string or nil
 *
 * Returns the last error that occured in _xc_ or +nil+, if no error
 * occured yet.
 */
static VALUE c_last_error_get (VALUE self)
{
	RbXmmsClient *xmms = NULL;
	const char *s;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	s = xmmsc_get_last_error (xmms->real);

	return s ? rb_str_new2 (s) : Qnil;
}

/*
 * call-seq:
 *  xc.io_fd -> integer
 *
 * Returns the file descriptor of the XmmsClient IPC socket.
 */
static VALUE c_io_fd (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	return INT2FIX (xmmsc_io_fd_get (xmms->real));
}

/*
 * call-seq:
 *  xc.io_want_out -> true or false
 *
 * Returns +true+ if an outgoing (to server) clientlib command is pending,
 * +false+ otherwise.
 */
static VALUE c_io_want_out (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	return xmmsc_io_want_out (xmms->real) ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *  xc.io_in_handle -> nil
 *
 * Retrieves one incoming (from server) clientlib command if there are any in
 * the buffer.
 */
static VALUE c_io_in_handle (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_io_in_handle (xmms->real);

	return Qnil;
}

/*
 * call-seq:
 *  xc.io_out_handle -> nil
 *
 * Sends one outgoing (to server) clientlib command. You should check
 * XmmsClient::XmmsClient#io_want_out before calling this method.
 */
static VALUE c_io_out_handle (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_io_out_handle (xmms->real);

	return Qnil;
}

/*
 * call-seq:
 *  xc.io_disconnect -> nil
 *
 * Disconnects the IPC socket. This should only be used by event loop
 * implementations.
 */
static VALUE c_io_disconnect (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_io_disconnect (xmms->real);

	return Qnil;
}

/*
 * call-seq:
 *  xc.quit -> result
 *
 * Shuts down the XMMS2 daemon.
 */
static VALUE c_quit (VALUE self)
{
	METHOD_ADD_HANDLER (quit, DEFAULT);
}

/*
 * call-seq:
 *  xc.broadcast_quit -> result
 *
 * Will be called when the server is terminating.
 */
static VALUE c_broadcast_quit (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_quit, BROADCAST);
}

/*
 * call-seq:
 *  xc.playback_start -> result
 *
 * Starts playback.
 */
static VALUE c_playback_start (VALUE self)
{
	METHOD_ADD_HANDLER (playback_start, DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_pause -> result
 *
 * Pauses playback.
 */
static VALUE c_playback_pause (VALUE self)
{
	METHOD_ADD_HANDLER (playback_pause, DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_stop -> result
 *
 * Stops playback.
 */
static VALUE c_playback_stop (VALUE self)
{
	METHOD_ADD_HANDLER (playback_stop, DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_tickle -> result
 *
 * Advances to the next playlist entry.
 */
static VALUE c_playback_tickle (VALUE self)
{
	METHOD_ADD_HANDLER (playback_tickle, DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_status -> result
 *
 * Retrieves the playback status.
 */
static VALUE c_playback_status (VALUE self)
{
	METHOD_ADD_HANDLER (playback_status, DEFAULT);
}

/*
 * call-seq:
 *  xc.broadcast_playback_status -> result
 *
 * Retrieves the playback status as a broadcast.
 */
static VALUE c_broadcast_playback_status (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playback_status, BROADCAST);
}

/*
 * call-seq:
 *  xc.playback_playtime -> result
 *
 * Retrieves the playtime.
 */
static VALUE c_playback_playtime (VALUE self)
{
	METHOD_ADD_HANDLER (playback_playtime, DEFAULT);
}

/*
 * call-seq:
 *  xc.signal_playback_playtime -> result
 *
 * Retrieves the playtime as a signal.
 */
static VALUE c_signal_playback_playtime (VALUE self)
{
	METHOD_ADD_HANDLER (signal_playback_playtime, SIGNAL);
}

/*
 * call-seq:
 *  xc.playback_current_id -> result
 *
 * Retrieves the media id of the currently played track.
 */
static VALUE c_playback_current_id (VALUE self)
{
	METHOD_ADD_HANDLER (playback_current_id, DEFAULT);
}

/*
 * call-seq:
 *  xc.broadcast_playback_current_id -> result
 *
 * Retrieves the media id of the currently played track as a broadcast.
 */
static VALUE c_broadcast_playback_current_id (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playback_current_id, BROADCAST);
}

/*
 * call-seq:
 *  xc.broadcast_configval_changed -> result
 *
 * Retrieves configuration properties as a broadcast.
 */
static VALUE c_broadcast_configval_changed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_configval_changed, BROADCAST);
}

/*
 * call-seq:
 *  xc.playback_seek_ms(ms) -> result
 *
 * Seek to the song position given in _ms_.
 */
static VALUE c_playback_seek_ms (VALUE self, VALUE ms)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	Check_Type (ms, T_FIXNUM);

	res = xmmsc_playback_seek_ms (xmms->real, NUM2UINT (ms));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_seek_ms_rel(ms) -> result
 *
 * Seek in the song by the offset given in ms.
 */
static VALUE c_playback_seek_ms_rel (VALUE self, VALUE ms)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	Check_Type (ms, T_FIXNUM);

	res = xmmsc_playback_seek_ms_rel (xmms->real, NUM2INT (ms));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_seek_samples(samples) -> result
 *
 * Seek to the song position given in _samples_.
 */
static VALUE c_playback_seek_samples (VALUE self, VALUE samples)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	Check_Type (samples, T_FIXNUM);

	res = xmmsc_playback_seek_samples (xmms->real, NUM2UINT (samples));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

 /*
  * call-seq:
  *  xc.playback_seek_samples_rel(samples) -> result
  *
  * Seek in the song position by the offset given in samples.
  */
 static VALUE c_playback_seek_samples_rel (VALUE self, VALUE samples)
 {
	 RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;
 
	Data_Get_Struct (self, RbXmmsClient, xmms);
 
	CHECK_DELETED (xmms);
 
	Check_Type (samples, T_FIXNUM);
 
	res = xmmsc_playback_seek_samples_rel (xmms->real, NUM2INT (samples));
 
	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
 }

/*
 * call-seq:
 *  xc.playback_volume_set(channel, volume) -> result
 *
 * Sets playback volume for _channel_ to _volume_.
 */
static VALUE c_playback_volume_set (VALUE self, VALUE channel,
                                    VALUE volume)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	StringValue (channel);
	Check_Type (volume, T_FIXNUM);

	res = xmmsc_playback_volume_set (xmms->real,
	                                 StringValuePtr (channel),
	                                 NUM2UINT (volume));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playback_volume_get -> result
 *
 * Gets the current playback volume.
 */
static VALUE c_playback_volume_get (VALUE self)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_playback_volume_get (xmms->real);

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.broadcast_volume_changed -> result
 *
 * Registers a broadcast handler that's evoked when the playback volume
 * changes.
 */
static VALUE c_broadcast_playback_volume_changed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playback_volume_changed, BROADCAST);
}

/*
 * call-seq:
 *  xc.broadcast_playlist_changed -> result
 *
 * Retrieves a hash describing the change to the playlist as a broadcast.
 */
static VALUE c_broadcast_playlist_changed (VALUE self)
{
	METHOD_ADD_HANDLER(broadcast_playlist_changed, BROADCAST);
}

/*
 * call-seq:
 *  xc.playlist_current_pos -> result
 *
 * Retrieves the current position of the playlist. May raise an
 * XmmsClient::XmmsClient::ValueError exception if the current position is
 * undefined.
 */
static VALUE c_playlist_current_pos (VALUE self)
{
	METHOD_ADD_HANDLER(playlist_current_pos, DEFAULT);
}

/*
 * call-seq:
 *  xc.broadcast_playlist_current_pos -> result
 *
 * Retrieves the current playlist position as a broadcast. See
 * _playlist_current_pos_.
 */
static VALUE c_broadcast_playlist_current_pos (VALUE self)
{
	METHOD_ADD_HANDLER(broadcast_playlist_current_pos, BROADCAST);
}

/*
 * call-seq:
 *  xc.broadcast_medialib_entry_changed -> result
 *
 * Retrieves the id of a changed medialib entry as a broadcast.
 */
static VALUE c_broadcast_medialib_entry_changed (VALUE self)
{
	METHOD_ADD_HANDLER(broadcast_medialib_entry_changed, BROADCAST);
}

/*
 * call-seq:
 *  xc.playlist_shuffle -> result
 *
 * Shuffles the playlist.
 */
static VALUE c_playlist_shuffle (VALUE self)
{
	METHOD_ADD_HANDLER(playlist_shuffle, DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_list -> result
 *
 * Retrieves an array containing an id for each playlist position.
 */
static VALUE c_playlist_list (VALUE self)
{
	METHOD_ADD_HANDLER(playlist_list, DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_clear -> result
 *
 * Clears the playlist.
 */
static VALUE c_playlist_clear (VALUE self)
{
	METHOD_ADD_HANDLER(playlist_clear, DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_set_next(pos) -> result
 *
 * Sets the next song to be played to _pos_ (an absolute position).
 */
static VALUE c_playlist_set_next (VALUE self, VALUE pos)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	Check_Type (pos, T_FIXNUM);

	res = xmmsc_playlist_set_next (xmms->real, FIX2INT (pos));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_set_next_rel(pos) -> result
 *
 * Sets the next song to be played based on the current position where
 * _pos_ is a value relative to the current position.
 */
static VALUE c_playlist_set_next_rel (VALUE self, VALUE pos)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	Check_Type (pos, T_FIXNUM);

	res = xmmsc_playlist_set_next_rel (xmms->real, FIX2INT (pos));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_add(arg) -> result
 *
 * Adds an entry to the current playlist. _arg_ can be either an URL or an id.
 */
static VALUE c_playlist_add (VALUE self, VALUE arg)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;
	bool is_str;

	if (!NIL_P (rb_check_string_type (arg)))
		is_str = true;
	else if (rb_obj_is_kind_of (arg, rb_cFixnum))
		is_str = false;
	else {
		rb_raise (eXmmsClientError, "unsupported argument");
		return Qnil;
	}

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	if (is_str)
		res = xmmsc_playlist_add (xmms->real, StringValuePtr (arg));
	else
		res = xmmsc_playlist_add_id (xmms->real, NUM2UINT (arg));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_insert(pos, arg) -> result
 *
 * Inserts an entry to the current playlist at position _pos_.
 * _arg_ can be either an URL or an id.
 */
static VALUE c_playlist_insert (VALUE self, VALUE pos, VALUE arg)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;
	bool is_str;

	if (!NIL_P (rb_check_string_type (arg)))
		is_str = true;
	else if (rb_obj_is_kind_of (arg, rb_cFixnum))
		is_str = false;
	else {
		rb_raise (eXmmsClientError, "unsupported argument");
		return Qnil;
	}

	Check_Type (pos, T_FIXNUM);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	if (is_str)
		res = xmmsc_playlist_insert (xmms->real, NUM2UINT (pos), StringValuePtr (arg));
	else
		res = xmmsc_playlist_insert_id (xmms->real, NUM2UINT (pos), NUM2UINT (arg));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_remove(pos) -> result
 *
 * Removes the entry at _pos_ from the current playlist.
 */
static VALUE c_playlist_remove (VALUE self, VALUE pos)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Check_Type (pos, T_FIXNUM);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_playlist_remove (xmms->real, NUM2UINT (pos));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_move(current_pos, new_pos) -> result
 *
 * Moves the entry at _current_pos_ to _new_pos_.
 */
static VALUE c_playlist_move (VALUE self, VALUE cur_pos, VALUE new_pos)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Check_Type (cur_pos, T_FIXNUM);
	Check_Type (new_pos, T_FIXNUM);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_playlist_move (xmms->real, NUM2UINT (cur_pos),
	                           NUM2UINT (new_pos));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.playlist_sort(property) -> result
 *
 * Sorts the playlist on _property_, which is a medialib property such as
 * "title".
 */
static VALUE c_playlist_sort (VALUE self, VALUE property)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (property);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_playlist_sort (xmms->real, StringValuePtr (property));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_select(query) -> result
 *
 * Runs an SQL query on the medialib.
 */
static VALUE c_medialib_select (VALUE self, VALUE query)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (query);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_select (xmms->real, StringValuePtr (query));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_playlist_save_current(name) -> result
 *
 * Saves the current playlist to the medialib under _name_.
 */
static VALUE c_medialib_playlist_save_current (VALUE self, VALUE name)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (name);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_playlist_save_current (xmms->real,
	                                            StringValuePtr (name));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_playlist_load(name) -> result
 *
 * Appends the playlist _name_ to the current playlist.
 */
static VALUE c_medialib_playlist_load (VALUE self, VALUE name)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (name);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_playlist_load (xmms->real,
	                                    StringValuePtr (name));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_add_entry(url) -> result
 *
 * Adds _url_ to the medialib.
 */
static VALUE c_medialib_add_entry (VALUE self, VALUE url)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (url);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_add_entry (xmms->real, StringValuePtr (url));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_get_info(id) -> result
 *
 * Retrieves medialib info for _id_.
 */
static VALUE c_medialib_get_info (VALUE self, VALUE id)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	Check_Type (id, T_FIXNUM);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_get_info (xmms->real, FIX2INT (id));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_entry_property_set(id, key, value, *source) -> result
 *
 * Write info to the medialib at _id_. _source_ is an optional argument that
 * describes where to write the mediainfo. If _source_ is omitted, the
 * mediainfo is written to "client/<yourclient>" where <yourclient> is the
 * name you specified in _XmmsClient::XmmsClient.new(name)_.
 */
static VALUE c_medialib_entry_property_set (int argc, VALUE *argv,
                                            VALUE self)
{
	VALUE id, key, value, src = Qnil;
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	rb_scan_args (argc, argv, "31", &id, &key, &value, &src);

	Check_Type (id, T_FIXNUM);
	StringValue (key);
	StringValue (value);

	if (NIL_P (src))
		res = xmmsc_medialib_entry_property_set (xmms->real,
		                                         FIX2INT (id),
		                                         StringValuePtr (key),
		                                         StringValuePtr (value));
	else
		res = xmmsc_medialib_entry_property_set_with_source (
			xmms->real,
			FIX2INT (id),
			StringValuePtr (src),
			StringValuePtr (key),
			StringValuePtr (value));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_add_to_playlist(query) -> result
 *
 * Adds files matching the SQL query to the current playlist.
 */
static VALUE c_medialib_add_to_playlist (VALUE self, VALUE query)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (query);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_add_to_playlist (xmms->real,
	                                      StringValuePtr (query));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_playlists_list -> result
 *
 * Retrieves all playlists that are stored in the medialib.
 * Note that clients should treat internally used playlists
 * (marked with a leading underscore) carefully.
 */
static VALUE c_medialib_playlists_list (VALUE self)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_playlists_list (xmms->real);

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_playlist_import(playlist, url) -> result
 *
 * Imports a new playlist from _url_ to the medialib.
 */
static VALUE c_medialib_playlist_import (VALUE self, VALUE playlist,
                                         VALUE url)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (playlist);
	StringValue (url);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_playlist_import (xmms->real,
	                                      StringValuePtr (playlist),
	                                      StringValuePtr (url));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_playlist_export(playlist, mime) -> result
 *
 * Exports a medialib playlist in a format described by _mime_.
 */
static VALUE c_medialib_playlist_export (VALUE self, VALUE playlist,
                                         VALUE mime)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (playlist);
	StringValue (mime);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_playlist_import (xmms->real,
	                                      StringValuePtr (playlist),
	                                      StringValuePtr (mime));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_path_import(path) -> result
 *
 * Recursively imports all media files under _path_ to the medialib.
 */
static VALUE c_medialib_path_import (VALUE self, VALUE path)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (path);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_path_import (xmms->real,
	                                  StringValuePtr (path));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.medialib_rehash(id) -> result
 *
 * Rehashes the medialib entry at _id_ or the whole medialib if _id_ == 0.
 */
static VALUE c_medialib_rehash (VALUE self, VALUE id)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Check_Type (id, T_FIXNUM);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_medialib_rehash (xmms->real, FIX2INT (id));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.broadcast_mediainfo_reader_status -> result
 *
 * Requests the status of the mediainfo reader.
 */
static VALUE c_broadcast_mediainfo_reader_status (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_mediainfo_reader_status, BROADCAST);
}

/*
 * call-seq:
 *  xc.signal_mediainfo_reader_unindexed -> result
 *
 * Requests the number of unindexed entries in the medialib.
 */
static VALUE c_signal_mediainfo_reader_unindexed (VALUE self)
{
	METHOD_ADD_HANDLER (signal_mediainfo_reader_unindexed, SIGNAL);
}

/*
 * call-seq:
 *  xc.configval_get(key) -> result
 *
 * Retrieves the value of the configuration property at _key_.
 */
static VALUE c_configval_get (VALUE self, VALUE key)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (key);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_configval_get (xmms->real, StringValuePtr (key));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.configval_set(key, value) -> result
 *
 * Sets the value of the configuration property at _key_ to _value_.
 */
static VALUE c_configval_set (VALUE self, VALUE key, VALUE val)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	StringValue (key);
	StringValue (val);

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_configval_set (xmms->real, StringValuePtr (key),
	                           StringValuePtr (val));

	return TO_XMMS_CLIENT_RESULT (self, res, RESULT_TYPE_DEFAULT);
}

/*
 * call-seq:
 *  xc.signal_visualization_data -> result
 *
 * Retrieves visualization data as a signal.
 */
static VALUE c_signal_visualisation_data (VALUE self)
{
	METHOD_ADD_HANDLER(signal_visualisation_data, SIGNAL);
}

void Init_XmmsClient (VALUE mXmmsClient)
{
	VALUE c;

	c = rb_define_class_under (mXmmsClient, "XmmsClient", rb_cObject);

	rb_define_alloc_func (c, c_alloc);
	rb_define_method (c, "initialize", c_init, 1);
	rb_define_method (c, "connect", c_connect, -1);
	rb_define_method (c, "delete!", c_delete, 0);
	rb_define_method (c, "on_disconnect", c_on_disconnect, 0);
	rb_define_method (c, "last_error", c_last_error_get, 0);

	rb_define_method (c, "io_fd", c_io_fd, 0);
	rb_define_method (c, "io_want_out", c_io_want_out, 0);
	rb_define_method (c, "io_in_handle", c_io_in_handle, 0);
	rb_define_method (c, "io_out_handle", c_io_out_handle, 0);
	rb_define_method (c, "io_disconnect", c_io_disconnect, 0);

	rb_define_method (c, "quit", c_quit, 0);
	rb_define_method (c, "broadcast_quit", c_broadcast_quit, 0);

	rb_define_method (c, "playback_start", c_playback_start, 0);
	rb_define_method (c, "playback_pause", c_playback_pause, 0);
	rb_define_method (c, "playback_stop", c_playback_stop, 0);
	rb_define_method (c, "playback_tickle", c_playback_tickle, 0);
	rb_define_method (c, "broadcast_playback_status",
	                  c_broadcast_playback_status, 0);
	rb_define_method (c, "playback_status", c_playback_status, 0);
	rb_define_method (c, "playback_playtime", c_playback_playtime, 0);
	rb_define_method (c, "signal_playback_playtime",
	                  c_signal_playback_playtime, 0);
	rb_define_method (c, "playback_current_id",
	                  c_playback_current_id, 0);
	rb_define_method (c, "broadcast_playback_current_id",
	                  c_broadcast_playback_current_id, 0);
	rb_define_method (c, "playback_seek_ms", c_playback_seek_ms, 1);
	rb_define_method (c, "playback_seek_ms_rel", c_playback_seek_ms_rel, 1);
	rb_define_method (c, "playback_seek_samples",
	                  c_playback_seek_samples, 1);
	rb_define_method (c, "playback_seek_samples_rel",
	                  c_playback_seek_samples_rel, 1);
	rb_define_method (c, "playback_volume_set",
	                  c_playback_volume_set, 2);
	rb_define_method (c, "playback_volume_get",
	                  c_playback_volume_get, 0);
	rb_define_method (c, "broadcast_playback_volume_changed",
	                  c_broadcast_playback_volume_changed, 0);

	rb_define_method (c, "broadcast_playlist_changed",
	                  c_broadcast_playlist_changed, 0);
	rb_define_method (c, "playlist_current_pos",
	                  c_playlist_current_pos, 0);
	rb_define_method (c, "broadcast_playlist_current_pos",
	                  c_broadcast_playlist_current_pos, 0);
	rb_define_method (c, "broadcast_medialib_entry_changed",
	                  c_broadcast_medialib_entry_changed, 0);

	rb_define_method (c, "playlist_shuffle", c_playlist_shuffle, 0);
	rb_define_method (c, "playlist_list", c_playlist_list, 0);
	rb_define_method (c, "playlist_clear", c_playlist_clear, 0);
	rb_define_method (c, "playlist_set_next", c_playlist_set_next, 1);
	rb_define_method (c, "playlist_set_next_rel",
	                  c_playlist_set_next_rel, 1);
	rb_define_method (c, "playlist_add", c_playlist_add, 1);
	rb_define_method (c, "playlist_insert", c_playlist_insert, 2);
	rb_define_method (c, "playlist_remove", c_playlist_remove, 1);
	rb_define_method (c, "playlist_move", c_playlist_move, 2);
	rb_define_method (c, "playlist_sort", c_playlist_sort, 1);

	rb_define_method (c, "medialib_select", c_medialib_select, 1);
	rb_define_method (c, "medialib_playlist_save_current",
	                  c_medialib_playlist_save_current, 1);
	rb_define_method (c, "medialib_playlist_load",
	                  c_medialib_playlist_load, 1);
	rb_define_method (c, "medialib_add_entry", c_medialib_add_entry, 1);
	rb_define_method (c, "medialib_get_info", c_medialib_get_info, 1);
	rb_define_method (c, "medialib_entry_property_set",
	                  c_medialib_entry_property_set, -1);
	rb_define_method (c, "medialib_add_to_playlist",
	                  c_medialib_add_to_playlist, 1);
	rb_define_method (c, "medialib_playlists_list",
	                  c_medialib_playlists_list, 0);
	rb_define_method (c, "medialib_playlist_import",
	                  c_medialib_playlist_import, 2);
	rb_define_method (c, "medialib_playlist_export",
	                  c_medialib_playlist_export, 2);
	rb_define_method (c, "medialib_path_import", c_medialib_path_import, 1);
	rb_define_method (c, "medialib_rehash", c_medialib_rehash, 1);

	rb_define_method (c, "broadcast_mediainfo_reader_status",
	                  c_broadcast_mediainfo_reader_status, 0);
	rb_define_method (c, "signal_mediainfo_reader_unindexed",
	                  c_signal_mediainfo_reader_unindexed, 0);

	rb_define_method (c, "signal_visualisation_data",
	                  c_signal_visualisation_data, 0);

	rb_define_method (c, "configval_get", c_configval_get, 1);
	rb_define_method (c, "configval_set", c_configval_set, 2);
	rb_define_method (c, "broadcast_configval_changed",
	                  c_broadcast_configval_changed, 0);

	rb_define_const (c, "PLAY",
	                 INT2FIX (XMMS_PLAYBACK_STATUS_PLAY));
	rb_define_const (c, "STOP",
	                 INT2FIX (XMMS_PLAYBACK_STATUS_STOP));
	rb_define_const (c, "PAUSE",
	                 INT2FIX (XMMS_PLAYBACK_STATUS_PAUSE));

	rb_define_const (c, "IDLE",
	                 INT2FIX (XMMS_MEDIAINFO_READER_STATUS_IDLE));
	rb_define_const (c, "RUNNING",
	                 INT2FIX (XMMS_MEDIAINFO_READER_STATUS_RUNNING));

	eXmmsClientError = rb_define_class_under (c, "XmmsClientError",
	                                          rb_eStandardError);
	eDisconnectedError = rb_define_class_under (c, "DisconnectedError",
	                                            eXmmsClientError);

	Init_Result (mXmmsClient);
}
