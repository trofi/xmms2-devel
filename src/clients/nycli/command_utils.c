/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "command_utils.h"


/* FIXME: Can we differentiate between unset, set and default value? :-/ */
gboolean
command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_INT) {
		*v = arg->value.vint;
		retval = TRUE;
	}

	return retval;
}

gboolean
command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_STRING) {
		*v = arg->value.vstring;
		retval = TRUE;
	}

	return retval;
}

gint
command_arg_count (command_context_t *ctx)
{
	return ctx->argc - 1;
}

gboolean
command_arg_int_get (command_context_t *ctx, gint at, gint *v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = strtol (ctx->argv[at + 1], NULL, 10);
		retval = TRUE;
	}

	return retval;
}

gboolean
command_arg_string_get (command_context_t *ctx, gint at, gchar **v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = ctx->argv[at + 1];
		retval = TRUE;
	}

	return retval;
}

/* Grab all the arguments after the index as a single string.
 * Warning: the string must be freed manually afterwards!
 */
gboolean
command_arg_longstring_get (command_context_t *ctx, gint at, gchar **v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = g_strjoinv (" ", &ctx->argv[at + 1]);
		retval = TRUE;
	}

	return retval;
}

/* Parse a time value, either an absolute position or an offset. */
gboolean
command_arg_time_get (command_context_t *ctx, gint at, command_arg_time_t *v)
{
	gboolean retval = FALSE;
	gchar *s, *endptr;

	if (at < command_arg_count (ctx) && command_arg_string_get (ctx, at, &s)) {
		if (*s == '+' || *s == '-') {
			v->type = COMMAND_ARG_TIME_OFFSET;
			v->value.offset = strtol (s, &endptr, 10);
		} else {
			/* FIXME: always signed long int anyway? */
			v->type = COMMAND_ARG_TIME_POSITION;
			v->value.pos = strtol (s, &endptr, 10);
		}

		/* FIXME: We can have cleverer parsing for '2:17' or '3min' etc */
		if (*endptr == '\0') {
			retval = TRUE;
		}
	}

	return retval;
}