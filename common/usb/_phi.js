export const phi = {
	kind: "module",
	name: "USB",
	label: "USB",
	description: "PHY ports of the chip, listed at boot. caps is what the silicon can do. A port may be locked to device or host; the others stay free and off. A lock is armed in that role, and a device lock raises the pull-up so a host reset shows on the link. One task applies connect, disconnect, reset, suspend and fault events. No endpoints and no classes.",
	define: "SUPPORT_COMMON_USB",
	sources: ["usb.c"],
	headers: ["usb.h"],
	settings: [
		{ name: "MP_USB_PORT_MAX", label: "PHY ports", type: "number", default: 4, min: 1, max: 8 },
		{ name: "MP_USB_EVENTS", label: "Queued link events per port", type: "number", default: 8, min: 2, max: 32 },
	],
}
