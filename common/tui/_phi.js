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
	description: "A user interface for small screens. On start it shows miniPhi, the logo and the version together, then a home of applications declared by the firmware. Opening one shows a scrollable page and a Back button. With INPUT, every touch panel, mouse and keyboard clicks, drags, drops and scrolls them. Things are drawn in bands into any RGB565 display: no framebuffer, and several screens at once. The default font and the logo are rasterized when build/ is written.",
	define: "SUPPORT_COMMON_TUI",
	sources: [
		"tui.c", "draw.c", "font.c", "task.c", "font_default.c",
		"widgets/box.c", "widgets/label.c", "widgets/bar.c", "widgets/table.c",
		"widgets/columns.c", "widgets/window.c", "widgets/button.c", "widgets/switch.c",
		"widgets/view.c", "widgets/page.c", "widgets/named.c",
	],
	headers: ["tui.h", "font.h"],
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
	],
}

function selected(ctx) {
	return fonts.find((font) => font.name === ctx.setting("MP_TUI_FONT")) || fonts[0]
}

export function prepare(ctx) {
	const font = selected(ctx)
	ctx.requireFile(here + font.file, "The default font file of common/tui is missing.")
	ctx.requireFile("config/assets/logo.svg", "The TUI splash logo is missing.")
}

export function generate(ctx) {
	const font = selected(ctx)
	const { c } = ctx.compileFont(here + font.file, {
		symbol: "mp_tui_font_default",
		label: font.label,
		size: Number(ctx.setting("MP_TUI_FONT_SIZE")),
		bpp: Number(ctx.setting("MP_TUI_FONT_BPP")),
		charset: ctx.setting("MP_TUI_FONT_CHARSET"),
	})
	const logo = ctx.compileLogo("config/assets/logo.svg", { symbol: "mp_tui_logo", size: 96 })
	return {
		"include/gen/tui_font_default.h": c,
		"include/gen/tui_logo.h": logo.c,
	}
}
