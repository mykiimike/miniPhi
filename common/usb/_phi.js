export const phi = {
	kind: "module",
	name: "USB",
	label: "USB",
	description: "PHY ports of the chip, listed at boot with no role yet. caps is what the silicon can do. A port may be locked to device or host; the others stay free. The PHY stays off, and its pins stay free, until a role is chosen. One task applies connect, reset, suspend and fault events. No endpoints and no classes.",
	define: "SUPPORT_COMMON_USB",
	sources: ["usb.c"],
	headers: ["usb.h"],
	settings: [
		{ name: "MP_USB_PORT_MAX", label: "PHY ports", type: "number", default: 4, min: 1, max: 8 },
		{ name: "MP_USB_EVENTS", label: "Queued link events per port", type: "number", default: 8, min: 2, max: 32 },
	],
}
