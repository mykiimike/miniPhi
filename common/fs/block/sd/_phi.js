export const phi = {
	kind: "module",
	name: "SD",
	label: "SD card",
	group: ["fs"],
	description: "SD card in SPI mode, as one mp_fs_block_t of 512-byte sectors. Each mp_fs_sd_init owns its SPI bus and its chip select, so two cards are two structs on two sets of pins. The second init fails when the pins are already claimed. Init leaves the bus at the shared SPI rate, and keeps it when no card answers so a later probe can try again. cd is an optional active-low socket switch with a pull-up; without it, presence is the last probe. A high cd pin is not clocked. The card is read with CMD17 and written with CMD24, one sector at a time. A transfer that gets no answer clears the identification. Capacity comes from the CSD. A set CCS bit in the OCR selects block addressing; otherwise the sector is sent as a byte address. on_media, when set after init, runs when an identified card appears or disappears.",
	define: "SUPPORT_COMMON_FS_BLOCK_SD",
	sources: ["sd.c"],
	headers: ["sd.h"],
	requires: ["FS", "SPI"],
	tests: ["test/card.c"],
}
