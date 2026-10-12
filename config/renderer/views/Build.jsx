import { useEffect, useRef, useState } from "react"
import { useConfig } from "../state.js"
import { Badge, Empty, Section, StatusIcon } from "../components/ui.jsx"

function Adder({ placeholder, onAdd }) {
	const [value, setValue] = useState("")
	const submit = (event) => {
		event.preventDefault()
		const text = value.trim()
		if (!text)
			return
		onAdd(text)
		setValue("")
	}
	return (
		<form className="adder" onSubmit={submit}>
			<input className="input mono" value={value} placeholder={placeholder} spellCheck={false} onChange={(event) => setValue(event.target.value)} />
			<button className="btn" type="submit">Add</button>
		</form>
	)
}

function Sources() {
	const { analysis, change } = useConfig()
	return (
		<div className="card">
			{analysis.extra.length ? (
				<div className="rows">
					{analysis.extra.map((item, index) => (
						<div className="row" key={item.path}>
							<code>{item.path}</code>
							{item.exists ? null : <Badge tone="err">Missing</Badge>}
							<div className="grow" />
							<button
								className="btn small ghost danger"
								onClick={() => change((draft) => {
									draft.sources.splice(index, 1)
								})}
							>
								Remove
							</button>
						</div>
					))}
				</div>
			) : <p className="muted">Nothing besides the app sources.</p>}
			<Adder
				placeholder="app/mine/extra.c"
				onAdd={(path) => change((draft) => {
					draft.sources = [...(draft.sources || []), path.replace(/^\.\//, "")]
				})}
			/>
		</div>
	)
}

function Defines() {
	const { selection, analysis, change } = useConfig()
	const fixed = analysis.defines.filter((define) => !(selection.defines || []).includes(define))
	return (
		<div className="card">
			<div className="choice-meta">
				{fixed.map((define) => <span className="chip" key={define} title="From the arch or the MCU">{define}</span>)}
				{(selection.defines || []).map((define, index) => (
					<span className="chip removable" key={define}>
						{define}
						<button
							title="Remove"
							onClick={() => change((draft) => {
								draft.defines.splice(index, 1)
							})}
						>
							×
						</button>
					</span>
				))}
			</div>
			<Adder
				placeholder="NAME or NAME=value"
				onAdd={(define) => change((draft) => {
					draft.defines = [...new Set([...(draft.defines || []), define])]
				})}
			/>
		</div>
	)
}

function Checks() {
	const { result } = useConfig()
	if (!result.checks.length)
		return <div className="card muted">No prepare() hook ran.</div>
	return (
		<div className="card">
			{result.checks.map((check, index) => (
				<div className="check" key={index}>
					<StatusIcon level={check.level} />
					<div>
						<div className={check.level === "ok" ? "mono" : ""}>{check.text}</div>
						{check.hint ? <div className="hint">{check.hint}</div> : null}
					</div>
					<span className="src">{check.source}</span>
				</div>
			))}
		</div>
	)
}

function SaveBoard() {
	const { selection, catalog, actions } = useConfig()
	const board = catalog.boards.find((record) => record.name === selection.board)
	const [dir, setDir] = useState(board ? board.dir.replace(/^boards\//, "") : "")
	const [label, setLabel] = useState(board ? board.data.label : "")

	return (
		<div className="card">
			<div className="fields two">
				<label className="field">
					<span className="field-label">Directory</span>
					<input className="mono" value={dir} placeholder="vendor/my-board" spellCheck={false} onChange={(event) => setDir(event.target.value)} />
					<code className="field-name">boards/{dir || "<vendor>/<board>"}/_phi.js</code>
				</label>
				<label className="field">
					<span className="field-label">Label</span>
					<input value={label} placeholder="My board" onChange={(event) => setLabel(event.target.value)} />
				</label>
			</div>
			<div className="actions">
				<button className="btn" disabled={!dir} onClick={() => actions.saveBoard(dir, label)}>Save as board</button>
			</div>
		</div>
	)
}

function Files() {
	const { result, ui, setUi, actions, analysis } = useConfig()
	const links = "include/ links"
	const width = Math.max(0, ...analysis.headers.map((header) => header.link.length))
	const files = {
		...result.files,
		[links]: analysis.headers.map((header) => header.link.padEnd(width + 2) + "-> " + header.target + "\n").join(""),
	}
	const names = Object.keys(files)
	const current = names.includes(ui.file) ? ui.file : names[0]
	const blocked = analysis.issues.some((item) => item.level === "error")
	if (!Object.keys(result.files).length)
		return <Empty>Nothing to generate yet.</Empty>

	return (
		<div className="card">
			<div className="tabs">
				{names.map((name) => (
					<button key={name} className={name === current ? "active" : ""} onClick={() => setUi({ file: name })}>{name}</button>
				))}
				<div className="grow" />
				<button className="btn small" disabled={blocked} onClick={() => actions.write()}>Write build/</button>
			</div>
			<pre className="code">{files[current]}</pre>
		</div>
	)
}

function Console() {
	const { log, making, actions, analysis } = useConfig()
	const ref = useRef(null)
	const blocked = analysis.issues.some((item) => item.level === "error")

	useEffect(() => {
		if (ref.current)
			ref.current.scrollTop = ref.current.scrollHeight
	}, [log])

	return (
		<div className="card">
			<div className="tabs">
				<span className="tabs-title">make -C build</span>
				<div className="grow" />
				<button className="btn small" disabled={making} onClick={() => actions.make("clean")}>make clean</button>
				{analysis.probes.flatMap((probe) => probe.targets.map((target) => (
					<button
						key={target.name}
						className="btn small"
						title={target.label + " with the " + probe.label}
						disabled={blocked || making}
						onClick={() => actions.make(target.name)}
					>
						make {target.name}
					</button>
				)))}
				<button className="btn small primary" disabled={blocked || making} onClick={() => actions.make()}>
					{making ? "Building…" : "Run make"}
				</button>
			</div>
			<pre className="code console" ref={ref}>
				{log.length
					? log.map((chunk, index) => <span key={index} className={chunk.stream}>{chunk.text}</span>)
					: <span className="faint">Output of make shows up here.</span>}
			</pre>
		</div>
	)
}

export function Build() {
	const { analysis } = useConfig()
	if (!analysis.target)
		return <Empty>Pick a target first.</Empty>

	return (
		<>
			<div className="cols">
				<div>
					<Section title="Extra sources" subtitle="Compiled next to the app, or your own main() when no app is picked.">
						<Sources />
					</Section>
					<Section title="Defines" subtitle="Passed on the compiler command line.">
						<Defines />
					</Section>
					<Section title="Board preset" subtitle="Save this selection under boards/.">
						<SaveBoard />
					</Section>
				</div>
				<div>
					<Section title="Environment" subtitle="What the prepare() hooks found on this machine.">
						<Checks />
					</Section>
				</div>
			</div>
			<Section title="Console"><Console /></Section>
			<Section title="Generated files" subtitle="Preview of build/.">
				<Files />
			</Section>
		</>
	)
}
