// One-shot: xiao-esp32c6 sysinfo with PUI on the SSD1306.
import fs from "fs"
import path from "path"
import { fileURLToPath } from "url"
import { loadCatalog } from "./catalog.js"
import { analyze, boardSelection, neededDevices } from "./resolve.js"
import { buildFiles, writeBuild } from "./generate.js"
import { runPrepare, runGenerate } from "./prepare.js"

const here = path.dirname(fileURLToPath(import.meta.url))
const root = path.resolve(here, "..")
const exists = (rel) => fs.existsSync(path.resolve(root, rel))

const catalog = await loadCatalog(root, [])
const board = catalog.boards.find((record) => record.name === "xiao-esp32c6")
if (!board) {
	console.error("missing board xiao-esp32c6")
	process.exit(1)
}

const sel = boardSelection(catalog, board)
if (!sel.modules.includes("PUI"))
	sel.modules.push("PUI")
if (!sel.modules.includes("I2C"))
	sel.modules.push("I2C")
sel.drivers = ["LED"]
if (!sel.devices.some((device) => device.driver === "SSD1306" && device.name === "oled")) {
	sel.devices.push({
		driver: "SSD1306",
		name: "oled",
		options: { sda: "pa22", clk: "pa23", address: "0x3C", contrast: 207 },
	})
}
if (!sel.devices.some((device) => device.driver === "PCF8563" && device.name === "rtc")) {
	sel.devices.push({
		driver: "PCF8563",
		name: "rtc",
		options: { address: "0x51" },
	})
}
sel.defines = ["MP_ARCH_RF_TEST"]
sel.app = "sysinfo"
sel.devices = neededDevices(catalog, sel)

const analysis = analyze(catalog, sel, exists)
if (analysis.issues.some((issue) => issue.level === "error")) {
	console.error("errors:")
	for (const issue of analysis.issues.filter((item) => item.level === "error"))
		console.error("  " + issue.view + ": " + issue.text)
	process.exit(1)
}

await runPrepare(root, catalog, analysis, sel)
const files = buildFiles(analysis, sel)
const generated = await runGenerate(root, catalog, analysis, sel)
writeBuild(root, analysis, { ...files, ...generated })
console.log("ok: PUI xiao-esp32c6 build written")
