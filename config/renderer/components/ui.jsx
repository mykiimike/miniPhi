import { parsePin, useConfig } from "../state.js"

export function Section({ title, subtitle, tools, children }) {
	return (
		<section className="section">
			<div className="section-head">
				<h2>{title}</h2>
				{subtitle ? <p>{subtitle}</p> : null}
				{tools ? <><div className="grow" />{tools}</> : null}
			</div>
			{children}
		</section>
	)
}

export function Chip({ children, tone }) {
	return <span className={"chip" + (tone ? " " + tone : "")}>{children}</span>
}

export function Badge({ children, tone }) {
	return <span className={"badge" + (tone ? " " + tone : "")}>{children}</span>
}

export function Choice({ active, title, badges, sub, meta, onClick }) {
	return (
		<button className={"choice" + (active ? " active" : "")} onClick={onClick}>
			<div className="choice-title">{title}{badges}</div>
			{sub ? <div className="choice-sub">{sub}</div> : null}
			{meta && meta.length ? (
				<div className="choice-meta">
					{meta.filter(Boolean).map((item) => <Chip key={item}>{item}</Chip>)}
				</div>
			) : null}
		</button>
	)
}

export function Placeholder({ title, sub }) {
	return (
		<div className="choice placeholder">
			<div className="choice-title">{title}</div>
			<div className="choice-sub">{sub}</div>
		</div>
	)
}

export function Empty({ children }) {
	return <div className="empty">{children}</div>
}

export function Switch(props) {
	return <input type="checkbox" className="switch" {...props} />
}

export function StatusIcon({ level }) {
	const glyph = { ok: "✓", error: "!", warning: "!", info: "i" }[level]
	return <span className={"icon " + level}>{glyph}</span>
}

export function issuesFor(analysis, device, option) {
	return analysis.issues.filter((item) => item.refs.some((ref) => ref.device === device && ref.option === option))
}

function short(text) {
	return text.replace(/^[^:]+: /, "")
}

export function SettingField({ setting }) {
	const { selection, change } = useConfig()
	const raw = selection.settings ? selection.settings[setting.name] : undefined
	const current = raw === undefined || raw === "" ? setting.default : raw

	const set = (value, debounce) => change((next) => {
		next.settings = next.settings || {}
		if (value === undefined)
			delete next.settings[setting.name]
		else
			next.settings[setting.name] = value
	}, { debounce })

	if (setting.type === "bool") {
		return (
			<label className="field check">
				<Switch checked={!!current} onChange={(event) => set(event.target.checked)} />
				<span className="field-label">{setting.label}</span>
				<code className="field-name">{setting.name}</code>
			</label>
		)
	}

	let input
	if (setting.type === "enum") {
		input = (
			<select value={String(current)} onChange={(event) => set(event.target.value)}>
				{setting.values.map((value, index) => (
					<option key={value} value={value}>{setting.labels ? setting.labels[index] : value}</option>
				))}
			</select>
		)
	}
	else {
		input = (
			<input
				type="number"
				value={raw === undefined ? setting.default : raw}
				min={setting.min}
				max={setting.max}
				onChange={(event) => {
					const value = event.target.value
					set(value === "" ? "" : Number(value), true)
				}}
			/>
		)
	}

	return (
		<label className={"field" + (setting.valid ? "" : " invalid")}>
			<span className="field-label">
				{setting.label}
				{setting.unit ? <em>{setting.unit}</em> : null}
				{setting.explicit && setting.value !== setting.default
					? <em className="changed" title={"Default: " + setting.default}>changed</em>
					: null}
			</span>
			{input}
			{setting.valid
				? <code className="field-name">{setting.name}</code>
				: <span className="field-error">{setting.type === "number" ? setting.min + " to " + setting.max : "Invalid value"}</span>}
		</label>
	)
}

function others(owners, device, option) {
	return (owners || []).filter((owner) => !(owner.device === device && (option === undefined || owner.option === option)))
}

function PinSelect({ index, option, value, onChange }) {
	const { analysis } = useConfig()
	const mcu = analysis.target.mcu
	const ports = new Map()
	for (const pin of mcu.pins) {
		const { port } = parsePin(pin)
		if (!ports.has(port))
			ports.set(port, [])
		ports.get(port).push(pin)
	}

	return (
		<select value={value || ""} onChange={onChange}>
			<option value="">—</option>
			{[...ports].map(([port, pins]) => {
				const irq = (mcu.irqPorts || []).includes(port)
				return (
					<optgroup key={port} label={"P" + port + (irq ? "  ·  interrupts" : "")}>
						{pins.map((pin) => {
							const owners = others(analysis.pins[pin], index, option.name)
							return (
								<option key={pin} value={pin} disabled={option.irq && !irq && pin !== value}>
									{pin}{owners.length ? "   ·  " + owners.map((owner) => owner.label).join(", ") : ""}
								</option>
							)
						})}
					</optgroup>
				)
			})}
		</select>
	)
}

export function OptionField({ index, option }) {
	const { analysis, selection, change } = useConfig()
	const mcu = analysis.target.mcu
	const device = selection.devices[index]
	const info = analysis.devices[index]
	const raw = device.options ? device.options[option.name] : undefined
	const value = raw === undefined || raw === "" ? option.default : raw
	const problems = issuesFor(analysis, index, option.name)

	const set = (next, extra, debounce) => change((draft) => {
		const target = draft.devices[index]
		target.options = target.options || {}
		if (next === undefined || next === "")
			delete target.options[option.name]
		else
			target.options[option.name] = next
		if (extra)
			extra(target)
	}, { debounce })

	if (option.type === "bool") {
		return (
			<label className="field check">
				<Switch checked={value === true || value === "true"} onChange={(event) => set(event.target.checked)} />
				<span className="field-label">{option.label}</span>
				<code className="field-name">{option.name}</code>
			</label>
		)
	}

	let input
	if (option.type === "pin") {
		input = <PinSelect index={index} option={option} value={value} onChange={(event) => set(event.target.value || undefined)} />
	}
	else if (option.type === "gate") {
		const gates = Object.entries(mcu.gates || {}).filter(([, gate]) => !option.gateKind || gate.kinds.includes(option.gateKind))
		input = (
			<select
				value={value || ""}
				onChange={(event) => {
					const name = event.target.value
					const gate = mcu.gates[name]
					const fixed = gate && gate.pins && info ? gate.pins[info.bus] : null
					set(name || undefined, (target) => {
						if (fixed)
							Object.assign(target.options, fixed)
					})
				}}
			>
				<option value="">—</option>
				{gates.map(([name]) => {
					const owners = others(analysis.gates[name], index)
					return (
						<option key={name} value={name}>
							{name}{owners.length ? "   ·  " + owners.map((owner) => owner.label).join(", ") : ""}
						</option>
					)
				})}
			</select>
		)
	}
	else if (option.type === "enum") {
		input = (
			<select value={value === undefined ? "" : String(value)} onChange={(event) => set(event.target.value || undefined)}>
				{option.default === undefined ? <option value="">—</option> : null}
				{option.values.map((item, position) => (
					<option key={item} value={item}>{option.labels ? option.labels[position] : item}</option>
				))}
			</select>
		)
	}
	else {
		input = (
			<input
				type={option.type === "number" ? "number" : "text"}
				value={value === undefined ? "" : value}
				spellCheck={false}
				onChange={(event) => set(event.target.value, null, true)}
			/>
		)
	}

	return (
		<label className={"field" + (problems.length ? " invalid" : "")}>
			<span className="field-label">
				{option.label}
				{option.required ? <span className="req" title="Required">*</span> : null}
				{option.irq ? <em title="Needs a pin with an interrupt vector">IRQ</em> : null}
			</span>
			{input}
			{problems.length
				? <span className="field-error">{short(problems[0].text)}</span>
				: <code className="field-name">{option.name}</code>}
		</label>
	)
}

export function DeviceCard({ index, kind }) {
	const { analysis, selection, change } = useConfig()
	const device = selection.devices[index]
	const info = analysis.devices[index]
	const problems = issuesFor(analysis, index, "name")

	return (
		<div className="card device">
			<div className="device-head">
				{kind ? <Badge tone="info">{kind}</Badge> : null}
				<input
					className={"name-input" + (problems.length ? " invalid" : "")}
					value={device.name || ""}
					spellCheck={false}
					title="Device name"
					onChange={(event) => {
						const name = event.target.value
						change((draft) => {
							draft.devices[index].name = name
						}, { debounce: true })
					}}
				/>
				{info ? <code className="macro" title="Initializer in build/pinout.h">{info.macro}</code> : null}
				<div className="grow" />
				<button
					className="btn small ghost danger"
					onClick={() => change((draft) => {
						draft.devices.splice(index, 1)
					})}
				>
					Remove
				</button>
			</div>
			{problems.length ? <div className="field-error block">{short(problems[0].text)}</div> : null}
			{info && info.schema.length ? (
				<div className="fields">
					{info.schema.map((option) => <OptionField key={option.name} index={index} option={option} />)}
				</div>
			) : <p className="muted">This device has no options.</p>}
		</div>
	)
}
