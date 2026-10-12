export const phi = {
	kind: "module",
	name: "KALMAN",
	label: "Kalman filter",
	description: "One-dimensional Kalman filter for noisy sensor readings.",
	define: "SUPPORT_COMMON_KALMAN",
	sources: ["kalman.c"],
	headers: ["kalman.h"],
}
