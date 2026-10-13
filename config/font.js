import fs from "fs"
import opentype from "opentype.js"

/* code point ranges per charset, inclusive */
export const charsets = {
	ascii: [[0x20, 0x7e]],
	latin1: [[0x20, 0x7e], [0xa0, 0xff]],
}

const samples = 8

function flatten(commands) {
	const contours = []
	let contour = null
	let x = 0
	let y = 0
	const point = (px, py) => {
		contour.push([px, py])
		x = px
		y = py
	}
	for (const command of commands) {
		if (command.type === "M") {
			contour = []
			contours.push(contour)
			point(command.x, command.y)
		}
		else if (command.type === "L")
			point(command.x, command.y)
		else if (command.type === "Q") {
			const [x0, y0] = [x, y]
			for (let step = 1; step <= 8; step++) {
				const t = step / 8
				const u = 1 - t
				point(u * u * x0 + 2 * u * t * command.x1 + t * t * command.x, u * u * y0 + 2 * u * t * command.y1 + t * t * command.y)
			}
		}
		else if (command.type === "C") {
			const [x0, y0] = [x, y]
			for (let step = 1; step <= 12; step++) {
				const t = step / 12
				const u = 1 - t
				point(
					u * u * u * x0 + 3 * u * u * t * command.x1 + 3 * u * t * t * command.x2 + t * t * t * command.x,
					u * u * u * y0 + 3 * u * u * t * command.y1 + 3 * u * t * t * command.y2 + t * t * t * command.y,
				)
			}
		}
	}
	return contours.filter((item) => item.length > 2)
}

/* coverage of each pixel, 0 to samples², non-zero winding */
function rasterize(contours) {
	let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity
	for (const contour of contours)
		for (const [px, py] of contour) {
			minX = Math.min(minX, px)
			minY = Math.min(minY, py)
			maxX = Math.max(maxX, px)
			maxY = Math.max(maxY, py)
		}
	if (!contours.length || maxX - minX <= 0 || maxY - minY <= 0)
		return { x: 0, y: 0, w: 0, h: 0, coverage: [] }

	const x0 = Math.floor(minX)
	const y0 = Math.floor(minY)
	const w = Math.ceil(maxX) - x0
	const h = Math.ceil(maxY) - y0
	const coverage = new Array(w * h).fill(0)

	const edges = []
	for (const contour of contours)
		for (let index = 0; index < contour.length; index++) {
			const [xa, ya] = contour[index]
			const [xb, yb] = contour[(index + 1) % contour.length]
			if (ya !== yb)
				edges.push({ xa, ya, xb, yb, dir: yb > ya ? 1 : -1 })
		}

	for (let row = 0; row < h * samples; row++) {
		const sy = y0 + (row + 0.5) / samples
		const crossings = []
		for (const edge of edges) {
			const low = Math.min(edge.ya, edge.yb)
			const high = Math.max(edge.ya, edge.yb)
			if (sy < low || sy >= high)
				continue
			crossings.push({ x: edge.xa + ((sy - edge.ya) * (edge.xb - edge.xa)) / (edge.yb - edge.ya), dir: edge.dir })
		}
		crossings.sort((a, b) => a.x - b.x)

		const pixelRow = Math.floor(row / samples) * w
		let winding = 0
		for (let index = 0; index < crossings.length - 1; index++) {
			winding += crossings[index].dir
			if (!winding)
				continue
			const first = Math.max(0, Math.ceil((crossings[index].x - x0) * samples - 0.5))
			const last = Math.min(w * samples, Math.ceil((crossings[index + 1].x - x0) * samples - 0.5))
			for (let column = first; column < last; column++)
				coverage[pixelRow + Math.floor(column / samples)]++
		}
	}
	return { x: x0, y: y0, w, h, coverage }
}

function pack(levels, bpp, bytes) {
	let current = 0
	let used = 0
	for (const level of levels) {
		current = (current << bpp) | level
		used += bpp
		if (used === 8) {
			bytes.push(current)
			current = 0
			used = 0
		}
	}
	if (used)
		bytes.push(current << (8 - used))
}

function hex(bytes) {
	const lines = []
	for (let index = 0; index < bytes.length; index += 16)
		lines.push("\t" + bytes.slice(index, index + 16).map((byte) => "0x" + byte.toString(16).padStart(2, "0")).join(", ") + ",")
	return lines.join("\n")
}

/**
 * Rasterizes a TrueType or OpenType file into the mp_tui_font_t layout of common/tui/font.h.
 * size is the em size in pixels. Returns the C source and a short summary.
 */
export function compileFont(file, { symbol, size, bpp, charset, label, source }) {
	const data = fs.readFileSync(file)
	const font = opentype.parse(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength))
	const scale = size / font.unitsPerEm
	const ascent = Math.ceil(font.ascender * scale)
	const descent = Math.ceil(-font.descender * scale)
	const gap = Math.max(0, Math.round(((font.tables.hhea && font.tables.hhea.lineGap) || 0) * scale))
	const maxLevel = (1 << bpp) - 1
	const threshold = (samples * samples * 2) / 5

	const bytes = []
	const ranges = []
	let missing = 0
	for (const [first, last] of charsets[charset]) {
		const glyphs = []
		for (let code = first; code <= last; code++) {
			const glyph = font.charToGlyph(String.fromCodePoint(code))
			if (!glyph || glyph.index === 0) {
				missing++
				glyphs.push({ offset: 0, w: 0, h: 0, dx: 0, dy: 0, advance: 0 })
				continue
			}
			const shape = rasterize(flatten(glyph.getPath(0, 0, size).commands))
			const levels = shape.coverage.map((value) =>
				bpp === 1 ? (value >= threshold ? 1 : 0) : Math.round((value * maxLevel) / (samples * samples)))
			const advance = Math.round(glyph.advanceWidth * scale)
			if (shape.w > 255 || shape.h > 255 || shape.x < -128 || shape.x > 127 || shape.y < -128 || shape.y > 127 || advance > 255)
				throw new Error(label + " at " + size + " px does not fit the glyph table (U+" + code.toString(16) + ")")
			glyphs.push({ offset: bytes.length, w: shape.w, h: shape.h, dx: shape.x, dy: shape.y, advance })
			pack(levels, bpp, bytes)
		}
		ranges.push({ first, glyphs })
	}

	const glyphTables = ranges.map((range, index) => [
		"static const mp_tui_glyph_t _glyphs" + index + "[] = {",
		...range.glyphs.map((glyph, code) =>
			"\t{ " + [glyph.offset + "u", glyph.w, glyph.h, glyph.dx, glyph.dy, glyph.advance].join(", ") + " }, /* U+" +
			(range.first + code).toString(16).toUpperCase().padStart(4, "0") + " */"),
		"};",
		"",
	].join("\n"))

	const c = [
		"/* Generated by miniPhi config. Do not edit. */",
		"/* " + label + ", " + size + " px, " + bpp + " bpp, " + charset + ", from " + source + " */",
		"",
		"static const uint8_t _bitmap[] = {",
		hex(bytes.length ? bytes : [0]),
		"};",
		"",
		...glyphTables,
		"static const mp_tui_font_range_t _ranges[] = {",
		...ranges.map((range, index) => "\t{ 0x" + range.first.toString(16) + ", " + range.glyphs.length + ", _glyphs" + index + " },"),
		"};",
		"",
		"const mp_tui_font_t " + symbol + " = {",
		"\t" + JSON.stringify(label) + ",",
		"\t" + size + ", " + bpp + ", " + (ascent + descent + gap) + ", " + ascent + ",",
		"\t" + ranges.length + ", _ranges, _bitmap",
		"};",
		"",
	].join("\n")

	return { c, bytes: bytes.length, missing, lineHeight: ascent + descent + gap }
}
