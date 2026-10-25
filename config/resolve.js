const identifier = /^[a-z_][a-z0-9_]*$/
const defineName = /^[A-Za-z_][A-Za-z0-9_]*(=.*)?$/

/* p1.2 on MSP430 (numbered ports), pa12 on STM32 (lettered ports) */
/** pinout.h aliases PHI_<DRIVER>_<RANK> to the devices of a driver, in board order */
export const deviceRanks = ["first", "second", "third", "fourth"]

export function parsePin(pin) {
	const numbered = /^p(\d+)\.(\d)$/.exec(pin || "")
	if (numbered)
		return { port: Number(numbered[1]), bit: Number(numbered[2]) }
	const lettered = /^p([a-k])(\d{1,2})$/.exec(pin || "")
	return lettered ? { port: lettered[1].toUpperCase(), bit: Number(lettered[2]) } : null
}

/* irqPorts: whole ports have a vector. irqLines: any pin can, one per line number (STM32 EXTI) */
export function pinHasIrq(mcu, pin) {
	const parsed = parsePin(pin)
	return !!parsed && (!!mcu.irqLines || (mcu.irqPorts || []).includes(parsed.port))
}

function clone(value) {
	return JSON.parse(JSON.stringify(value))
}

export function emptySelection(catalog, previous) {
	const arch = catalog.archs[0]
	const series = arch && catalog.series.find((record) => record.data.arch === arch.name)
	return {
		board: null,
		arch: arch ? arch.name : null,
		series: series ? series.name : null,
		mcu: series && series.data.mcus && series.data.mcus[0] ? series.data.mcus[0].name : null,
		toolchain: arch && arch.data.toolchains && arch.data.toolchains[0] ? arch.data.toolchains[0].name : null,
		vars: { ...((previous && previous.vars) || {}) },
		modules: [],
		drivers: [],
		settings: {},
		defines: [],
		devices: [],
		app: null,
		sources: [],
		probes: [],
	}
}

export function boardSelection(catalog, board, previous) {
	const data = board.data
	return {
		board: board.name,
		arch: data.arch || null,
		series: data.series || null,
		mcu: data.mcu || null,
		toolchain: data.toolchain || null,
		vars: { ...((previous && previous.vars) || {}) },
		modules: [...(data.modules || [])],
		drivers: [...(data.drivers || [])],
		settings: { ...(data.settings || {}) },
		defines: [...(data.defines || [])],
		devices: clone(data.devices || []),
		app: typeof data.app === "string" ? data.app : null,
		sources: [...(data.sources || [])],
		probes: [...(data.requires || [])],
	}
}

export function migrateSelection(selection) {
	const probes = selection.probes || []
	if (Array.isArray(selection.app))
		return { ...selection, app: null, sources: [...(selection.sources || []), ...selection.app], probes }
	return { ...selection, app: selection.app || null, sources: selection.sources || [], probes }
}

export function neededDevices(catalog, selection) {
	const app = catalog.apps.find((record) => record.name === selection.app)
	const devices = [...(selection.devices || [])]
	for (const need of (app && app.data.devices) || [])
		if (!devices.some((device) => !device.driver && device.bus === need.bus && device.name === need.name))
			devices.push({ bus: need.bus, name: need.name, options: {} })
	return devices
}

function validSetting(setting, value) {
	if (setting.type === "bool")
		return typeof value === "boolean"
	if (setting.type === "enum")
		return (setting.values || []).includes(value)
	if (setting.type === "number") {
		const number = Number(value)
		if (!Number.isInteger(number))
			return false
		if (setting.min !== undefined && number < setting.min)
			return false
		if (setting.max !== undefined && number > setting.max)
			return false
		return true
	}
	return true
}

function isEmpty(value) {
	return value === undefined || value === null || value === ""
}

export function analyze(catalog, sel, exists) {
	const issues = []
	const issue = (level, view, text, refs = []) => issues.push({ level, view, text, refs })

	for (const problem of catalog.problems)
		issue(problem.level, "issues", problem.text)

	const arch = catalog.archs.find((record) => record.name === sel.arch)
	const series = arch && catalog.series.find((record) => record.data.arch === arch.name && record.name === sel.series)
	const mcu = series && (series.data.mcus || []).find((item) => item.name === sel.mcu)
	const toolchain = arch && (arch.data.toolchains || []).find((item) => item.name === sel.toolchain)

	if (!arch)
		issue("error", "target", "Pick an architecture.")
	else if (!series)
		issue("error", "target", "Pick a series for " + arch.data.label + ".")
	else if (!mcu)
		issue("error", "target", "Pick a microcontroller.")
	if (arch && !toolchain)
		issue("error", "target", "Pick a toolchain.")
	if (!catalog.kernel)
		issue("error", "kernel", "common/_phi.js is missing.")
	if (!arch || !series || !mcu || !toolchain || !catalog.kernel)
		return { target: null, issues }

	const kernel = catalog.kernel
	const modules = new Map(catalog.modules.map((record) => [record.name, record]))
	const drivers = new Map(catalog.drivers.map((record) => [record.name, record]))

	const driverState = new Map()
	for (const name of sel.drivers || []) {
		if (!drivers.has(name))
			issue("warning", "drivers", "The selection names a driver with no _phi.js: " + name)
		else
			driverState.set(name, { state: "selected", by: new Set() })
	}
	for (const device of sel.devices || []) {
		if (!device.driver || !drivers.has(device.driver))
			continue
		if (!driverState.has(device.driver))
			driverState.set(device.driver, { state: "auto", by: new Set() })
		if (driverState.get(device.driver).state === "auto")
			driverState.get(device.driver).by.add(device.name)
	}

	const app = sel.app ? catalog.apps.find((record) => record.name === sel.app) : null
	if (sel.app && !app)
		issue("error", "app", "No app/" + sel.app + "/_phi.js.")
	for (const name of (app && app.data.drivers) || []) {
		if (!drivers.has(name)) {
			issue("error", "app", app.data.label + " needs the driver " + name + ", which has no _phi.js.")
			continue
		}
		if (!driverState.has(name))
			driverState.set(name, { state: "auto", by: new Set() })
		driverState.get(name).by.add(app.data.label)
	}

	const moduleState = new Map()
	const want = (name, by) => {
		const record = modules.get(name)
		if (!record) {
			issue("error", "kernel", (by || "The selection") + " needs " + name + ", which has no _phi.js.")
			return null
		}
		let entry = moduleState.get(name)
		if (!entry) {
			entry = { state: record.data.required ? "required" : "auto", by: new Set() }
			moduleState.set(name, entry)
			for (const dependency of record.data.requires || [])
				want(dependency, record.data.label || name)
		}
		if (by)
			entry.by.add(by)
		return entry
	}

	for (const dependency of kernel.data.requires || [])
		want(dependency, kernel.data.label)
	for (const record of catalog.modules)
		if (record.data.required)
			want(record.name, null)
	for (const name of sel.modules || []) {
		if (!modules.has(name)) {
			issue("warning", "kernel", "The selection names a module with no _phi.js: " + name)
			continue
		}
		const entry = want(name, null)
		if (entry.state === "auto")
			entry.state = "selected"
	}
	for (const dependency of (app && app.data.requires) || [])
		want(dependency, app.data.label)
	for (const [name] of driverState)
		for (const dependency of drivers.get(name).data.requires || [])
			want(dependency, drivers.get(name).data.label)

	const activeModules = catalog.modules.filter((record) => moduleState.has(record.name))
	const activeDrivers = catalog.drivers.filter((record) => driverState.has(record.name))

	/* accelerators: { SHA256: { sources, headers } } on the series; an MCU entry replaces it, null removes it */
	const accelerated = { ...(series.data.accelerators || {}), ...(mcu.accelerators || {}) }
	const accelerators = activeModules
		.filter((record) => accelerated[record.name])
		.map((record) => ({
			module: record.name,
			label: mcu.label + " " + (record.data.label || record.name) + " accelerator",
			define: "MP_ARCH_" + record.name,
			sources: accelerated[record.name].sources || [],
			headers: accelerated[record.name].headers || [],
		}))
	/* the directory under common/crypto/ is the class: hash, mac, cipher, kdf, rng, sign */
	const cryptoFamilyLabel = { hash: "Hash", mac: "MAC", cipher: "Cipher", kdf: "KDF", rng: "RNG", sign: "Signature", mode: "Mode" }
	const cryptoFamily = (dir) => {
		const parts = (dir || "").split("/")
		const at = parts.indexOf("crypto")
		const name = at >= 0 ? parts[at + 1] : ""
		if (!name)
			return { name: "other", label: "Other" }
		const label = cryptoFamilyLabel[name] || name.charAt(0).toUpperCase() + name.slice(1)
		return { name, label }
	}
	/* crypto modules linked in this firmware, plus any block the chip declares */
	const crypto = []
	const cryptoSeen = new Set()
	for (const record of catalog.modules) {
		const hardware = !!accelerated[record.name]
		const built = moduleState.has(record.name)
		/* common/crypto/<class>/<algorithm>; the class directory itself holds the common API */
		if (!record.dir.startsWith("common/crypto/") || record.dir.split("/").length < 4 || (!built && !hardware))
			continue
		cryptoSeen.add(record.name)
		const family = cryptoFamily(record.dir)
		crypto.push({
			name: record.name,
			label: record.data.label || record.name,
			family: family.name,
			familyLabel: family.label,
			hardware,
			built,
		})
	}
	for (const [name, spec] of Object.entries(accelerated)) {
		if (!spec || cryptoSeen.has(name))
			continue
		crypto.push({ name, label: name, family: "other", familyLabel: "Other", hardware: true, built: false })
	}

	const declared = []
	const declare = (list, owner, view) => {
		for (const setting of list || [])
			declared.push({ ...setting, owner, view })
	}
	declare(kernel.data.settings, kernel.data.label, "kernel")
	for (const record of activeModules)
		declare(record.data.settings, record.data.label, "kernel")
	declare(series.data.settings, series.data.label, "target")
	declare(toolchain.settings, toolchain.label, "target")
	if (app)
		declare(app.data.settings, app.data.label, "app")

	const settingNames = new Set()
	const settings = declared.map((setting) => {
		if (settingNames.has(setting.name))
			issue("error", setting.view, setting.name + " is declared twice.")
		settingNames.add(setting.name)

		const explicit = !!sel.settings && !isEmpty(sel.settings[setting.name])
		const value = explicit ? sel.settings[setting.name] : setting.default
		const valid = validSetting(setting, value)
		if (!valid) {
			const range = setting.type === "number" ? " (" + setting.min + " to " + setting.max + ")" : ""
			issue("error", setting.view, setting.owner + ": " + setting.label + " has an invalid value" + range + ".", [{ setting: setting.name }])
		}
		return {
			name: setting.name,
			label: setting.label,
			type: setting.type,
			unit: setting.unit || null,
			values: setting.values || null,
			labels: setting.labels || null,
			min: setting.min,
			max: setting.max,
			default: setting.default,
			value,
			explicit,
			valid,
			owner: setting.owner,
			view: setting.view,
			emit: setting.emit || "header",
			ram: !!setting.ram,
		}
	})
	const settingValue = (name) => {
		const found = settings.find((setting) => setting.name === name)
		return found ? found.value : undefined
	}

	const pins = {}
	const gates = {}
	const usePin = (pin, owner) => (pins[pin] = pins[pin] || []).push(owner)
	const useGate = (gate, owner) => (gates[gate] = gates[gate] || []).push(owner)

	const board = catalog.boards.find((record) => record.name === sel.board)
	const reservedPins = [
		...(series.data.reserved || []),
		...(mcu.reserved || []),
		...((board && board.data.reserved) || []),
	]
	for (const reserved of reservedPins) {
		if (reserved.when) {
			const negated = reserved.when.startsWith("!")
			const value = !!settingValue(negated ? reserved.when.slice(1) : reserved.when)
			if (value === negated)
				continue
		}
		const owner = { kind: "reserved", label: reserved.who }
		if (reserved.pin)
			usePin(reserved.pin, owner)
		if (reserved.gate)
			useGate(reserved.gate, owner)
	}

	const buses = arch.data.buses || {}
	const macros = new Map()
	const irqLines = new Map()

	const devices = (sel.devices || []).map((device, index) => {
		const driver = device.driver ? drivers.get(device.driver) : null
		const view = device.driver ? "drivers" : "pinout"
		const options = device.options || {}
		const nameRef = [{ device: index, option: "name" }]

		if (device.driver && !driver)
			issue("error", "drivers", "Device " + device.name + " uses " + device.driver + ", which has no _phi.js.", nameRef)

		const schema = []
		let bus = device.bus || null
		if (driver) {
			const list = driver.data.buses || []
			bus = list[0] || null
			if (list.length > 1 && driver.data.busOption) {
				const picked = options[driver.data.busOption]
				schema.push({
					name: driver.data.busOption,
					label: "Bus",
					type: "enum",
					values: list,
					labels: list.map((name) => (buses[name] ? buses[name].label : name)),
					default: list[0],
				})
				if (list.includes(picked))
					bus = picked
			}
		}
		if (bus) {
			if (!buses[bus])
				issue("error", view, arch.data.label + " has no " + bus + " bus.", nameRef)
			else
				schema.push(...buses[bus].options)
		}
		if (driver)
			schema.push(...(driver.data.options || []))

		const valueOf = (option) => (isEmpty(options[option.name]) ? option.default : options[option.name])
		const active = schema.filter((option) => !option.when || Object.entries(option.when).every(([key, wanted]) => {
			const source = schema.find((item) => item.name === key)
			return (source ? valueOf(source) : options[key]) === wanted
		}))

		const kindLabel = driver ? driver.data.label : (buses[bus] ? buses[bus].label : bus || "Device")
		const label = kindLabel + " " + (device.name || "?")
		const kind = driver ? "device" : "app"

		if (!identifier.test(device.name || ""))
			issue("error", view, label + ": names use a-z, 0-9 and _.", nameRef)
		const macro = "PHI_" + (driver ? driver.name + "_" : "") + String(device.name || "").toUpperCase()
		if (driver && deviceRanks.includes(String(device.name).toLowerCase()))
			issue("error", view, macro + " is reserved for the " + String(device.name).toLowerCase() + " " + driver.name + " device.", nameRef)
		else if (macros.has(macro))
			issue("error", view, macro + " names two devices.", nameRef)
		else
			macros.set(macro, index)

		const values = {}
		for (const option of active) {
			let value = valueOf(option)
			const refs = [{ device: index, option: option.name }]
			if (isEmpty(value)) {
				if (option.required)
					issue("error", view, label + ": " + option.label + " is required.", refs)
				continue
			}

			if (option.type === "bool")
				value = value === true || value === "true"

			if (option.type === "pin") {
				const parsed = parsePin(value)
				if (!parsed || !mcu.pins.includes(value)) {
					issue("error", view, label + ": " + value + " does not exist on " + mcu.label + ".", refs)
				}
				else {
					if (option.irq && !pinHasIrq(mcu, value))
						issue("error", view, label + ": " + option.label + " needs an interrupt pin. Only P" + (mcu.irqPorts || []).join(" and P") + " have one.", refs)
					if (option.irq && mcu.irqLines) {
						const other = irqLines.get(parsed.bit)
						if (other && other.pin !== value)
							issue("error", view, label + ": " + option.label + " on " + value + " shares interrupt line " + parsed.bit + " with " + other.label + " on " + other.pin + ".", refs)
						else
							irqLines.set(parsed.bit, { pin: value, label })
					}
					usePin(value, { kind, label, device: index, option: option.name, optionLabel: option.label, driver: device.driver || null })
				}
			}

			if (option.type === "gate") {
				const gate = mcu.gates && mcu.gates[value]
				if (!gate)
					issue("error", view, label + ": " + value + " does not exist on " + mcu.label + ".", refs)
				else {
					if (option.gateKind && !gate.kinds.includes(option.gateKind))
						issue("error", view, label + ": " + value + " cannot run " + (buses[bus] ? buses[bus].label : option.gateKind) + ".", refs)
					useGate(value, { kind, label, device: index, driver: device.driver || null })
					for (const pin of gate.claims || [])
						usePin(pin, { kind, label: label + " (" + value + ")", device: index, option: option.name, optionLabel: option.label, driver: device.driver || null })
				}
			}

			if (option.type === "enum" && !(option.values || []).includes(String(value)))
				issue("error", view, label + ": " + option.label + " cannot be " + value + ".", refs)

			values[option.name] = value
		}

		const gate = values.gate && mcu.gates ? mcu.gates[values.gate] : null
		const fixed = gate && gate.pins ? gate.pins[bus] : null
		if (fixed) {
			for (const [option, pin] of Object.entries(fixed)) {
				if (values[option] === undefined || values[option] === pin)
					continue
				const optionLabel = (active.find((item) => item.name === option) || { label: option }).label
				issue("error", view, label + ": " + values.gate + " " + optionLabel + " is wired to " + pin + ", not " + values[option] + ".", [{ device: index, option }])
			}
		}

		return {
			index,
			kind,
			driver: device.driver || null,
			bus,
			name: device.name,
			label,
			macro,
			view,
			schema: active,
			values,
		}
	})

	for (const [pin, owners] of Object.entries(pins)) {
		if (owners.length < 2)
			continue
		const refs = owners.filter((owner) => owner.device !== undefined).map((owner) => ({ device: owner.device, option: owner.option }))
		issue("error", "pinout", pin + " is claimed by " + owners.map((owner) => owner.label).join(" and ") + ".", refs)
	}
	for (const [gate, owners] of Object.entries(gates)) {
		if (owners.length < 2)
			continue
		const refs = owners.filter((owner) => owner.device !== undefined).map((owner) => ({ device: owner.device, option: "gate" }))
		issue("error", "pinout", gate + " is claimed by " + owners.map((owner) => owner.label).join(" and ") + ". A gate has one owner.", refs)
	}

	const sources = []
	const addSources = (list, owner) => {
		for (const source of list || []) {
			if (sources.includes(source))
				continue
			sources.push(source)
			if (!exists(source))
				issue("error", "build", owner + ": " + source + " is missing.")
		}
	}
	addSources(kernel.data.sources, kernel.data.label)
	for (const record of activeModules)
		addSources(record.data.sources, record.data.label)
	addSources(arch.data.sources, arch.data.label)
	addSources(series.data.boot, series.data.label)
	for (const accelerator of accelerators)
		addSources(accelerator.sources, accelerator.label)
	for (const record of activeDrivers)
		addSources(record.data.sources, record.data.label)
	if (app)
		addSources(app.data.sources, app.data.label)
	if (board)
		addSources(board.data.sources, board.data.label)
	addSources(sel.sources, "Extra sources")

	const needs = ((app && app.data.devices) || []).map((need) => {
		const index = (sel.devices || []).findIndex((device) => !device.driver && device.bus === need.bus && device.name === need.name)
		if (index < 0)
			issue("error", "app", app.data.label + " needs a " + (buses[need.bus] ? buses[need.bus].label : need.bus) + " named " + need.name + ".")
		return { ...need, index }
	})

	const headers = []
	const links = new Map([["autoconf.h", "the configurator"], ["pinout.h", "the configurator"]])
	const addHeaders = (record) => {
		for (const file of record.data.headers || []) {
			const link = (record.data.headerDir ? record.data.headerDir + "/" : "") + file.split("/").pop()
			if (!exists(file))
				issue("error", "build", record.data.label + ": header " + file + " is missing.")
			if (links.has(link)) {
				issue("error", "build", "include/" + link + " is declared by " + links.get(link) + " and " + record.data.label + ".")
				continue
			}
			links.set(link, record.data.label)
			headers.push({ link: "include/" + link, target: file, owner: record.data.label })
		}
	}
	addHeaders(kernel)
	for (const record of activeModules)
		addHeaders(record)
	addHeaders(arch)
	addHeaders(series)
	for (const accelerator of accelerators)
		addHeaders({ data: { label: accelerator.label, headers: accelerator.headers, headerDir: series.data.headerDir } })
	for (const record of activeDrivers)
		addHeaders(record)
	if (app)
		addHeaders(app)
	if (board)
		addHeaders(board)

	if (!app && !(sel.sources || []).length)
		issue("warning", "app", "No application. The firmware has no main().")
	if (toolchain.linkerScript && mcu.ld && !exists(mcu.ld))
		issue("error", "target", "Linker script " + mcu.ld + " is missing.")

	for (const define of sel.defines || [])
		if (!defineName.test(define))
			issue("error", "build", "Define " + define + " is not a valid macro name.")

	const vars = (toolchain.vars || []).map((item) => {
		const value = sel.vars && !isEmpty(sel.vars[item.name]) ? sel.vars[item.name] : (item.default || "")
		if (isEmpty(value))
			issue("error", "target", toolchain.label + ": set " + item.name + ".", [{ variable: item.name }])
		return { ...item, value }
	})

	const rsaBits = settingValue("MP_RSA_BITS")
	if (rsaBits !== undefined && Number(rsaBits) % 64 !== 0)
		issue("error", "kernel", "RSA modulus must be a multiple of 64 bits.", [{ setting: "MP_RSA_BITS" }])

	/* RSA and the post-quantum signature families are mutually exclusive */
	const pqNames = ["LMS", "SLH", "MLDSA"]
	const activePq = pqNames.filter((name) => moduleState.has(name))
	if (moduleState.has("RSA") && activePq.length)
		issue("error", "kernel", "RSA and post-quantum signatures (" + activePq.join(", ") + ") are mutually exclusive. Pick one family.", activePq.map((name) => ({ module: name })).concat([{ module: "RSA" }]))

	const ramItems = settings
		.filter((setting) => setting.ram && setting.valid)
		.map((setting) => ({ label: setting.label, owner: setting.owner, bytes: Number(setting.value) }))
	const ramUsed = ramItems.reduce((sum, item) => sum + item.bytes, 0)
	if (ramUsed > mcu.ram)
		issue("error", "kernel", "Declared RAM is " + ramUsed + " bytes, " + mcu.label + " has " + mcu.ram + ".")

	const strip = (record) => ({ name: record.name, label: record.data.label, description: record.data.description, path: record.path })

	const makeTargets = new Map([["all", "the configurator"], ["clean", "the configurator"]])
	const probes = []
	for (const name of sel.probes || []) {
		const record = catalog.probes.find((item) => item.name === name)
		if (!record) {
			issue("error", "target", (board ? board.data.label : "The selection") + " requires the probe " + name + ", which has no _phi.js.")
			continue
		}
		for (const file of record.data.files || [])
			if (!exists(file))
				issue("error", "build", record.data.label + ": " + file + " is missing.")
		const targets = []
		for (const target of record.data.targets || []) {
			if (!exists(target.script))
				issue("error", "build", record.data.label + ": " + target.script + " is missing.")
			if (makeTargets.has(target.name)) {
				issue("error", "build", "make " + target.name + " is declared by " + makeTargets.get(target.name) + " and " + record.data.label + ".")
				continue
			}
			makeTargets.set(target.name, record.data.label)
			targets.push({ name: target.name, label: target.label || target.name, script: target.script, args: target.args || "" })
		}
		probes.push({ ...strip(record), vendor: record.data.vendor || null, targets })
	}

	return {
		target: {
			arch: { ...strip(arch), define: arch.data.define },
			series: { ...strip(series), boot: series.data.boot },
			mcu,
			toolchain,
		},
		kernel: { ...strip(kernel), sources: kernel.data.sources },
		modules: catalog.modules.map((record) => {
			const entry = moduleState.get(record.name)
			return {
				...strip(record),
				define: record.data.define || null,
				requires: record.data.requires || [],
				sources: record.data.sources,
				required: !!record.data.required,
				state: entry ? entry.state : "off",
				by: entry ? [...entry.by] : [],
			}
		}),
		drivers: catalog.drivers.map((record) => {
			const entry = driverState.get(record.name)
			return {
				...strip(record),
				define: record.data.define || null,
				buses: record.data.buses || [],
				requires: record.data.requires || [],
				sources: record.data.sources,
				state: entry ? entry.state : "off",
				devices: devices.filter((device) => device.driver === record.name).length,
			}
		}),
		devices,
		settings,
		pins,
		gates,
		sources,
		defines: [arch.data.define, mcu.define, ...(sel.defines || [])].filter(Boolean),
		includes: ["include"],
		headers,
		headerDefines: [
			...activeModules.map((record) => record.data.define).filter(Boolean),
			...accelerators.map((accelerator) => accelerator.define),
			...activeDrivers.map((record) => record.data.define).filter(Boolean),
		],
		accelerators: accelerators.map(({ module, label, define }) => ({ module, label, define })),
		crypto,
		app: app ? {
			...strip(app),
			requires: app.data.requires || [],
			drivers: app.data.drivers || [],
			sources: app.data.sources,
			headers: app.data.headers,
			needs,
		} : null,
		extra: (sel.sources || []).map((source) => ({ path: source, exists: exists(source) })),
		probes,
		vars,
		ram: { total: mcu.ram, used: ramUsed, items: ramItems },
		issues,
	}
}
