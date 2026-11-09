/* Host test of the command registry. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_COMMANDER
#define MP_COMMANDER_MAX 4
#define MP_COMMANDER_ARGS 4
#include "../commander.c"

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static int ran;
static mp_kernel_t *sawKernel;
static mp_command_ctx_t *sawCtx;
static long sawGain;
static int sawLoud;
static int sawHaveNote;
static const char *sawNote;
static int sawMode;

static const char *const modes[] = { "auto", "manual", NULL };

static const mp_command_param_t gainParams[] = {
	{ "gain", "Gain", NULL, MP_COMMAND_ARG_INT, 0, 10, NULL, NULL, NO },
	{ NULL }
};
static const mp_command_param_t gainOptions[] = {
	{ "loud", "Loud", NULL, MP_COMMAND_ARG_BOOL, 0, 0, NULL, NULL, YES },
	{ NULL }
};
static const mp_command_param_t noteParams[] = {
	{ "text", "Text", NULL, MP_COMMAND_ARG_STRING, 0, 0, NULL, NULL, YES },
	{ NULL }
};
static const mp_command_param_t modeParams[] = {
	{ "mode", "Mode", NULL, MP_COMMAND_ARG_SELECT, 0, 0, modes, NULL, NO },
	{ NULL }
};
static const mp_command_param_t levelParams[] = {
	{ "level", "Level", NULL, MP_COMMAND_ARG_FLOAT, -1, 1, NULL, NULL, NO },
	{ NULL }
};

static mp_ret_t _ping(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
	const mp_command_value_t *params, int paramCount,
	const mp_command_value_t *options, int optionCount, void *user) {
	(void)params;
	(void)paramCount;
	(void)options;
	(void)optionCount;
	sawKernel = kernel;
	sawCtx = ctx;
	ran++;
	return(user ? TRUE : FALSE);
}

static mp_ret_t _gain(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
	const mp_command_value_t *params, int paramCount,
	const mp_command_value_t *options, int optionCount, void *user) {
	(void)kernel;
	(void)ctx;
	(void)user;
	if(paramCount != 1 || !params || !params[0].present)
		return(FALSE);
	sawGain = params[0].as.integer;
	sawLoud = optionCount == 1 && options && options[0].present && options[0].as.on;
	ran++;
	return(TRUE);
}

static mp_ret_t _note(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
	const mp_command_value_t *params, int paramCount,
	const mp_command_value_t *options, int optionCount, void *user) {
	(void)kernel;
	(void)ctx;
	(void)options;
	(void)optionCount;
	(void)user;
	sawHaveNote = paramCount == 1 && params && params[0].present;
	sawNote = sawHaveNote ? params[0].as.string : NULL;
	ran++;
	return(TRUE);
}

static mp_ret_t _mode(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
	const mp_command_value_t *params, int paramCount,
	const mp_command_value_t *options, int optionCount, void *user) {
	(void)kernel;
	(void)ctx;
	(void)options;
	(void)optionCount;
	(void)user;
	if(paramCount != 1 || !params || !params[0].present)
		return(FALSE);
	sawMode = params[0].as.index;
	ran++;
	return(TRUE);
}

static mp_ret_t _level(mp_kernel_t *kernel, mp_command_ctx_t *ctx,
	const mp_command_value_t *params, int paramCount,
	const mp_command_value_t *options, int optionCount, void *user) {
	char text[16];
	const mp_command_param_t *slot = user;

	(void)kernel;
	(void)ctx;
	(void)options;
	(void)optionCount;
	if(paramCount != 1 || !params)
		return(FALSE);
	expect(mp_command_value_text(slot, &params[0], text, (int)sizeof(text)) > 0, "level text");
	expect(strcmp(text, "-0.5") == 0, "level reads back");
	ran++;
	return(TRUE);
}

int main(void) {
	mp_kernel_t kernel;
	mp_command_ctx_t ctx;
	mp_command_t ping, gain, note, mode, level, extra;
	char out[32];
	char tiny[1];
	const char *argv[4];
	int before;
	int session = 7;

	memset(&kernel, 0, sizeof(kernel));
	memset(&ping, 0, sizeof(ping));
	memset(&gain, 0, sizeof(gain));
	memset(&note, 0, sizeof(note));
	memset(&mode, 0, sizeof(mode));
	memset(&level, 0, sizeof(level));
	memset(&extra, 0, sizeof(extra));
	memset(&ctx, 0, sizeof(ctx));
	ctx.session = &session;
	mp_commander_init(&kernel);

	expect(mp_command_register(NULL, &ping, "CORE", "ping", NULL, NULL, NULL, NULL, _ping, &session) == FALSE, "no kernel");
	expect(mp_command_register(&kernel, &ping, "CORE", "ping", "Ping", NULL, NULL, NULL, _ping, &session) == TRUE, "register ping");
	expect(mp_command_register(&kernel, &ping, "CORE", "ping", "Ping", NULL, NULL, NULL, _ping, &session) == TRUE, "register again");
	expect(mp_command_count(&kernel) == 1, "one command");
	expect(mp_command_register(&kernel, &gain, "CORE", "PING", NULL, NULL, NULL, NULL, _ping, NULL) == FALSE, "name taken");
	expect(mp_command_register(&kernel, &gain, "SENSOR", "gain", "Gain", NULL, gainParams, gainOptions, _gain, NULL) == TRUE, "register gain");
	expect(mp_command_register(&kernel, &note, "SENSOR", "note", NULL, NULL, noteParams, NULL, _note, NULL) == TRUE, "register note");
	expect(mp_command_register(&kernel, &mode, "SENSOR", "mode", NULL, NULL, modeParams, NULL, _mode, NULL) == TRUE, "register mode");
	expect(mp_command_count(&kernel) == 4, "full");
	expect(mp_command_register(&kernel, &extra, "SENSOR", "level", NULL, NULL, levelParams, NULL, _level, (void *)levelParams) == FALSE, "past max");
	mp_command_unregister(&kernel, &note);
	expect(mp_command_count(&kernel) == 3, "after unregister");
	expect(mp_command_register(&kernel, &level, "SENSOR", "level", NULL, NULL, levelParams, NULL, _level, (void *)levelParams) == TRUE, "register level");
	expect(mp_command_find(&kernel, "Gain") == &gain, "find gain");
	expect(mp_command_at(&kernel, 1) == &gain, "at 1");
	expect(mp_command_find(&kernel, "note") == NULL, "note is gone");

	argv[0] = NULL;
	expect(mp_command_call(&kernel, &ctx, "ping", 0, NULL, out, (int)sizeof(out)) == TRUE, "ping");
	expect(out[0] == 0, "ping is silent");
	expect(sawKernel == &kernel && sawCtx == &ctx && sawCtx->session == &session, "same context");
	expect(mp_command_call(&kernel, NULL, "ping", 0, NULL, out, (int)sizeof(out)) == FALSE, "no context");
	expect(strcmp(out, "no context") == 0, "no context text");
	expect(mp_command_call(&kernel, &ctx, "missing", 0, NULL, out, (int)sizeof(out)) == FALSE, "unknown");
	expect(strcmp(out, "unknown") == 0, "unknown text");

	before = ran;
	argv[0] = "4";
	argv[1] = "--loud";
	expect(mp_command_call(&kernel, &ctx, "GAIN", 2, argv, out, (int)sizeof(out)) == TRUE, "gain 4 loud");
	expect(sawGain == 4 && sawLoud == 1, "gain stored");
	argv[1] = "--loud";
	argv[2] = "off";
	expect(mp_command_call(&kernel, &ctx, "gain", 3, argv, out, (int)sizeof(out)) == TRUE, "gain loud off");
	expect(sawLoud == 0, "loud cleared");
	argv[1] = "--loud=yes";
	expect(mp_command_call(&kernel, &ctx, "gain", 2, argv, out, (int)sizeof(out)) == TRUE, "loud equals");
	expect(sawLoud == 1, "loud from equals");
	argv[0] = "11";
	expect(mp_command_call(&kernel, &ctx, "gain", 1, argv, out, (int)sizeof(out)) == FALSE, "gain too high");
	expect(strcmp(out, "bad args") == 0, "bad args text");
	expect(ran == before + 3, "refused gain did not run");
	argv[0] = "--unit";
	argv[1] = "4";
	expect(mp_command_call(&kernel, &ctx, "gain", 2, argv, out, (int)sizeof(out)) == FALSE, "unknown option");

	argv[0] = "board";
	expect(mp_command_call(&kernel, &ctx, "note", 1, argv, out, (int)sizeof(out)) == FALSE, "note was removed");
	expect(mp_command_register(&kernel, &note, "SENSOR", "note", NULL, NULL, noteParams, NULL, _note, NULL) == FALSE, "registry still full");
	mp_command_unregister(&kernel, &ping);
	expect(mp_command_register(&kernel, &note, "SENSOR", "note", NULL, NULL, noteParams, NULL, _note, NULL) == TRUE, "note returns");
	expect(mp_command_call(&kernel, &ctx, "note", 0, NULL, out, (int)sizeof(out)) == TRUE, "optional omitted");
	expect(sawHaveNote == 0, "note absent");
	argv[0] = "board";
	expect(mp_command_call(&kernel, &ctx, "note", 1, argv, out, (int)sizeof(out)) == TRUE, "note text");
	expect(sawHaveNote == 1 && strcmp(sawNote, "board") == 0, "note stored");

	argv[0] = "MANUAL";
	expect(mp_command_call(&kernel, &ctx, "mode", 1, argv, out, (int)sizeof(out)) == TRUE, "select");
	expect(sawMode == 1, "manual index");
	argv[0] = "warm";
	expect(mp_command_call(&kernel, &ctx, "mode", 1, argv, out, (int)sizeof(out)) == FALSE, "bad select");

	argv[0] = "-0.5";
	expect(mp_command_call(&kernel, &ctx, "level", 1, argv, out, (int)sizeof(out)) == TRUE, "float");
	expect(mp_command_value_text(&levelParams[0], NULL, tiny, 1) == 0, "no value");

	mp_commander_fini(&kernel);
	expect(mp_command_count(&kernel) == 0, "fini clears");
	expect(mp_command_call(&kernel, &ctx, "level", 1, argv, out, (int)sizeof(out)) == FALSE, "gone after fini");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
