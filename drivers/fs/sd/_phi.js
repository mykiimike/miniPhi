export const phi = {
	kind: "driver",
	group: ["fs"],
	name: "SD",
	label: "SD card",
	tested: false,
	description: "SD card in SPI mode as block devices. The whole card is one MP_DEVICE_BLOCK, and each primary MBR partition is another. A protective MBR is left as the whole card. The socket is probed from open, from face, and from set when the value is not zero. Losing the card unregisters those blocks; the next successful probe registers them again. Pins are sck, mosi, miso and cs. cd is an optional active-low card detect with no pin on the ZLX TF slot.",
	define: "SUPPORT_DRV_FS_SD",
	sources: ["sd.c"],
	headers: ["sd.h"],
	/* the SD module already publishes include/sd/sd.h */
	headerDir: "fs-sd",
	requires: ["DEVICE", "SD", "PART"],
	device: { struct: "mp_drv_sd_t", ops: "mp_drv_sd_device" },
	options: [
		{ name: "sck", label: "SCK", type: "pin", required: true },
		{ name: "mosi", label: "MOSI", type: "pin", required: true },
		{ name: "miso", label: "MISO", type: "pin", required: true },
		{ name: "cs", label: "Chip select (active low)", type: "pin", required: true },
		{ name: "cd", label: "Card detect (active low)", type: "pin" },
	],
}
