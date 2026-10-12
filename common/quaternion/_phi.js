export const phi = {
	kind: "module",
	name: "QUATERNION",
	label: "Quaternions",
	description: "Orientation math for IMU fusion.",
	define: "SUPPORT_COMMON_QUATERNION",
	sources: ["quaternion.c"],
	headers: ["quaternion.h"],
}
