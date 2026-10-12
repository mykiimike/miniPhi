import { useConfig } from "../state.js"
import { Chip, Choice, DeviceCard, Empty, Placeholder, Section, SettingField } from "../components/ui.jsx"

export function Application() {
	const { catalog, selection, analysis, change } = useConfig()
	if (!analysis.target)
		return <Empty>Pick a target first.</Empty>

	const arch = catalog.archs.find((record) => record.name === selection.arch)
	const buses = arch ? arch.data.buses || {} : {}
	const busLabel = (bus) => (buses[bus] ? buses[bus].label : bus)
	const app = analysis.app
	const settings = app ? analysis.settings.filter((setting) => setting.owner === app.label) : []

	const addNeed = (need) => (draft) => {
		draft.devices = draft.devices || []
		if (!draft.devices.some((device) => !device.driver && device.bus === need.bus && device.name === need.name))
			draft.devices.push({ bus: need.bus, name: need.name, options: {} })
	}

	const pick = (record) => change((draft) => {
		draft.app = record.name
		for (const need of record.data.devices || [])
			addNeed(need)(draft)
	})

	return (
		<>
			<Section title="Application" subtitle="app/<name>/_phi.js">
				<div className="grid">
					{catalog.apps.map((record) => (
						<Choice
							key={record.name}
							active={record.name === selection.app}
							title={record.data.label}
							sub={record.data.description}
							meta={[record.name, ...(record.data.requires || []).map((name) => "needs " + name)]}
							onClick={() => pick(record)}
						/>
					))}
					<Choice
						active={!selection.app}
						title="None"
						sub="Bring your own main() through Build, Extra sources."
						onClick={() => change((draft) => {
							draft.app = null
						})}
					/>
					<Placeholder title="Another application" sub="Add app/<name>/_phi.js and main.c, then rescan." />
				</div>
			</Section>

			{app ? (
				<>
					<Section title={app.label} subtitle={app.path}>
						<div className="card stack">
							<p className="module-desc">{app.description}</p>
							<div className="choice-meta">
								{app.requires.map((name) => <Chip key={"m" + name}>needs {name}</Chip>)}
								{app.drivers.map((name) => <Chip key={"d" + name}>driver {name}</Chip>)}
								{app.sources.map((source) => <Chip key={source}>{source.split("/").pop()}</Chip>)}
							</div>
							{settings.length ? (
								<div className="fields">
									{settings.map((setting) => <SettingField key={setting.name} setting={setting} />)}
								</div>
							) : null}
						</div>
					</Section>

					<Section title="Peripherals it opens" subtitle="Each one reaches the app as PHI_<NAME> in build/include/pinout.h.">
						{app.needs.length ? app.needs.map((need) => (
							need.index >= 0
								? <DeviceCard key={need.bus + need.name} index={need.index} kind={(need.label || need.name) + " · " + busLabel(need.bus)} />
								: (
									<div className="card missing" key={need.bus + need.name}>
										<span>{app.label} needs a {busLabel(need.bus)} named <code>{need.name}</code>.</span>
										<div className="grow" />
										<button className="btn accent small" onClick={() => change(addNeed(need))}>Add {need.label || need.name}</button>
									</div>
								)
						)) : <Empty>This application opens no peripheral itself.</Empty>}
					</Section>
				</>
			) : null}
		</>
	)
}
