export const phi = {
	kind: "module",
	name: "MEM",
	label: "Memory pool",
	description: "Fixed-size chunk allocator over a static pool, catching double and stray frees and overruns. No fragmentation.",
	define: "SUPPORT_COMMON_MEM",
	sources: ["mem.c"],
	headers: ["mem.h"],
	settings: [
		{ name: "MP_MEM_SIZE", label: "Pool size", type: "number", unit: "bytes", default: 1024, min: 128, max: 12288, ram: true },
		{ name: "MP_MEM_CHUNK", label: "Chunk size", type: "number", unit: "bytes", default: 50, min: 16, max: 512 },
		{ name: "MP_COMMON_MEM_USE_MALLOC", label: "Allocate the pool with malloc()", type: "bool", default: false },
	],
}
