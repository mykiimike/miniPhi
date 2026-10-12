import fs from "fs"
import path from "path"

function which(command) {
	if (command.includes("/"))
		return fs.existsSync(command) ? command : null
	for (const dir of (process.env.PATH || "").split(path.delimiter)) {
		const full = path.join(dir, command)
		try {
			fs.accessSync(full, fs.constants.X_OK)
			return full
		}
		catch {
			continue
		}
	}
	return null
}

function clone(value) {
	return JSON.parse(JSON.stringify(value))
}

export async function runPrepare(root, catalog, analysis, selection) {
	const checks = []
	const values = Object.fromEntries(analysis.vars.map((item) => [item.name, item.value]))
	const expand = (text) => text.replace(/\$\(([A-Z0-9_]+)\)/g, (_, name) => (values[name] !== undefined ? values[name] : ""))
	const varsIn = (text) => [...text.matchAll(/\$\(([A-Z0-9_]+)\)/g)].map((match) => match[1])

	const { arch, series, mcu, toolchain } = analysis.target
	const records = [
		catalog.archs.find((record) => record.name === arch.name),
		catalog.series.find((record) => record.data.arch === arch.name && record.name === series.name),
		catalog.kernel,
		...catalog.modules.filter((record) => analysis.modules.some((item) => item.name === record.name && item.state !== "off")),
		...catalog.drivers.filter((record) => analysis.drivers.some((item) => item.name === record.name && item.state !== "off")),
		catalog.apps.find((record) => analysis.app && record.name === analysis.app.name),
		catalog.boards.find((record) => record.name === selection.board),
	].filter((record) => record && record.prepare)

	for (const record of records) {
		const source = record.path
		const push = (level, text, hint, vars) => checks.push({ level, text, hint: hint || null, source, vars: vars || [] })
		const ctx = {
			root,
			selection: clone(selection),
			arch: clone(arch),
			series: clone(series),
			mcu: clone(mcu),
			toolchain: clone(toolchain),
			vars: { ...values },
			setting(name) {
				const found = analysis.settings.find((item) => item.name === name)
				return found ? found.value : undefined
			},
			requireTool(command, hint) {
				const found = which(expand(command))
				push(found ? "ok" : "error", found ? command + " is " + found : command + " is not in PATH", found ? null : hint, varsIn(command))
			},
			requireFile(file, hint) {
				const full = path.resolve(root, expand(file))
				const found = fs.existsSync(full)
				push(found ? "ok" : "error", found ? full : full + " is missing", found ? null : hint, varsIn(file))
			},
			info(text) {
				push("info", text)
			},
			warn(text, hint) {
				push("warning", text, hint)
			},
			error(text, hint) {
				push("error", text, hint)
			},
		}

		try {
			await record.prepare(ctx)
		}
		catch (error) {
			push("error", "prepare() threw: " + error.message)
		}
	}

	return checks
}
