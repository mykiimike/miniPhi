export const phi = {
	kind: "module",
	name: "TASK",
	label: "Tasks",
	description: "A task is a callback in a fixed table. The build picks one scheduler. On a preemptive C3, C6 or STM32, a task above priority 0 has its own stack so two of the same priority can share a tick.",
	required: true,
	sources: ["task.c", "sched/coop.c", "sched/priority.c", "sched/preempt.c"],
	headers: ["task.h"],
	tests: ["test/coop.c", "test/priority.c", "test/preempt.c"],
	settings: [
		{ name: "MP_TASK_MAX", label: "Task slots", type: "number", default: 10, min: 1, max: 64 },
		{
			name: "MP_TASK_SCHEDULER",
			label: "Scheduler",
			type: "enum",
			values: ["0", "1", "2"],
			labels: ["Cooperative", "Priority cooperative", "Preemptive"],
			default: "0",
		},
		{ name: "MP_TASK_PRIORITIES", label: "Priority levels", type: "number", default: 4, min: 2, max: 8 },
	],
}
