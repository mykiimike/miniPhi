export const phi = {
	kind: "module",
	name: "FS",
	label: "Block device",
	group: ["fs"],
	description: "The sector interface a filesystem sits on. One mp_fs_block_t is one medium: read sectors, write sectors, and the number of sectors when the medium knows it. The caller owns the struct. Two media are two structs, and this module keeps nothing of its own.",
	define: "SUPPORT_COMMON_FS",
	sources: ["fs.c"],
	headers: ["fs.h"],
}
