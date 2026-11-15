export const phi = {
	kind: "module",
	name: "FAT",
	label: "FAT",
	group: ["fs"],
	description: "FAT12, FAT16 and FAT32 on an mp_fs_block_t. The kind comes from the cluster count, not from the name in the boot sector. The block is the volume, so a partitioned disk is mounted on the partition's block. Long names are read. Two mounts are two structs and do not share a cache. A write replaces bytes inside the size already recorded; it does not grow a file, create one, or free clusters.",
	define: "SUPPORT_COMMON_FS_FORMAT_FAT",
	sources: ["fat.c"],
	headers: ["fat.h"],
	requires: ["FS"],
	tests: ["test/fat.c"],
}
