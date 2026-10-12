const base = { width: 18, height: 18, viewBox: "0 0 24 24", fill: "none", stroke: "currentColor", strokeWidth: 1.7, strokeLinecap: "round", strokeLinejoin: "round" }

export const icons = {
	target: (
		<svg {...base}>
			<circle cx="12" cy="12" r="8.5" />
			<circle cx="12" cy="12" r="4" />
			<path d="M12 1.5v3M12 19.5v3M1.5 12h3M19.5 12h3" />
		</svg>
	),
	app: (
		<svg {...base}>
			<rect x="3" y="3" width="18" height="18" rx="4" />
			<path d="m10 8.5 5 3.5-5 3.5z" />
		</svg>
	),
	kernel: (
		<svg {...base}>
			<path d="M12 2.5 20.5 7.25v9.5L12 21.5l-8.5-4.75v-9.5z" />
			<path d="M12 12 20.5 7.25M12 12v9.5M12 12 3.5 7.25" />
		</svg>
	),
	drivers: (
		<svg {...base}>
			<rect x="6" y="6" width="12" height="12" rx="2" />
			<path d="M9 2.5V6M15 2.5V6M9 18v3.5M15 18v3.5M2.5 9H6M2.5 15H6M18 9h3.5M18 15h3.5" />
		</svg>
	),
	pinout: (
		<svg {...base}>
			<rect x="3" y="3" width="7" height="7" rx="1.5" />
			<rect x="14" y="3" width="7" height="7" rx="1.5" />
			<rect x="3" y="14" width="7" height="7" rx="1.5" />
			<rect x="14" y="14" width="7" height="7" rx="1.5" />
		</svg>
	),
	build: (
		<svg {...base}>
			<rect x="2.5" y="4" width="19" height="16" rx="2.5" />
			<path d="m7 9.5 3 2.5-3 2.5M12.5 15h4.5" />
		</svg>
	),
	issues: (
		<svg {...base}>
			<path d="M10.3 3.9 2.4 17.6A2 2 0 0 0 4.1 20.6h15.8a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z" />
			<path d="M12 9.5v4.5M12 17.2v.1" />
		</svg>
	),
	refresh: (
		<svg {...base} width={14} height={14}>
			<path d="M20 11a8 8 0 1 0-2.3 5.7M20 4v7h-7" />
		</svg>
	),
}
