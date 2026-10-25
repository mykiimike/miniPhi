export const phi = {
	kind: "module",
	name: "RESISTIVE",
	label: "4-wire resistive touch",
	description: "A resistive sheet with no controller chip. The board names the glass (pins, size, calibration, orientation). This module drives the plates, reads two ADC channels and registers the panel as a touch device. Sampling starts with the first subscriber.",
	define: "SUPPORT_COMMON_RESISTIVE",
	sources: ["resistive.c"],
	headers: ["resistive.h"],
	requires: ["DEVICE"],
}
