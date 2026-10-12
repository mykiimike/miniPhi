import { createContext, useContext } from "react"

export const Config = createContext(null)

export function useConfig() {
	return useContext(Config)
}

export function parsePin(pin) {
	const match = /^p(\d+)\.(\d)$/.exec(pin || "")
	return match ? { port: Number(match[1]), bit: Number(match[2]) } : null
}

export function bytes(value) {
	if (value >= 1024 && value % 1024 === 0)
		return value / 1024 + " KB"
	if (value >= 1024)
		return (value / 1024).toFixed(1) + " KB"
	return value + " B"
}

export function hue(name) {
	let hash = 0
	for (const char of String(name))
		hash = (hash * 31 + char.charCodeAt(0)) % 360
	return (hash * 7) % 360
}

export function nextName(selection, base) {
	const taken = new Set((selection.devices || []).map((device) => device.name))
	for (let count = 1; ; count++)
		if (!taken.has(base + count))
			return base + count
}
