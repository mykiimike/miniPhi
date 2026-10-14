export const phi = {
	kind: "module",
	name: "HCI",
	label: "HCI",
	description: "Bluetooth HCI host over a UART (H4). Resets the controller and reads its buffers, keeps the command and ACL credits, cuts and reassembles L2CAP frames, and resets the controller again when the stream is lost. Give the UART rts and cts pins: RTS rises when the task falls behind. Both queues hold about 600 bytes of pool when full.",
	define: "SUPPORT_COMMON_HCI",
	sources: ["hci.c", "hci_cmds.c"],
	headers: ["hci.h"],
	requires: ["CIRCULAR"],
	settings: [
		{ name: "MP_HCI_POLL", label: "Timeout check period", type: "number", unit: "ms", default: 100, min: 1, max: 1000 },
		{ name: "MP_HCI_CMD_TIMEOUT", label: "Command timeout", type: "number", unit: "ms", default: 2000, min: 100, max: 30000 },
		{ name: "MP_HCI_CONNECTIONS", label: "Connections", type: "number", default: 2, min: 1, max: 8 },
		{ name: "MP_HCI_RX_SLACK", label: "Bytes the controller sends after RTS rises", type: "number", unit: "bytes", default: 32, min: 1, max: 128 },
		{ name: "MP_HCI_ACL_MAX", label: "Longest L2CAP frame", type: "number", unit: "bytes", default: 255, min: 27, max: 1024 },
	],
}
