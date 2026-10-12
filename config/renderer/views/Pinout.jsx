import { hue, nextName, parsePin, useConfig } from "../state.js"
import { Badge, DeviceCard, Empty, Section } from "../components/ui.jsx"

const appHue = 212

function ownerHue(owner) {
	return owner.driver ? hue(owner.driver) : appHue
}

function gateFunctions(mcu) {
	const functions = {}
	for (const [gate, def] of Object.entries(mcu.gates || {}))
		for (const [bus, pins] of Object.entries(def.pins || {}))
			for (const [signal, pin] of Object.entries(pins))
				(functions[pin] = functions[pin] || []).push(gate + " " + bus.toUpperCase() + " " + signal.toUpperCase())
	return functions
}

function Cell({ pin, functions }) {
	const { analysis, selection, ui, setUi } = useConfig()
	const owners = analysis.pins[pin] || []
	const conflict = owners.length > 1
	const owner = owners[0]

	let className = "pin"
	let style
	let tag = null
	let signal = null
	if (conflict) {
		className += " conflict"
		tag = owners.length + " owners"
	}
	else if (owner && owner.kind === "reserved") {
		className += " reserved"
		tag = "system"
	}
	else if (owner) {
		className += " used"
		style = { "--hue": ownerHue(owner) }
		const device = selection.devices[owner.device]
		tag = device ? device.name : owner.label
		signal = owner.option === "port" ? null : owner.optionLabel
	}
	else if (functions[pin])
		className += " fn"
	if (ui.pin === pin)
		className += " selected"

	const title = [pin.toUpperCase(), ...owners.map((item) => item.label + (item.optionLabel ? " · " + item.optionLabel : "")), ...(functions[pin] || [])].join("\n")

	return (
		<button className={className} style={style} title={title} onClick={() => setUi({ pin })}>
			<span className="bit">{parsePin(pin).bit}</span>
			{tag ? <span className="tag">{tag}</span> : null}
			{signal ? <span className="signal">{signal}</span> : null}
		</button>
	)
}

function Detail({ functions }) {
	const { analysis, ui, go } = useConfig()
	const mcu = analysis.target.mcu
	if (!ui.pin || !mcu.pins.includes(ui.pin))
		return <div className="card muted">Click a pin to see who owns it.</div>

	const pin = ui.pin
	const { port } = parsePin(pin)
	const owners = analysis.pins[pin] || []
	const irq = (mcu.irqPorts || []).includes(port)

	return (
		<div className="card pin-detail">
			<h3>{pin.toUpperCase()}</h3>
			<dl className="kv">
				<dt>Interrupt</dt>
				<dd>{irq ? "Yes, port " + port + " has a vector" : "No"}</dd>
				<dt>Functions</dt>
				<dd>{functions[pin] ? functions[pin].join(", ") : "GPIO only"}</dd>
			</dl>
			<div className="side-label dark">Owners</div>
			{owners.length ? (
				<ul className="owners">
					{owners.map((owner, index) => (
						<li className="owner" key={index}>
							<i className="swatch" style={{ background: owner.kind === "reserved" ? "#bdb7ab" : "hsl(" + ownerHue(owner) + " 55% 60%)" }} />
							<span>{owner.label}{owner.optionLabel ? " · " + owner.optionLabel : ""}</span>
							{owner.driver ? (
								<button className="btn small ghost" onClick={() => go("drivers", { driver: owner.driver })}>Open</button>
							) : null}
						</li>
					))}
				</ul>
			) : <p className="muted">Free.</p>}
			{owners.length > 1 ? <p className="field-error">Two owners on one pin. Move one of them.</p> : null}
		</div>
	)
}

function Legend() {
	const { analysis } = useConfig()
	const devices = analysis.devices.filter((device) => device.schema.some((option) => option.type === "pin" && device.values[option.name]))

	return (
		<div className="card legend">
			{devices.map((device) => (
				<div className="legend-row" key={device.index}>
					<i className="swatch" style={{ background: "hsl(" + (device.driver ? hue(device.driver) : appHue) + " 55% 60%)" }} />
					{device.label}
				</div>
			))}
			<div className="legend-row"><i className="swatch reserved" />Reserved by the kernel or the board</div>
			<div className="legend-row"><i className="swatch conflict" />Conflict</div>
			<div className="legend-row"><i className="swatch fn" />Free, wired to a gate</div>
		</div>
	)
}

function Gates() {
	const { analysis } = useConfig()
	const mcu = analysis.target.mcu

	return (
		<div className="card flush">
			<table className="gates">
				<thead>
					<tr><th>Gate</th><th>Can run</th><th>Owner</th><th /></tr>
				</thead>
				<tbody>
					{Object.entries(mcu.gates || {}).map(([name, gate]) => {
						const owners = analysis.gates[name] || []
						return (
							<tr key={name}>
								<td><code>{name}</code></td>
								<td>{gate.kinds.map((kind) => kind.toUpperCase()).join(" / ")}</td>
								<td>{owners.map((owner) => owner.label).join(", ") || <span className="faint">Free</span>}</td>
								<td className="right">
									{owners.length > 1
										? <Badge tone="err">Conflict</Badge>
										: owners.length ? <Badge tone={owners[0].kind === "reserved" ? "" : "ok"}>{owners[0].kind === "reserved" ? "Reserved" : "In use"}</Badge> : null}
								</td>
							</tr>
						)
					})}
				</tbody>
			</table>
		</div>
	)
}

export function Pinout() {
	const { analysis, catalog, selection, change } = useConfig()
	if (!analysis.target)
		return <Empty>Pick a target first.</Empty>

	const mcu = analysis.target.mcu
	const functions = gateFunctions(mcu)
	const ports = new Map()
	for (const pin of mcu.pins) {
		const { port, bit } = parsePin(pin)
		if (!ports.has(port))
			ports.set(port, new Map())
		ports.get(port).set(bit, pin)
	}

	const arch = catalog.archs.find((record) => record.name === selection.arch)
	const buses = arch ? arch.data.buses || {} : {}
	const appIndexes = selection.devices
		.map((device, index) => (device.driver ? -1 : index))
		.filter((index) => index >= 0)

	const addApp = (bus) => change((draft) => {
		draft.devices = draft.devices || []
		draft.devices.push({ bus, name: nextName(draft, bus), options: {} })
	})

	const used = Object.values(analysis.pins).filter((owners) => owners.length).length

	return (
		<>
			<Section title="Pins" subtitle={used + " of " + mcu.pins.length + " pins claimed"}>
				<div className="pinout">
					<div className="card">
						<div className="pins">
							<div />
							{[0, 1, 2, 3, 4, 5, 6, 7].map((bit) => <div className="head" key={bit}>bit {bit}</div>)}
							{[...ports].map(([port, bits]) => [
								<div className="port" key={"p" + port}>
									P{port}
									{(mcu.irqPorts || []).includes(port) ? <span className="irq">IRQ</span> : null}
								</div>,
								...[0, 1, 2, 3, 4, 5, 6, 7].map((bit) => bits.has(bit)
									? <Cell key={port + "." + bit} pin={bits.get(bit)} functions={functions} />
									: <div key={port + "." + bit} className="pin none" />),
							])}
						</div>
					</div>
					<div className="stack">
						<Detail functions={functions} />
						<Legend />
					</div>
				</div>
			</Section>

			<Section
				title="Application peripherals"
				subtitle="Buses your main.c opens itself. They land in build/pinout.h as PHI_<NAME>."
				tools={(
					<div className="tools">
						{Object.entries(buses).map(([bus, def]) => (
							<button key={bus} className="btn small" onClick={() => addApp(bus)}>+ {def.label}</button>
						))}
					</div>
				)}
			>
				{appIndexes.length
					? appIndexes.map((index) => (
						<DeviceCard key={index} index={index} kind={buses[selection.devices[index].bus] ? buses[selection.devices[index].bus].label : selection.devices[index].bus} />
					))
					: <Empty>No application peripheral.</Empty>}
			</Section>

			<Section title="Gates" subtitle="A gate has a single owner at a time.">
				<Gates />
			</Section>
		</>
	)
}
