// One-shot: xiao-esp32c6 sysinfo with PUI on the PowerBread ST7735.
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
sel.modules = ["SERIAL", "DEVICE", "PUI", "SPI", "I2C"]
sel.drivers = ["LED", "ST7735", "INA3221"]
sel.devices = [
	{ driver: "LED", name: "light", options: { port: "pa15", reverse: true } },
	{ bus: "uart", name: "console", options: { gate: "USB", baudRate: "115200" } },
	{
		driver: "ST7735",
		name: "tft",
		options: { sck: "pa19", mosi: "pa18", dc: "pa20", reset: "pa21", orientation: "flip" },
	},
	{
		driver: "INA3221",
		name: "meter",
		options: {
			sda: "pa22", clk: "pa23", address: "0x40", shunt: "50", channels: "12",
			label1: "3V3", label2: "5V",
		},
	},
]
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
console.log("ok: PowerBread xiao-esp32c6 build written")
