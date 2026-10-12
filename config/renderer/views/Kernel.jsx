import { bytes, useConfig } from "../state.js"
import { Badge, Chip, Empty, Section, SettingField, Switch } from "../components/ui.jsx"

const ramColors = ["#f2a93b", "#3b6fd8", "#2e8a57", "#a855c7", "#d1495b", "#0f9aa8"]

function Ram() {
	const { analysis } = useConfig()
	const { total, used, items } = analysis.ram
	const color = (index) => ramColors[index % ramColors.length]

	return (
		<div className="card">
			<div className="ram-head">
				<strong>{bytes(used)}</strong>
				<span className="muted">declared of {bytes(total)} RAM</span>
				<div className="grow" />
				{used > total
					? <Badge tone="err">Over budget</Badge>
					: <Badge tone="ok">{Math.round((used / total) * 100)} %</Badge>}
			</div>
			<div className="ram">
				{items.map((item, index) => (
					<span
						key={item.owner + item.label}
						title={item.owner + " · " + item.label + ": " + item.bytes + " bytes"}
						style={{ width: Math.min(100, (item.bytes / total) * 100) + "%", background: color(index) }}
					/>
				))}
			</div>
			<div className="ram-legend">
				{items.map((item, index) => (
					<span key={item.owner + item.label}>
						<i className="swatch" style={{ background: color(index) }} />
						{item.label} <span className="faint">({item.owner})</span> <strong>{bytes(item.bytes)}</strong>
					</span>
				))}
			</div>
			<p className="hint">
				Counts the sizes declared in _phi.js settings. Globals and the task table come on top; the map file has the real figure.
			</p>
		</div>
	)
}

function ModuleCard({ module }) {
	const { analysis, change } = useConfig()
	const on = module.state !== "off"
	const locked = module.state === "required" || module.state === "auto"
	const settings = analysis.settings.filter((setting) => setting.owner === module.label)

	const toggle = (checked) => change((draft) => {
		const list = new Set(draft.modules || [])
		if (checked)
			list.add(module.name)
		else
			list.delete(module.name)
		draft.modules = [...list]
	})

	let status = null
	if (module.state === "required")
		status = <Badge tone="accent">Required</Badge>
	else if (module.state === "auto")
		status = <Badge tone="info">Needed by {module.by.join(", ")}</Badge>
	else if (module.state === "selected" && module.by.length)
		status = <Badge>Also needed by {module.by.join(", ")}</Badge>

	return (
		<div className={"card module" + (on ? "" : " off")}>
			<div className="module-head">
				<Switch
					checked={on}
					disabled={locked}
					title={locked ? "Turned on by a dependency" : "Compile this module"}
					onChange={(event) => toggle(event.target.checked)}
				/>
				<span className="module-title">{module.label}</span>
				<code className="faint">{module.name}</code>
			</div>
			<p className="module-desc">{module.description}</p>
			<div className="choice-meta">
				{status}
				{module.requires.map((name) => <Chip key={name}>needs {name}</Chip>)}
				{module.define ? <Chip tone="define">{module.define}</Chip> : null}
			</div>
			{on && settings.length ? (
				<div className="fields">
					{settings.map((setting) => <SettingField key={setting.name} setting={setting} />)}
				</div>
			) : null}
		</div>
	)
}

export function Kernel() {
	const { analysis } = useConfig()
	if (!analysis.target)
		return <Empty>Pick a target first.</Empty>

	const kernelSettings = analysis.settings.filter((setting) => setting.owner === analysis.kernel.label)
	const required = analysis.modules.filter((module) => module.required)
	const optional = analysis.modules.filter((module) => !module.required)

	return (
		<>
			<Section title="Memory"><Ram /></Section>

			<Section title="Core" subtitle={analysis.kernel.path}>
				<div className="card stack">
					<div className="module-head">
						<span className="module-title">{analysis.kernel.label}</span>
						<Badge tone="accent">Always built</Badge>
					</div>
					<p className="module-desc">{analysis.kernel.description}</p>
					<div className="choice-meta">
						{analysis.kernel.sources.map((source) => <Chip key={source}>{source.split("/").pop()}</Chip>)}
					</div>
					<div className="fields">
						{kernelSettings.map((setting) => <SettingField key={setting.name} setting={setting} />)}
					</div>
				</div>
			</Section>

			{required.length ? (
				<Section title="Required modules">
					<div className="grid wide">
						{required.map((module) => <ModuleCard key={module.name} module={module} />)}
					</div>
				</Section>
			) : null}

			<Section title="Optional modules" subtitle="common/<name>/_phi.js">
				<div className="grid wide">
					{optional.map((module) => <ModuleCard key={module.name} module={module} />)}
				</div>
			</Section>
		</>
	)
}
