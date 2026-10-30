export const phi = {
	kind: "driver",
	name: "LED",
	label: "LED",
	tested: true,
	description: "On/off output on one GPIO.",
	define: "SUPPORT_DRV_LED",
	sources: ["led.c"],
	headers: ["led.h"],
	device: { struct: "mp_drv_led_t", ops: "mp_drv_led_device" },
	options: [
		{ name: "port", label: "Pin", type: "pin", required: true },
		{ name: "reverse", label: "Active low", type: "bool", default: false },
	],
}
