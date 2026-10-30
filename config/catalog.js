import fs from "fs"
import path from "path"
import { pathToFileURL } from "url"

const roots = ["arch", "common", "drivers", "app", "boards"]
const skipped = new Set(["node_modules", "build", ".git"])
const kinds = ["kernel", "module", "driver", "arch", "series", "app", "board", "probe"]

function walk(dir, found) {
	const entries = fs.readdirSync(dir, { withFileTypes: true })
		.sort((a, b) => a.name.localeCompare(b.name))
	for (const entry of entries) {
		if (skipped.has(entry.name))
			continue
		const full = path.join(dir, entry.name)
		if (entry.isDirectory())
			walk(full, found)
		else if (entry.name === "_phi.js")
			found.push(full)
	}
}

function join(dir, rel) {
	return path.posix.normalize(path.posix.join(dir, rel))
}

function normalize(record) {
	const { kind, dir, data } = record
	const fix = (list) => (list || []).map((rel) => join(dir, rel))

	if (kind === "kernel" || kind === "module" || kind === "driver" || kind === "arch" || kind === "app" || kind === "board")
		data.sources = fix(data.sources)
	data.headers = fix(data.headers)
	data.tests = fix(data.tests)
	if (data.headerDir === undefined)
		data.headerDir = path.posix.basename(dir).toLowerCase()
	const fixAccelerators = (list) => {
		for (const accelerator of Object.values(list || {}).filter(Boolean)) {
			accelerator.sources = fix(accelerator.sources)
			accelerator.headers = fix(accelerator.headers)
		}
	}
	if (kind === "series") {
		data.boot = fix(data.boot)
		fixAccelerators(data.accelerators)
		for (const mcu of data.mcus || []) {
			mcu.ld = mcu.ld ? join(dir, mcu.ld) : null
			fixAccelerators(mcu.accelerators)
		}
	}
	if (kind === "probe") {
		data.files = fix(data.files)
		for (const target of data.targets || [])
			target.script = join(dir, target.script)
	}
}

export async function loadCatalog(root) {
	const files = []
	for (const name of roots) {
		const dir = path.join(root, name)
		if (fs.existsSync(dir))
			walk(dir, files)
	}

	const records = []
	const problems = []

	for (const file of files) {
		const rel = path.relative(root, file).split(path.sep).join("/")
		let mod
		try {
			mod = await import(pathToFileURL(file).href + "?v=" + Date.now())
		}
		catch (error) {
			problems.push({ level: "error", text: rel + " does not load: " + error.message })
			continue
		}

		if (!mod.phi || typeof mod.phi !== "object") {
			problems.push({ level: "error", text: rel + " does not export a phi object" })
			continue
		}

		const data = JSON.parse(JSON.stringify(mod.phi))
		if (!kinds.includes(data.kind)) {
			problems.push({ level: "error", text: rel + " has an unknown kind: " + data.kind })
			continue
		}

		const dir = path.posix.dirname(rel)
		if (data.kind === "driver") {
			const group = data.group
			if (!Array.isArray(group) || group.length === 0 || group.some((name) => typeof name !== "string" || name === ""))
				problems.push({ level: "error", text: rel + " group must be a list of roles." })
		}
		const record = {
			kind: data.kind,
			name: data.name || path.posix.basename(dir),
			path: rel,
			dir,
			data,
			prepare: typeof mod.prepare === "function" ? mod.prepare : null,
			generate: typeof mod.generate === "function" ? mod.generate : null,
		}
		normalize(record)
		records.push(record)
	}

	const seen = new Map()
	for (const record of records) {
		const key = record.kind + ":" + (record.kind === "series" ? record.data.arch + "/" : "") + record.name
		if (seen.has(key))
			problems.push({ level: "error", text: record.path + " reuses the name " + record.name + " from " + seen.get(key) })
		else
			seen.set(key, record.path)
	}

	const of = (kind) => records.filter((record) => record.kind === kind)
	const kernels = of("kernel")
	if (kernels.length > 1)
		problems.push({ level: "error", text: "More than one kernel record: " + kernels.map((r) => r.path).join(", ") })

	return {
		kernel: kernels[0] || null,
		modules: of("module"),
		drivers: of("driver"),
		archs: of("arch"),
		series: of("series"),
		apps: of("app"),
		boards: of("board"),
		probes: of("probe"),
		problems,
	}
}

export function publicCatalog(catalog) {
	const strip = ({ prepare, generate, ...record }) => ({ ...record, hasPrepare: !!prepare, hasGenerate: !!generate })
	return {
		kernel: catalog.kernel ? strip(catalog.kernel) : null,
		modules: catalog.modules.map(strip),
		drivers: catalog.drivers.map(strip),
		archs: catalog.archs.map(strip),
		series: catalog.series.map(strip),
		apps: catalog.apps.map(strip),
		boards: catalog.boards.map(strip),
		probes: catalog.probes.map(strip),
		problems: catalog.problems,
	}
}
