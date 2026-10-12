import { useCallback, useEffect, useRef, useState } from "react"
import logo from "../assets/logo.svg"
import { Config, bytes } from "./state.js"
import { icons } from "./components/Icons.jsx"
import { Target } from "./views/Target.jsx"
import { Application } from "./views/Application.jsx"
import { Kernel } from "./views/Kernel.jsx"
import { Drivers } from "./views/Drivers.jsx"
import { Pinout } from "./views/Pinout.jsx"
import { Build } from "./views/Build.jsx"
import { Issues } from "./views/Issues.jsx"

const views = [
	{ id: "target", label: "Target", subtitle: "Architecture, series, microcontroller and toolchain.", View: Target },
	{ id: "app", label: "App", subtitle: "The application the kernel boots into, and what it needs.", View: Application },
	{ id: "kernel", label: "Kernel", subtitle: "Core settings and the common modules compiled in.", View: Kernel },
	{ id: "drivers", label: "Drivers", subtitle: "Driver code and the devices it runs.", View: Drivers },
	{ id: "pinout", label: "Pinout", subtitle: "Who owns each pin and gate, plus the application peripherals.", View: Pinout },
	{ id: "build", label: "Build", subtitle: "Sources, environment checks, generated files and make.", View: Build },
	{ id: "issues", label: "Issues", subtitle: "Everything that blocks or weakens this configuration.", View: Issues },
]

function cleanError(error) {
	return String(error.message || error).replace(/^Error invoking remote method '[^']+': (Error: )?/, "")
}

function counts(result, view) {
	if (!result)
		return { errors: 0, warnings: 0 }
	const issues = result.analysis.issues
	if (view === "issues")
		return {
			errors: issues.filter((item) => item.level === "error").length,
			warnings: issues.filter((item) => item.level === "warning").length,
		}
	const own = issues.filter((item) => item.view === view)
	const failed = view === "target" ? result.checks.filter((check) => check.level === "error").length : 0
	return {
		errors: own.filter((item) => item.level === "error").length,
		warnings: own.filter((item) => item.level === "warning").length + failed,
	}
}

function Sidebar({ ctx }) {
	const { catalog, selection, result, view, setView, dirty, actions } = ctx
	const analysis = result && result.analysis
	const target = analysis && analysis.target

	return (
		<aside className="side">
			<div className="brand">
				<img src={logo} alt="" />
				<div>
					<div className="brand-name">mini<span>Phi</span></div>
					<div className="brand-sub">Build configurator</div>
				</div>
			</div>

			<div>
				<div className="side-label">Start from</div>
				<select value={selection.board || ""} onChange={(event) => actions.loadBoard(event.target.value)}>
					{[...new Set(catalog.boards.map((board) => board.data.vendor || "Other"))].map((vendor) => (
						<optgroup key={vendor} label={vendor}>
							{catalog.boards.filter((board) => (board.data.vendor || "Other") === vendor).map((board) => (
								<option key={board.name} value={board.name}>{board.data.label || board.name}</option>
							))}
						</optgroup>
					))}
					<option value="">Blank configuration</option>
				</select>
			</div>

			<nav className="nav">
				{views.map((item) => {
					const { errors, warnings } = counts(result, item.id)
					return (
						<button key={item.id} className={view === item.id ? "active" : ""} onClick={() => setView(item.id)}>
							<span className="icon">{icons[item.id]}</span>
							{item.label}
							{errors ? <span className="count">{errors}</span> : warnings ? <span className="count warn">{warnings}</span> : null}
						</button>
					)
				})}
			</nav>

			<div className="side-foot">
				{target ? (
					<>
						<strong>{target.mcu.label}</strong>
						<span>{bytes(target.mcu.flash)} flash · {bytes(target.mcu.ram)} RAM</span>
						<span>{target.toolchain.label}</span>
						<span>App: {analysis.app ? analysis.app.label : "none"}</span>
						<span>
							{analysis.modules.filter((module) => module.state !== "off").length} modules ·{" "}
							{analysis.devices.length} devices
						</span>
					</>
				) : <span>No target</span>}
				<span className={"sync " + (dirty ? "dirty" : "clean")}>
					{dirty ? "Not written to build/ yet" : "build/ is up to date"}
				</span>
				<button className="link" onClick={actions.reload}>{icons.refresh} Rescan _phi.js</button>
			</div>

			<div className="side-legal">
				<span>miniPhi © 2014–2026 Michael VERGOZ</span>
				<span>
					Free software under the GNU General Public License, version 3 or later.
					It comes with no warranty. The kernel sources linked into the firmware are under the same license.
				</span>
			</div>
		</aside>
	)
}

function Topbar({ ctx, current }) {
	const { result, making, actions, setView } = ctx
	const { errors, warnings } = counts(result, "issues")
	const environment = result ? result.checks.filter((check) => check.level === "error").length : 0
	const blocked = !result || !result.analysis.target || errors > 0

	let status
	if (!result)
		status = <span className="status">Analyzing…</span>
	else if (errors)
		status = <button className="status err" onClick={() => setView("issues")}>{errors} error{errors > 1 ? "s" : ""}</button>
	else if (warnings)
		status = <button className="status warn" onClick={() => setView("issues")}>{warnings} warning{warnings > 1 ? "s" : ""}</button>
	else if (environment)
		status = <button className="status warn" onClick={() => setView("issues")}>Toolchain not found</button>
	else
		status = <span className="status ok">Ready to build</span>

	return (
		<header className="top">
			<div>
				<h1>{current.label}</h1>
				<p>{current.subtitle}</p>
			</div>
			<div className="grow" />
			{status}
			<button className="btn" disabled={blocked} onClick={() => actions.write()}>Write build/</button>
			<button className="btn primary" disabled={blocked || making} onClick={() => actions.make()}>
				{making ? "Building…" : "Run make"}
			</button>
		</header>
	)
}

export function App() {
	const [catalog, setCatalog] = useState(null)
	const [selection, setSelection] = useState(null)
	const [result, setResult] = useState(null)
	const [view, setView] = useState("target")
	const [ui, setUiState] = useState({ driver: null, pin: null, file: "Makefile" })
	const [dirty, setDirty] = useState(false)
	const [log, setLog] = useState([])
	const [making, setMaking] = useState(false)
	const [toast, setToast] = useState(null)
	const [root, setRoot] = useState("")
	const delay = useRef(0)

	useEffect(() => {
		window.phi.init().then((data) => {
			setCatalog(data.catalog)
			setSelection(data.selection)
			setRoot(data.root)
			setDirty(!data.saved)
		})
		window.phi.onMakeOutput((chunk) => setLog((lines) => [...lines, chunk]))
	}, [])

	useEffect(() => {
		if (!selection)
			return
		let live = true
		const timer = setTimeout(() => {
			window.phi.evaluate(selection).then((next) => {
				if (live)
					setResult(next)
			})
		}, delay.current)
		delay.current = 0
		return () => {
			live = false
			clearTimeout(timer)
		}
	}, [selection, catalog])

	const notify = useCallback((text, tone) => {
		const id = Date.now()
		setToast({ id, text, tone })
		setTimeout(() => setToast((current) => (current && current.id === id ? null : current)), 3500)
	}, [])

	const change = useCallback((mutate, options = {}) => {
		delay.current = options.debounce ? 200 : 0
		setSelection((previous) => {
			const next = structuredClone(previous)
			mutate(next)
			return next
		})
		setDirty(true)
	}, [])

	const setUi = useCallback((patch) => setUiState((current) => ({ ...current, ...patch })), [])

	const go = useCallback((next, patch) => {
		if (patch)
			setUi(patch)
		setView(next)
	}, [setUi])

	const write = async (quiet) => {
		try {
			const files = await window.phi.write(selection)
			setDirty(false)
			if (!quiet)
				notify("Wrote " + files.join(", "))
			return true
		}
		catch (error) {
			notify(cleanError(error), "err")
			return false
		}
	}

	const actions = {
		write,
		async make(target) {
			if (making)
				return
			if (target !== "clean" && !(await write(true)))
				return
			setMaking(true)
			setView("build")
			setLog([{ stream: "cmd", text: "$ make" + (target ? " " + target : "") + "\n" }])
			const outcome = await window.phi.make(target)
			setMaking(false)
			let tail = "\nmake exited with " + outcome.code + "\n"
			if (outcome.missing)
				tail = "build/Makefile is missing. Write build/ first.\n"
			if (outcome.busy)
				tail = "make is already running.\n"
			setLog((lines) => [...lines, { stream: outcome.code === 0 ? "ok" : "cmd", text: tail }])
		},
		async loadBoard(name) {
			if (dirty && !confirm("Replace the current selection?"))
				return
			const next = await window.phi.board(name || null, selection)
			setSelection(next)
			setDirty(true)
			setUi({ driver: null, pin: null })
		},
		async saveBoard(dir, label) {
			try {
				let saved = await window.phi.saveBoard({ dir, label, selection })
				if (saved.exists) {
					if (!confirm("boards/" + dir + "/_phi.js exists. Overwrite it?"))
						return
					saved = await window.phi.saveBoard({ dir, label, selection, overwrite: true })
				}
				setCatalog(saved.catalog)
				change((next) => {
					next.board = saved.board
				})
				notify("Saved " + saved.path)
			}
			catch (error) {
				notify(cleanError(error), "err")
			}
		},
		async browse(name, current) {
			const dir = await window.phi.browse(current)
			if (dir)
				change((next) => {
					next.vars = next.vars || {}
					next.vars[name] = dir
				})
		},
		async reload() {
			setCatalog(await window.phi.reload())
			notify("Rescanned the _phi.js files")
		},
	}

	if (!catalog || !selection)
		return <div className="loading"><img src={logo} alt="" /><span>Scanning _phi.js…</span></div>

	const current = views.find((item) => item.id === view)
	const ctx = {
		catalog,
		selection,
		result,
		analysis: result ? result.analysis : null,
		root,
		view,
		setView,
		ui,
		setUi,
		go,
		change,
		dirty,
		log,
		making,
		actions,
	}
	const { View } = current

	return (
		<Config.Provider value={ctx}>
			<div className="app">
				<Sidebar ctx={ctx} />
				<section className="main">
					<Topbar ctx={ctx} current={current} />
					<div className="view" key={view}>
						{result ? <View /> : <div className="empty">Analyzing the selection…</div>}
					</div>
				</section>
			</div>
			{toast ? <div className={"toast" + (toast.tone ? " " + toast.tone : "")}>{toast.text}</div> : null}
		</Config.Provider>
	)
}
