export const phi = {
	kind: "module",
	name: "TASK",
	label: "Tasks",
	description: "Cooperative task pool. A task is a callback in a fixed table, with no stack of its own.",
	required: true,
	sources: ["task.c"],
	headers: ["task.h"],
	settings: [
		{ name: "MP_TASK_MAX", label: "Task slots", type: "number", default: 10, min: 1, max: 64 },
	],
}
