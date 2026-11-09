// One-shot: regenerate the S3 build without TUI/UC8179 so the term shell
// glass can be 200x60 and the splash centers in the real window.
import path from "path"
import { fileURLToPath } from "url"
import { loadCatalog } from "./catalog.js"
import { analyze, boardSelection, neededDevices } from "./resolve.js"
import { buildFiles, writeBuild } from "./generate.js"
import { runPrepare, runGenerate } from "./prepare.js"

const here = path.dirname(fileURLToPath(import.meta.url))
const root = path.resolve(here, "..")
const exists = (rel) => fs.existsSync(path.resolve(root, rel))
import fs from "fs"

const catalog = await loadCatalog(root, [])
const board = catalog.boards.find((r) => r.name === "xiao-epaper")
const sel = boardSelection(catalog, board)

// drop the screen: TUI module, UC8179 driver, the panel device, font settings
sel.modules = sel.modules.filter((m) => m !== "TUI")
sel.drivers = sel.drivers.filter((d) => d !== "UC8179")
sel.devices = sel.devices.filter((d) => d.driver !== "UC8179")
delete sel.settings.MP_TUI_FONT
delete sel.settings.MP_TUI_FONT_SIZE
delete sel.settings.MP_TUI_FONT_BPP
delete sel.settings.MP_TUI_FONT_CHARSET
delete sel.settings.SYSINFO_SCREEN_COLUMNS

// unlimited glass: only the DRAM budget caps it now
sel.settings.MP_TERM_UI_COLS = 200
sel.settings.MP_TERM_UI_ROWS = 60

sel.app = "sysinfo"
sel.devices = neededDevices(catalog, sel)

const analysis = analyze(catalog, sel, exists)
if (analysis.issues.some((i) => i.level === "error")) {
	console.error("errors:")
	for (const i of analysis.issues.filter((x) => x.level === "error"))
		console.error("  " + i.view + ": " + i.text)
	process.exit(1)
}

await runPrepare(root, catalog, analysis, sel)
const files = buildFiles(analysis, sel)
const generated = await runGenerate(root, catalog, analysis, sel)
writeBuild(root, analysis, { ...files, ...generated })
console.log("ok: term-only S3 build written, glass 200x60")
