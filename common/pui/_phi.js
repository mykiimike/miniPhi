const here = "common/pui/"

export const phi = {
	kind: "module",
	name: "PUI",
	label: "Pager UI",
	description: "A page-scrolled UI for a small screen, driven by 1-2 buttons, rendered band by band. Widgets speak RGB888 and the display picks the band depth, 1, 16, or 24 bits per pixel. The SSD1306 band stays 1-bit. No pointer, no framebuffer. Body text is Silkscreen (a pixel font drawn for small OLEDs) and titles are Anybody Expanded bold, rasterized when build/ is written.",
	define: "SUPPORT_COMMON_PUI",
	requires: ["DEVICE"],
	sources: ["pui.c", "task.c", "widgets.c", "pages/about.c", "pages/cpu.c", "pages/temp.c", "pages/clock.c", "pages/charge.c", "pages/volt.c", "pages/measure.c", "pages/faces.c", "pui_font.c", "pui_font_default.c"],
	headers: ["pui.h", "pui_font.h", "pages/pages.h"],
	/* host program, compiled by make test, absent from the firmware */
	tests: ["test/color.c"],
}

export function prepare(ctx) {
	ctx.requireFile(here + "fonts/silkscreen/Silkscreen-Regular.ttf", "The PUI body font is missing.")
	ctx.requireFile(here + "fonts/anybody-expanded/AnybodyExpanded-Bold.ttf", "The PUI emphasis font is missing.")
	ctx.requireFile("data/miniphi-logo.png", "The miniPhi logo is missing.")
}

export function generate(ctx) {
	const regular = ctx.compileFont(here + "fonts/silkscreen/Silkscreen-Regular.ttf", {
		symbol: "mp_pui_font_default",
		label: "Silkscreen",
		size: 8,
		bpp: 1,
		charset: "ascii",
		types: "mp_pui",
	})
	const bold = ctx.compileFont(here + "fonts/anybody-expanded/AnybodyExpanded-Bold.ttf", {
		symbol: "mp_pui_font_bold",
		label: "Anybody Expanded Bold",
		size: 10,
		bpp: 1,
		charset: "ascii",
		types: "mp_pui",
	})
	const wordmark = ctx.compileFont(here + "fonts/anybody-expanded/AnybodyExpanded-Bold.ttf", {
		symbol: "mp_pui_font_wordmark",
		label: "Anybody Expanded Bold",
		size: 14,
		bpp: 1,
		charset: "ascii",
		types: "mp_pui",
	})
	const logo = ctx.compileBitmap("data/miniphi-logo.png", {
		symbol: "mp_pui_logo_bits",
		widthName: "MP_PUI_LOGO_W",
		heightName: "MP_PUI_LOGO_H",
		height: 20,
	})
	return {
		"include/gen/pui_font_default.h": regular.c + "\n" + bold.c + "\n" + wordmark.c,
		"include/gen/pui_logo.h": logo.c,
	}
}
