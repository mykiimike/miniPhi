import fs from "fs"

const samples = 4

function attributes(text) {
	const values = {}
	for (const match of text.matchAll(/([\w:-]+)\s*=\s*"([^"]*)"/g))
		values[match[1]] = match[2]
	return values
}

function color(text) {
	const hex = text.replace("#", "")
	const full = hex.length === 3 ? hex.split("").map((digit) => digit + digit).join("") : hex
	return [parseInt(full.slice(0, 2), 16), parseInt(full.slice(2, 4), 16), parseInt(full.slice(4, 6), 16)]
}

function number(values, name, fallback = 0) {
	return values[name] === undefined ? fallback : Number(values[name])
}

/* two semicircle arcs back to the start: an axis-aligned ellipse */
function ellipses(path) {
	const found = []
	const pattern = /M\s*([\d.]+)\s+([\d.]+)\s*a\s*([\d.]+)\s+([\d.]+)\s+0\s+1\s+0\s+([\d.]+)\s+0/g
	for (const match of path.matchAll(pattern)) {
		const x = Number(match[1])
		const y = Number(match[2])
		const rx = Number(match[3])
		const ry = Number(match[4])
		found.push({ cx: x + rx, cy: y, rx, ry })
	}
	return found
}

function shapesOf(file) {
	const text = fs.readFileSync(file, "utf8")
	const view = text.match(/viewBox="([^"]+)"/)
	if (!view)
		throw new Error(file + " has no viewBox")
	const [minX, minY, width, height] = view[1].split(/[\s,]+/).map(Number)
	const shapes = []
	const fill = []
	for (const match of text.matchAll(/<(\/?)([a-zA-Z]+)([^>]*?)(\/?)>/g)) {
		const closing = match[1] === "/"
		const name = match[2]
		const values = attributes(match[3])
		if (name === "g") {
			if (closing)
				fill.pop()
			else
				fill.push(values.fill ? color(values.fill) : fill[fill.length - 1])
			continue
		}
		if (closing || name === "svg")
			continue
		const paint = values.fill ? color(values.fill) : fill[fill.length - 1]
		if (!paint)
			continue
		if (name === "rect")
			shapes.push({
				kind: "rect",
				x: number(values, "x"), y: number(values, "y"),
				w: number(values, "width"), h: number(values, "height"),
				rx: number(values, "rx"),
				fill: paint,
			})
		else if (name === "circle")
			shapes.push({ kind: "circle", cx: number(values, "cx"), cy: number(values, "cy"), r: number(values, "r"), fill: paint })
		else if (name === "path") {
			const rings = ellipses(values.d || "")
			if (!rings.length)
				throw new Error(file + " has a path the splash rasterizer does not know")
			shapes.push({ kind: "rings", rings, fill: paint })
		}
		else
			throw new Error(file + " has a <" + name + "> the splash rasterizer does not know")
	}
	return { minX, minY, width, height, shapes }
}

function covers(shape, x, y) {
	if (shape.kind === "circle") {
		const dx = x - shape.cx
		const dy = y - shape.cy
		return dx * dx + dy * dy <= shape.r * shape.r
	}
	if (shape.kind === "rings") {
		let inside = 0
		for (const ring of shape.rings) {
			const dx = (x - ring.cx) / ring.rx
			const dy = (y - ring.cy) / ring.ry
			if (dx * dx + dy * dy <= 1)
				inside++
		}
		return inside % 2 === 1
	}
	let rx = shape.rx
	if (rx > shape.w / 2)
		rx = shape.w / 2
	if (rx > shape.h / 2)
		rx = shape.h / 2
	const localX = x - shape.x
	const localY = y - shape.y
	if (localX < 0 || localY < 0 || localX >= shape.w || localY >= shape.h)
		return false
	if (!rx)
		return true
	let cornerX = 0
	let cornerY = 0
	if (localX < rx)
		cornerX = rx
	else if (localX > shape.w - rx)
		cornerX = shape.w - rx
	if (localY < rx)
		cornerY = rx
	else if (localY > shape.h - rx)
		cornerY = shape.h - rx
	if (!cornerX && !cornerY)
		return true
	if (!cornerX || !cornerY)
		return true
	const dx = localX - cornerX
	const dy = localY - cornerY
	return dx * dx + dy * dy <= rx * rx
}

function rgb565(red, green, blue) {
	return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
}

/**
 * Rasterizes the splash logo. The SVG may use filled rects, circles and
 * even-odd ellipse rings, which is what config/assets/logo.svg uses.
 * Returns RGB565 pixels and a 4-bit alpha, high nibble first.
 */
export function rasterLogo(file, size) {
	const picture = shapesOf(file)
	const pixels = []
	const alpha = []
	let byte = 0

	for (let y = 0; y < size; y++) {
		for (let x = 0; x < size; x++) {
			let red = 0
			let green = 0
			let blue = 0
			let covered = 0
			for (let sy = 0; sy < samples; sy++) {
				for (let sx = 0; sx < samples; sx++) {
					const px = picture.minX + ((x + (sx + 0.5) / samples) * picture.width) / size
					const py = picture.minY + ((y + (sy + 0.5) / samples) * picture.height) / size
					let paint = null
					for (const shape of picture.shapes)
						if (covers(shape, px, py))
							paint = shape.fill
					if (!paint)
						continue
					red += paint[0]
					green += paint[1]
					blue += paint[2]
					covered++
				}
			}
			const total = samples * samples
			const level = Math.round((covered / total) * 15)
			if (covered) {
				red = Math.round(red / covered)
				green = Math.round(green / covered)
				blue = Math.round(blue / covered)
			}
			pixels.push(rgb565(red, green, blue))
			byte = (byte << 4) | level
			if (((y * size + x) & 1) === 1) {
				alpha.push(byte)
				byte = 0
			}
		}
	}
	if ((size * size) & 1)
		alpha.push(byte << 4)
	return { width: size, height: size, pixels, alpha }
}

function hexBytes(bytes) {
	const lines = []
	for (let index = 0; index < bytes.length; index += 16)
		lines.push("\t" + bytes.slice(index, index + 16).map((byte) => "0x" + byte.toString(16).padStart(2, "0")).join(", ") + ",")
	return lines.join("\n")
}

function hexPixels(pixels) {
	const lines = []
	for (let index = 0; index < pixels.length; index += 12)
		lines.push("\t" + pixels.slice(index, index + 12).map((pixel) => "0x" + pixel.toString(16).padStart(4, "0")).join(", ") + ",")
	return lines.join("\n")
}

/** Rasterizes an SVG into the mp_tui_bitmap_t layout. size is the width and height in pixels. */
export function compileLogo(file, { symbol, size, source }) {
	const image = rasterLogo(file, size)
	const c = [
		"/* generated from " + (source || file) + " — do not edit */",
		"static const mp_tui_color_t _logo_pixels[] = {",
		hexPixels(image.pixels),
		"};",
		"",
		"static const uint8_t _logo_alpha[] = {",
		hexBytes(image.alpha),
		"};",
		"",
		"const mp_tui_bitmap_t " + symbol + " = { " + image.width + ", " + image.height + ", _logo_pixels, _logo_alpha };",
		"",
	].join("\n")
	return { c, width: image.width, height: image.height }
}
