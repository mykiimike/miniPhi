import { bytes, useConfig } from "../state.js"
import { Badge, Choice, Placeholder, Section, SettingField, StatusIcon } from "../components/ui.jsx"

function Variable({ item }) {
	const { selection, result, change, actions } = useConfig()
	const checks = result.checks.filter((check) => check.vars.includes(item.name))
	const custom = selection.vars ? selection.vars[item.name] : undefined
	const value = custom === undefined ? item.default || "" : custom

	return (
		<div className="var">
			<div className="var-head">
				<span className="field-label">{item.label}</span>
				<code className="field-name">{item.name}</code>
				{item.hint ? <span className="hint">{item.hint}</span> : null}
			</div>
			<div className="var-input">
				<input
					className="input mono"
					value={value}
					spellCheck={false}
					onChange={(event) => {
						const next = event.target.value
						change((draft) => {
							draft.vars = draft.vars || {}
							draft.vars[item.name] = next
						}, { debounce: true })
					}}
				/>
				<button className="btn" onClick={() => actions.browse(item.name, value)}>Browse…</button>
				{custom !== undefined && item.default ? (
					<button
						className="btn ghost"
						title={item.default}
						onClick={() => change((draft) => {
							delete draft.vars[item.name]
						})}
					>
						Default
					</button>
				) : null}
			</div>
			{checks.length ? (
				<div className="var-checks">
					{checks.map((check, index) => (
						<div className="check compact" key={index}>
							<StatusIcon level={check.level} />
							<div>
								<div className="mono">{check.text}</div>
								{check.hint ? <div className="hint">{check.hint}</div> : null}
							</div>
						</div>
					))}
				</div>
			) : null}
		</div>
	)
}

export function Target() {
	const { catalog, selection, analysis, change } = useConfig()
	const arch = catalog.archs.find((record) => record.name === selection.arch)
	const seriesList = catalog.series.filter((record) => record.data.arch === selection.arch)
	const series = seriesList.find((record) => record.name === selection.series)
	const toolchains = arch ? arch.data.toolchains || [] : []
	const toolchain = toolchains.find((item) => item.name === selection.toolchain)
	const settingsOf = (owner) => (analysis.target ? analysis.settings.filter((setting) => setting.owner === owner) : [])

	const pickArch = (record) => change((draft) => {
		const first = catalog.series.find((item) => item.data.arch === record.name)
		draft.arch = record.name
		draft.series = first ? first.name : null
		draft.mcu = first && first.data.mcus[0] ? first.data.mcus[0].name : null
		draft.toolchain = record.data.toolchains && record.data.toolchains[0] ? record.data.toolchains[0].name : null
	})

	const pickSeries = (record) => change((draft) => {
		draft.series = record.name
		draft.mcu = record.data.mcus[0] ? record.data.mcus[0].name : null
	})

	return (
		<>
			<Section title="Architecture" subtitle="arch/<name>/_phi.js">
				<div className="grid">
					{catalog.archs.map((record) => (
						<Choice
							key={record.name}
							active={record.name === selection.arch}
							title={record.data.label}
							sub={record.data.description}
							meta={[
								record.data.vendor,
								record.data.define,
								catalog.series.filter((item) => item.data.arch === record.name).length + " series",
								(record.data.toolchains || []).length + " toolchains",
							]}
							onClick={() => pickArch(record)}
						/>
					))}
					<Placeholder title="Another architecture" sub="Add arch/<name>/_phi.js, then rescan." />
				</div>
			</Section>

			{arch ? (
				<Section title="Series" subtitle={"arch/" + arch.name + "/<series>/_phi.js"}>
					<div className="grid">
						{seriesList.map((record) => (
							<Choice
								key={record.name}
								active={record.name === selection.series}
								title={record.data.label}
								sub={record.data.description}
								meta={[record.data.vendor, record.data.mcus.length + " MCUs", ...(record.data.boot || []).map((file) => file.split("/").pop())]}
								onClick={() => pickSeries(record)}
							/>
						))}
					</div>
				</Section>
			) : null}

			{series ? (
				<Section title="Microcontroller">
					<div className="grid">
						{series.data.mcus.map((mcu) => (
							<Choice
								key={mcu.name}
								active={mcu.name === selection.mcu}
								title={mcu.label}
								badges={mcu.package ? <Badge>{mcu.package}</Badge> : null}
								meta={[
									bytes(mcu.flash) + " flash",
									bytes(mcu.ram) + " RAM",
									mcu.pins.length + " pins",
									Object.keys(mcu.gates || {}).length + " gates",
								]}
								onClick={() => change((draft) => {
									draft.mcu = mcu.name
								})}
							/>
						))}
					</div>
				</Section>
			) : null}

			{arch ? (
				<Section title="Toolchain">
					<div className="grid">
						{toolchains.map((item) => (
							<Choice
								key={item.name}
								active={item.name === selection.toolchain}
								title={item.label}
								badges={item.experimental ? <Badge tone="warn">Experimental</Badge> : null}
								sub={item.description}
								meta={[item.output]}
								onClick={() => change((draft) => {
									draft.toolchain = item.name
								})}
							/>
						))}
					</div>
					{toolchain && analysis.target ? (
						<div className="card stack">
							{analysis.vars.map((item) => <Variable key={item.name} item={item} />)}
							{settingsOf(toolchain.label).length ? (
								<div className="fields">
									{settingsOf(toolchain.label).map((setting) => <SettingField key={setting.name} setting={setting} />)}
								</div>
							) : null}
						</div>
					) : null}
				</Section>
			) : null}

			{series && settingsOf(series.data.label).length ? (
				<Section title="Clocks and debug" subtitle={series.data.label}>
					<div className="card">
						<div className="fields">
							{settingsOf(series.data.label).map((setting) => <SettingField key={setting.name} setting={setting} />)}
						</div>
					</div>
				</Section>
			) : null}
		</>
	)
}
