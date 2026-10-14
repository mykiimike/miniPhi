export const phi = {
	kind: "module",
	name: "QUATERNION",
	label: "Quaternions",
	description: "Orientation fusion of accelerometer, gyroscope (rad/s) and optional magnetometer: Madgwick with gyroscope bias tracking, or Mahony PI. Primes from the first sample, falls back to 6 axes without magnetometer, gives quaternion and Euler angles. Gains default to MP_QUATERNION_BETA, _ZETA, _KP and _KI.",
	define: "SUPPORT_COMMON_QUATERNION",
	sources: ["quaternion.c"],
	headers: ["quaternion.h"],
}
