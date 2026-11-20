/* TrueType fonts shipped in fonts/, each with its license next to it */
const fonts = [
	{ name: "dejavu-sans", label: "DejaVu Sans", file: "fonts/dejavu-sans/DejaVuSans.ttf", license: "Bitstream Vera" },
	{ name: "dejavu-sans-mono", label: "DejaVu Sans Mono", file: "fonts/dejavu-sans-mono/DejaVuSansMono.ttf", license: "Bitstream Vera" },
	{ name: "liberation-sans", label: "Liberation Sans", file: "fonts/liberation-sans/LiberationSans-Regular.ttf", license: "SIL OFL 1.1" },
	{ name: "jetbrains-mono", label: "JetBrains Mono", file: "fonts/jetbrains-mono/JetBrainsMono-Regular.ttf", license: "SIL OFL 1.1" },
	{ name: "silkscreen", label: "Silkscreen (pixel, 8 or 16 px)", file: "fonts/silkscreen/Silkscreen-Regular.ttf", license: "SIL OFL 1.1" },
]

const here = "common/tui/"

export const phi = {
	kind: "module",
	name: "TUI",
	label: "Things UI",
	description: "A user interface for small screens. On start it shows the phi and the word miniPhi, a short rule, the version and the Swiss and European flags. The mark rides a colour wave that settles to white, then the rule, the version and the flags fade in. Then a home of applications declared by the firmware. Opening one shows a scrollable page and a Back button. With INPUT, every touch panel, mouse and keyboard clicks, drags, drops and scrolls them. Things are drawn in bands into any RGB565 display: no framebuffer, and several screens at once. The task only runs while something is dirty or an input arrives; a screen saver dims, then turns off a display that can when hibernate is allowed. Gauges draw a dial, a thermometer or a segmented bar from milli-units. The default font, the splash wordmark and the phi are rasterized when build/ is written.",
	define: "SUPPORT_COMMON_TUI",
	requires: ["DEVICE"],
	sources: [
		"tui.c", "splash.c", "draw.c", "font.c", "task.c", "font_default.c",
		"widgets/box.c", "widgets/label.c", "widgets/bar.c", "widgets/gauge.c", "widgets/table.c",
		"widgets/columns.c", "widgets/window.c", "widgets/button.c", "widgets/switch.c",
		"widgets/view.c", "widgets/page.c", "widgets/named.c",
	],
	headers: ["tui.h", "font.h", "splash.h"],
	tests: ["test/splash.c"],
	settings: [
		{
			name: "MP_TUI_FONT", label: "Default font", type: "enum", emit: "none",
			values: fonts.map((font) => font.name),
			labels: fonts.map((font) => font.label + " (" + font.license + ")"),
			default: "dejavu-sans",
		},
		{ name: "MP_TUI_FONT_SIZE", label: "Default font size", type: "number", unit: "px", default: 12, min: 6, max: 48, emit: "none" },
		{
			name: "MP_TUI_FONT_BPP", label: "Default font rendering", type: "enum", emit: "none",
			values: ["4", "1"], labels: ["Smoothed, 4 bits per pixel", "Sharp, 1 bit per pixel"], default: "4",
		},
		{
			name: "MP_TUI_FONT_CHARSET", label: "Default font characters", type: "enum", emit: "none",
			values: ["latin1", "ascii"], labels: ["Latin-1, accents and °", "ASCII"], default: "latin1",
		},
		{ name: "MP_TUI_TEXT_MAX", label: "Label and title length", type: "number", unit: "bytes", default: 32, min: 8, max: 256 },
		{ name: "MP_TUI_DIRTY_MAX", label: "Dirty rectangles per screen", type: "number", default: 4, min: 1, max: 16 },
		{ name: "MP_TUI_DIM_AFTER", label: "Dim the backlight after (0 never)", type: "number", unit: "s", default: 30, min: 0, max: 3600 },
		{ name: "MP_TUI_OFF_AFTER", label: "Screen off after (0 never)", type: "number", unit: "s", default: 90, min: 0, max: 3600 },
		{ name: "MP_TUI_DIM_LEVEL", label: "Dimmed backlight", type: "number", unit: "%", default: 15, min: 1, max: 100 },
		{ name: "MP_TUI_OFF_LEVEL", label: "Backlight while off (0 dark)", type: "number", unit: "%", default: 0, min: 0, max: 100 },
	],
}

function selected(ctx) {
	return fonts.find((font) => font.name === ctx.setting("MP_TUI_FONT")) || fonts[0]
}

/* ST7789's glass string is columns x rows before orientation. ILI932x is 240x320. */
function glassOf(ctx) {
	for (const device of ctx.devices) {
		const values = device.values || {}
		if (device.driver === "ST7789") {
			const glass = values.glass || "172x320"
			const parts = String(glass).split("x").map(Number)
			const columns = parts[0] || 172
			const rows = parts[1] || 320
			const orientation = values.orientation || "landscape"
			if (String(orientation).startsWith("portrait"))
				return { w: columns, h: rows }
			return { w: rows, h: columns }
		}
		if (device.driver === "ILI932X") {
			const orientation = values.orientation || "landscape"
			if (String(orientation).startsWith("portrait"))
				return { w: 240, h: 320 }
			return { w: 320, h: 240 }
		}
	}
	return { w: 240, h: 320 }
}

/* PUI's lockup at a 14 px em is phi 16 + gap 4 + "miniPhi" 81 = 101 px.
 * The em is chosen so that lockup is near 62% of the glass width. */
function splashType(w, h) {
	const ems = 101 / 14
	let margin = 8
	let word

	void h
	if (margin * 2 >= w)
		margin = w > 4 ? 2 : 0
	word = Math.round(((w - margin * 2) * 62) / 100 / ems)
	if (word < 14)
		word = 14
	if (word > 32)
		word = 32
	return word
}

export function prepare(ctx) {
	const font = selected(ctx)
	ctx.requireFile(here + font.file, "The default font file of common/tui is missing.")
	ctx.requireFile("data/miniphi-logo.png", "The miniPhi phi is missing.")
	ctx.requireFile("common/pui/fonts/anybody-expanded/AnybodyExpanded-Bold.ttf", "The splash wordmark is missing.")
}

export function generate(ctx) {
	const font = selected(ctx)
	const glass = glassOf(ctx)
	const wordSize = splashType(glass.w, glass.h)
	const { c } = ctx.compileFont(here + font.file, {
		symbol: "mp_tui_font_default",
		label: font.label,
		size: Number(ctx.setting("MP_TUI_FONT_SIZE")),
		bpp: Number(ctx.setting("MP_TUI_FONT_BPP")),
		charset: ctx.setting("MP_TUI_FONT_CHARSET"),
	})
	const word = ctx.compileFont("common/pui/fonts/anybody-expanded/AnybodyExpanded-Bold.ttf", {
		symbol: "mp_tui_font_wordmark",
		label: "Anybody Expanded Bold",
		size: wordSize,
		bpp: 4,
		charset: "ascii",
		chars: "miniPhiv.0123456789",
	})
	const logoH = Math.max(1, Math.floor((wordSize * 20 + 7) / 14))
	const mark = ctx.compileMark("data/miniphi-logo.png", { symbol: "mp_tui_mark", height: logoH })
	return {
		"include/gen/tui_font_default.h": c,
		"include/gen/tui_wordmark.h": word.c,
		"include/gen/tui_logo.h": mark.c,
	}
}
