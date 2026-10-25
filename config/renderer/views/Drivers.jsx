import { nextName, useConfig } from "../state.js"
import { Badge, Chip, DeviceCard, Empty, Switch } from "../components/ui.jsx"

function DriverRow({ driver, active }) {
	const { analysis, change, setUi } = useConfig()
	const errors = analysis.issues.filter((item) => item.level === "error" && item.refs.some((ref) => {
		const device = analysis.devices[ref.device]
		return device && device.driver === driver.name
	})).length

	const toggle = (on) => {
		if (!on && driver.devices && !confirm("Remove the " + driver.devices + " " + driver.label + " device(s) too?"))
			return
		change((draft) => {
			const list = new Set(draft.drivers || [])
			if (on)
				list.add(driver.name)
			else {
				list.delete(driver.name)
				draft.devices = (draft.devices || []).filter((device) => device.driver !== driver.name)
			}
			draft.drivers = [...list]
		})
	}

	return (
		<div className={"list-row" + (active ? " active" : "")} onClick={() => setUi({ driver: driver.name })}>
			<Switch
				checked={driver.state !== "off"}
				onClick={(event) => event.stopPropagation()}
				onChange={(event) => toggle(event.target.checked)}
			/>
			<div className="list-text">
				<div className="name">{driver.label}</div>
				<div className="sub">{[driver.vendor, driver.buses.length ? driver.buses.map((bus) => bus.toUpperCase()).join(" / ") : "GPIO"].filter(Boolean).join(" · ")}</div>
			</div>
			{errors
				? <span className="count err">{errors}</span>
				: driver.devices ? <span className="count">{driver.devices}</span> : null}
		</div>
	)
}

function groupOf(driver) {
	const parts = driver.path.split("/")
	return parts.length > 3 ? parts[1] : "general"
}

export function Drivers() {
	const { analysis, selection, ui, change } = useConfig()
	if (!analysis.target)
		return <Empty>Pick a target first.</Empty>

	const drivers = analysis.drivers
	const current = drivers.find((driver) => driver.name === ui.driver)
		|| drivers.find((driver) => driver.state !== "off")
		|| drivers[0]
	if (!current)
		return <Empty>No drivers/&lt;name&gt;/_phi.js found.</Empty>

	const groups = new Map()
	for (const driver of drivers) {
		const name = groupOf(driver)
		if (!groups.has(name))
			groups.set(name, [])
		groups.get(name).push(driver)
	}

	const indexes = selection.devices
		.map((device, index) => (device.driver === current.name ? index : -1))
		.filter((index) => index >= 0)

	const addDevice = () => change((draft) => {
		draft.devices = draft.devices || []
		draft.devices.push({
			driver: current.name,
			name: nextName(draft, current.name.toLowerCase().replace(/[^a-z0-9]/g, "")),
			options: {},
		})
		if (!(draft.drivers || []).includes(current.name))
			draft.drivers = [...(draft.drivers || []), current.name]
	})

	return (
		<div className="split">
			<div className="list">
				{[...groups].map(([name, list]) => (
					<div key={name}>
						<div className="list-group">{name}</div>
						{list.map((driver) => <DriverRow key={driver.name} driver={driver} active={driver === current} />)}
					</div>
				))}
			</div>

			<div>
				<div className="detail-head">
					<div className="grow">
						<h2>
							{current.label}{" "}
							{current.state === "auto" ? <Badge tone="info">Pulled by its devices</Badge> : null}
						</h2>
						<p>{current.description}</p>
						<div className="meta">
							{current.vendor ? <Chip>{current.vendor}</Chip> : null}
							<Chip>{current.path}</Chip>
							{current.define ? <Chip tone="define">{current.define}</Chip> : null}
							{current.buses.map((bus) => <Chip key={bus}>{bus.toUpperCase()}</Chip>)}
							{current.requires.map((name) => <Chip key={name}>needs {name}</Chip>)}
							{current.sources.map((source) => <Chip key={source}>{source.split("/").pop()}</Chip>)}
						</div>
					</div>
				</div>

				<div className="section-head">
					<h2>Devices</h2>
					<p>Each one becomes an initializer in build/pinout.h.</p>
					<div className="grow" />
					<button className="btn accent small" onClick={addDevice}>+ Add device</button>
				</div>

				{indexes.length
					? indexes.map((index) => <DeviceCard key={index} index={index} />)
					: (
						<Empty>
							{current.state === "off"
								? "Turned off. Add a device or flip the switch to compile it."
								: "Compiled, with no device declared. Your code passes its own options."}
						</Empty>
					)}
			</div>
		</div>
	)
}
