export const phi = {
	kind: "module",
	name: "COMMANDER",
	label: "Command registry",
	description: "Named commands with typed parameters and options. A module exports a callback; a transport (a terminal, an HTTP request) owns a context and calls the command through it, and can call the next one in the same context. The header stays in the build either way: with the module off the export calls compile to nothing.",
	define: "SUPPORT_COMMON_COMMANDER",
	sources: ["commander.c"],
	headers: ["commander.h"],
	tests: ["test/call.c", "test/off.c"],
	settings: [
		{ name: "MP_COMMANDER_MAX", label: "Registered commands", type: "number", default: 32, min: 4, max: 128 },
		{ name: "MP_COMMANDER_ARGS", label: "Parameters per command", type: "number", default: 8, min: 1, max: 32 },
	],
}
