export const phi = {
	kind: "module",
	name: "CIRCULAR",
	label: "Circular buffers",
	description: "Chained ring buffers built on pool chunks.",
	define: "SUPPORT_COMMON_CIRCULAR",
	sources: ["circular.c"],
	headers: ["circular.h"],
	requires: ["MEM"],
}
