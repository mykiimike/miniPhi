export const phi = {
	kind: "module",
	name: "KFLASH",
	label: "Flash key/value store",
	description: "Keeps small values across power cycles in the last two pages of the internal flash. A key is a name hashed with FNV-1a; the module that owns it declares its size and its default with mp_kflash_listen(), and its callback runs whenever someone changes it. Values are appended, so a page is only erased when it fills up. Needs an arch with mp_flash_program(): STM32 only for now.",
	define: "SUPPORT_COMMON_KFLASH",
	sources: ["kflash.c"],
	headers: ["kflash.h"],
	settings: [
		{ name: "MP_KFLASH_VALUE_MAX", label: "Largest value", type: "number", unit: "bytes", default: 32, min: 4, max: 256 },
	],
}
