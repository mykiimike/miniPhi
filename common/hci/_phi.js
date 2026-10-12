export const phi = {
	kind: "module",
	name: "HCI",
	label: "HCI",
	description: "Bluetooth HCI transport over a UART.",
	define: "SUPPORT_COMMON_HCI",
	sources: ["hci.c", "hci_cmds.c"],
	headers: ["hci.h"],
	requires: ["CIRCULAR"],
}
