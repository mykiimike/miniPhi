import { useConfig } from "../state.js"
import { Section, StatusIcon } from "../components/ui.jsx"

const labels = { target: "Target", app: "App", kernel: "Kernel", drivers: "Drivers", pinout: "Pinout", build: "Build" }

function Row({ item }) {
	const { analysis, go } = useConfig()
	const ref = item.refs.find((entry) => entry.device !== undefined)
	const device = ref ? analysis.devices[ref.device] : null
	const view = device ? device.view : item.view
	const open = () => go(view, device && device.driver ? { driver: device.driver } : undefined)

	return (
		<div className="check">
			<StatusIcon level={item.level} />
			<div className="grow">{item.text}</div>
			{labels[view] ? <button className="btn small ghost" onClick={open}>{labels[view]} →</button> : null}
		</div>
	)
}

export function Issues() {
	const { analysis, result } = useConfig()
	const errors = analysis.issues.filter((item) => item.level === "error")
	const warnings = analysis.issues.filter((item) => item.level !== "error")
	const environment = result.checks.filter((check) => check.level !== "ok")

	if (!analysis.issues.length && !environment.length)
		return <div className="all-clear"><div className="mark">✓</div><h2>Nothing to fix</h2><p>Write build/ and run make.</p></div>

	return (
		<>
			{errors.length ? (
				<Section title="Errors" subtitle="These block Write build/.">
					<div className="card">{errors.map((item, index) => <Row key={index} item={item} />)}</div>
				</Section>
			) : null}
			{warnings.length ? (
				<Section title="Warnings">
					<div className="card">{warnings.map((item, index) => <Row key={index} item={item} />)}</div>
				</Section>
			) : null}
			{environment.length ? (
				<Section title="Environment" subtitle="Found by the prepare() hooks. They do not block Write build/, make will fail.">
					<div className="card">
						{environment.map((check, index) => (
							<div className="check" key={index}>
								<StatusIcon level={check.level} />
								<div className="grow">
									<div>{check.text}</div>
									{check.hint ? <div className="hint">{check.hint}</div> : null}
								</div>
								<span className="src">{check.source}</span>
							</div>
						))}
					</div>
				</Section>
			) : null}
		</>
	)
}
