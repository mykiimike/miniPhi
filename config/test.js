/* Host tests declared by a module's _phi.js. They are not firmware sources. */

import { spawnSync } from "node:child_process"
import fs from "node:fs"
import os from "node:os"
import path from "node:path"
import { fileURLToPath } from "node:url"
import { loadCatalog } from "./catalog.js"

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..")
const include = [
	"-I" + path.join(root, "common", "test"),
	"-I" + path.join(root, "common"),
]
/* MP_TEST_SAN=1 adds ASan and UBSan. The default command line is unchanged. */
const san = process.env.MP_TEST_SAN === "1"
const cflags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pthread", ...include]
if (san)
	cflags.push("-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g")
const runEnv = { ...process.env }
if (san) {
	runEnv.UBSAN_OPTIONS = runEnv.UBSAN_OPTIONS
		? runEnv.UBSAN_OPTIONS + ":halt_on_error=1"
		: "halt_on_error=1"
	runEnv.ASAN_OPTIONS = runEnv.ASAN_OPTIONS
		? runEnv.ASAN_OPTIONS + ":halt_on_error=1"
		: "halt_on_error=1"
}

const catalog = await loadCatalog(root)
const records = [
	catalog.kernel,
	...catalog.modules,
	...catalog.drivers,
	...catalog.archs,
	...catalog.apps,
].filter(Boolean)

let ran = 0
let failed = 0

for (const record of records) {
	for (const rel of record.data.tests || []) {
		ran++
		const src = path.join(root, rel)
		const out = path.join(os.tmpdir(), "miniphi-" + path.basename(rel, ".c") + "-" + process.pid)
		if (!fs.existsSync(src)) {
			console.error(rel + " is missing")
			failed++
			continue
		}
		const compile = spawnSync("gcc", [
			...cflags, src, "-o", out, "-lm",
		], { encoding: "utf8" })
		if (compile.status !== 0) {
			console.error(rel)
			process.stderr.write(compile.stderr || compile.stdout || "")
			failed++
			continue
		}
		const run = spawnSync(out, [], { encoding: "utf8", env: runEnv })
		fs.rmSync(out, { force: true })
		if (run.status !== 0) {
			console.error(rel)
			process.stdout.write(run.stdout || "")
			process.stderr.write(run.stderr || "")
			failed++
			continue
		}
		console.log(rel + " ok")
	}
}

if (!ran) {
	console.error("no tests")
	process.exit(1)
}
if (failed) {
	console.error(failed + " failed")
	process.exit(1)
}
