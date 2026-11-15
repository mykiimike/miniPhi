export const phi = {
	kind: "module",
	name: "PART",
	label: "Partition table",
	group: ["fs"],
	description: "Primary partitions on an mp_fs_block_t. mp_fs_part_scan reads the MBR in sector 0 and fills one mp_fs_part_t per usable entry. Each part is a block that starts at that entry and stops at its length. A protective MBR (type 0xEE) is handed to mp_fs_part_gpt, which reports it and does not parse GPT. Two scans are two calls and this module keeps nothing of its own.",
	define: "SUPPORT_COMMON_FS_PART",
	sources: ["part.c"],
	headers: ["part.h"],
	requires: ["FS"],
	tests: ["test/part.c"],
}
