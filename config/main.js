import { app, BrowserWindow, Menu, dialog, ipcMain } from "electron"
import fs from "fs"
import path from "path"
import { spawn } from "child_process"
import { fileURLToPath, pathToFileURL } from "url"
import { loadCatalog, publicCatalog } from "./catalog.js"
import { analyze, boardSelection, emptySelection, migrateSelection, neededDevices } from "./resolve.js"
import { boardSource, buildFiles, writeBuild } from "./generate.js"
import { runPrepare } from "./prepare.js"

const here = path.dirname(fileURLToPath(import.meta.url))
const root = path.resolve(here, "..")
const buildDir = path.join(root, "build")

let catalog = null
let window = null
let running = null

async function reload() {
	catalog = await loadCatalog(root)
	return publicCatalog(catalog)
}

async function savedSelection() {
	const file = path.join(buildDir, "config.js")
	if (!fs.existsSync(file))
		return null
	try {
		const mod = await import(pathToFileURL(file).href + "?v=" + Date.now())
		return mod.config && mod.config.arch ? migrateSelection(mod.config) : null
	}
	catch {
		return null
	}
}

const exists = (rel) => fs.existsSync(path.resolve(root, rel))

async function evaluate(selection) {
	const analysis = analyze(catalog, selection, exists)
	if (!analysis.target)
		return { analysis, checks: [], files: {} }
	const checks = await runPrepare(root, catalog, analysis, selection)
	const files = buildFiles(analysis, selection)
	return { analysis, checks, files }
}

ipcMain.handle("phi:init", async () => {
	const pub = await reload()
	const saved = await savedSelection()
	const board = catalog.boards[0]
	let selection = saved || (board ? boardSelection(catalog, board) : emptySelection(catalog))
	selection = { ...selection, devices: neededDevices(catalog, selection) }
	return { catalog: pub, selection, saved: !!saved, root }
})

ipcMain.handle("phi:reload", () => reload())

ipcMain.handle("phi:evaluate", (_event, selection) => evaluate(selection))

ipcMain.handle("phi:board", (_event, name, previous) => {
	const board = catalog.boards.find((record) => record.name === name)
	const selection = board ? boardSelection(catalog, board, previous) : emptySelection(catalog, previous)
	return { ...selection, devices: neededDevices(catalog, selection) }
})

ipcMain.handle("phi:write", async (_event, selection) => {
	const { analysis, files } = await evaluate(selection)
	if (!analysis.target || analysis.issues.some((item) => item.level === "error"))
		throw new Error("Fix the errors before writing build/.")
	return writeBuild(root, analysis, files)
})

ipcMain.handle("phi:make", (_event, target) => new Promise((resolve) => {
	if (running)
		return resolve({ code: null, busy: true })
	if (!fs.existsSync(path.join(buildDir, "Makefile")))
		return resolve({ code: null, missing: true })

	const send = (stream) => (chunk) => {
		if (window && !window.isDestroyed())
			window.webContents.send("phi:make-output", { stream, text: chunk.toString() })
	}
	let done = false
	const finish = (code) => {
		if (done)
			return
		done = true
		running = null
		resolve({ code })
	}

	running = spawn("make", target ? [target] : [], { cwd: buildDir, env: process.env })
	running.stdout.on("data", send("out"))
	running.stderr.on("data", send("err"))
	running.on("error", (error) => {
		send("err")(error.message + "\n")
		finish(-1)
	})
	running.on("close", finish)
}))

ipcMain.handle("phi:save-board", async (_event, { dir, label, selection, overwrite }) => {
	const match = /^([a-z0-9_-]+)\/([a-z0-9_-]+)$/.exec(dir || "")
	if (!match)
		throw new Error("Boards live in <vendor>/<board>, both in a-z, 0-9, - and _.")
	const [, vendorDir, name] = match
	const rel = "boards/" + vendorDir + "/" + name
	const sibling = [...catalog.boards, ...catalog.probes].find((record) => record.dir.startsWith("boards/" + vendorDir + "/") && record.data.vendor)
	const vendor = sibling ? sibling.data.vendor : vendorDir
	const file = path.join(root, rel, "_phi.js")
	if (fs.existsSync(file) && !overwrite)
		return { exists: true }
	fs.mkdirSync(path.dirname(file), { recursive: true })
	fs.writeFileSync(file, boardSource(rel, vendor, label, selection))
	return { path: rel + "/_phi.js", board: name, catalog: await reload() }
})

ipcMain.handle("phi:browse", async (_event, current) => {
	const result = await dialog.showOpenDialog(window, {
		properties: ["openDirectory"],
		defaultPath: current && fs.existsSync(current) ? current : undefined,
	})
	return result.canceled ? null : result.filePaths[0]
})

function createWindow() {
	window = new BrowserWindow({
		width: 1320,
		height: 860,
		minWidth: 1080,
		minHeight: 680,
		title: "miniPhi config",
		backgroundColor: "#f5f3ee",
		icon: path.join(here, "assets", "icon.png"),
		webPreferences: {
			preload: path.join(here, "preload.cjs"),
			contextIsolation: true,
			nodeIntegration: false,
		},
	})
	window.loadFile(path.join(here, "dist", "index.html"))
}

Menu.setApplicationMenu(null)
app.whenReady().then(createWindow)
app.on("window-all-closed", () => app.quit())
