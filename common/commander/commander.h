/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _HAVE_MP_COMMANDER_H
	#define _HAVE_MP_COMMANDER_H

	/**
	 * Commands any module can export, and a transport can call.
	 * The transport owns an mp_command_ctx_t and keeps it across calls.
	 * mp.h includes this header even when the module is off. The calls
	 * below then compile to nothing, so an export site needs no ifdef.
	 * The command tables and the callback still do: they use these types.
	 */

	#ifdef SUPPORT_COMMON_COMMANDER

		#ifndef MP_COMMANDER_MAX
			#define MP_COMMANDER_MAX 32
		#endif
		#ifndef MP_COMMANDER_ARGS
			#define MP_COMMANDER_ARGS 8
		#endif

		typedef struct mp_command_ctx_s mp_command_ctx_t;
		typedef struct mp_command_s mp_command_t;
		typedef struct mp_command_param_s mp_command_param_t;

		/**
		 * One execution. The transport fills it and passes the same one
		 * to every command of the session. Commander does not read it.
		 */
		struct mp_command_ctx_s {
			void *session;
		};

		typedef enum {
			MP_COMMAND_ARG_NONE = 0, /* an option flag, no value */
			MP_COMMAND_ARG_BOOL,
			MP_COMMAND_ARG_INT,
			MP_COMMAND_ARG_FLOAT,
			MP_COMMAND_ARG_STRING,
			MP_COMMAND_ARG_SELECT,
		} mp_command_arg_t;

		typedef struct {
			mp_command_arg_t arg;
			/** YES when this slot was given on the call */
			mp_bool_t present;
			union { int on; long integer; float number; int index; const char *string; } as;
		} mp_command_value_t;

		/**
		 * One parameter or one option. name == NULL ends the table.
		 * A parameter is positional. optional YES is only allowed after
		 * the required ones. An option is a --name flag and is always optional.
		 * NONE is an option flag. INT and FLOAT use min and max; max < min
		 * means the number is not bounded. choice is NULL-terminated.
		 */
		struct mp_command_param_s {
			const char *name;
			const char *label;
			const char *help;
			mp_command_arg_t arg;
			float min, max;
			const char *const *choice;
			const char *unit;
			mp_bool_t optional;
		};

		/**
		 * params and options are parallel to the tables, present says
		 * which slots the call filled. A string points at the caller's token.
		 */
		typedef mp_ret_t (*mp_command_fn_t)(
			mp_kernel_t *kernel,
			mp_command_ctx_t *ctx,
			const mp_command_value_t *params, int paramCount,
			const mp_command_value_t *options, int optionCount,
			void *user);

		/** Caller-owned. Zero it before the first register. */
		struct mp_command_s {
			mp_list_item_t item;
			const char *name;
			const char *label;
			const char *help;
			const char *module;
			const mp_command_param_t *params;
			const mp_command_param_t *options;
			mp_command_fn_t fn;
			void *user;
		};

		void mp_commander_init(mp_kernel_t *kernel);
		void mp_commander_fini(mp_kernel_t *kernel);

		/**
		 * Publish cmd. The same struct again updates it and returns TRUE.
		 * FALSE when the name is taken, the tables are refused, or the
		 * registry already holds MP_COMMANDER_MAX commands.
		 */
		mp_ret_t mp_command_register(mp_kernel_t *kernel, mp_command_t *cmd,
			const char *module, const char *name, const char *label, const char *help,
			const mp_command_param_t *params, const mp_command_param_t *options,
			mp_command_fn_t fn, void *user);
		void mp_command_unregister(mp_kernel_t *kernel, mp_command_t *cmd);

		int mp_command_count(mp_kernel_t *kernel);
		mp_command_t *mp_command_at(mp_kernel_t *kernel, int index);
		/** Compared without regard to case. */
		mp_command_t *mp_command_find(mp_kernel_t *kernel, const char *name);

		/**
		 * Run name in ctx. ctx is required and is not kept past the call.
		 * A token is a positional parameter, or an option: --name, --name=value,
		 * or --name value. A bool or NONE option with no value is on.
		 * A bool word in the next token (0, 1, on, off, true, false, yes, no)
		 * is that option's value. out receives "" when the command ran,
		 * otherwise "no context", "unknown", "bad args" or "refused".
		 */
		mp_ret_t mp_command_call(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
			const char *name, int argc, const char *const *argv,
			char *out, int outCap);

		/**
		 * Write value as text. A bool is "1" or "0", a select is its choice.
		 * Returns the length without the terminating NUL. A short buffer is
		 * still terminated, and the return is the full length.
		 */
		int mp_command_value_text(const mp_command_param_t *param,
			const mp_command_value_t *value, char *dst, int cap);

	#else

		#define mp_commander_init(...)       ((void)0)
		#define mp_commander_fini(...)       ((void)0)
		#define mp_command_register(...)     ((mp_ret_t)0)
		#define mp_command_unregister(...)   ((void)0)
		#define mp_command_count(...)        (0)
		#define mp_command_at(...)           ((void *)0)
		#define mp_command_find(...)         ((void *)0)
		#define mp_command_call(...)         ((mp_ret_t)0)
		#define mp_command_value_text(...)   (0)

	#endif

#endif
