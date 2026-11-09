import fs from "fs"
import zlib from "zlib"

function paeth(left, up, corner) {
	const guess = left + up - corner
	const dl = Math.abs(guess - left)
	const du = Math.abs(guess - up)
	const dc = Math.abs(guess - corner)

	if (dl <= du && dl <= dc)
		return left
	if (du <= dc)
		return up
	return corner
}

/** 8-bit PNG to a tightly packed RGBA buffer. */
export function decodePng(file) {
	const buf = fs.readFileSync(file)
	const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])

	if (buf.length < 8 || !buf.subarray(0, 8).equals(signature))
		throw new Error(file + " is not a PNG")
	let offset = 8
	let width = 0
	let height = 0
	let bitDepth = 0
	let colorType = -1
	const idat = []

	while (offset + 12 <= buf.length) {
		const length = buf.readUInt32BE(offset)
		const type = buf.toString("ascii", offset + 4, offset + 8)
		const data = buf.subarray(offset + 8, offset + 8 + length)

		if (type === "IHDR") {
			width = data.readUInt32BE(0)
			height = data.readUInt32BE(4)
			bitDepth = data[8]
			colorType = data[9]
		}
		else if (type === "IDAT")
			idat.push(data)
		else if (type === "IEND")
			break
		offset += 12 + length
	}
	const channels = { 0: 1, 2: 3, 4: 2, 6: 4 }[colorType]
	if (bitDepth !== 8 || !channels)
		throw new Error(file + " must be an 8-bit PNG")
	const raw = zlib.inflateSync(Buffer.concat(idat))
	const stride = width * channels
	const rgba = Buffer.alloc(width * height * 4)
	let previous = Buffer.alloc(stride)
	let src = 0

	for (let y = 0; y < height; y++) {
		const filter = raw[src++]
		const row = Buffer.alloc(stride)

		for (let index = 0; index < stride; index++) {
			const value = raw[src++]
			const left = index >= channels ? row[index - channels] : 0
			const up = previous[index]
			const corner = index >= channels ? previous[index - channels] : 0
			let sample = value

			if (filter === 1)
				sample = (value + left) & 255
			else if (filter === 2)
				sample = (value + up) & 255
			else if (filter === 3)
				sample = (value + ((left + up) >> 1)) & 255
			else if (filter === 4)
				sample = (value + paeth(left, up, corner)) & 255
			else if (filter !== 0)
				throw new Error(file + " has a PNG filter this rasterizer does not know")
			row[index] = sample
		}
		previous = row
		for (let x = 0; x < width; x++) {
			const index = x * channels
			const dest = (y * width + x) * 4

			if (channels === 4) {
				rgba[dest] = row[index]
				rgba[dest + 1] = row[index + 1]
				rgba[dest + 2] = row[index + 2]
				rgba[dest + 3] = row[index + 3]
			}
			else if (channels === 3) {
				rgba[dest] = row[index]
				rgba[dest + 1] = row[index + 1]
				rgba[dest + 2] = row[index + 2]
				rgba[dest + 3] = 255
			}
			else if (channels === 2) {
				rgba[dest] = rgba[dest + 1] = rgba[dest + 2] = row[index]
				rgba[dest + 3] = row[index + 1]
			}
			else {
				rgba[dest] = rgba[dest + 1] = rgba[dest + 2] = row[index]
				rgba[dest + 3] = 255
			}
		}
	}
	return { width, height, rgba }
}

function opaque(rgba, width, x, y) {
	return rgba[(y * width + x) * 4 + 3] >= 128
}

/**
 * Crops the opaque ink and scales it to height pixels, 1 bit, MSB first.
 * threshold is the fraction of a destination pixel that must be ink.
 */
export function compileBitmap(file, { symbol, widthName, heightName, height, threshold, source }) {
	const image = decodePng(file)
	const cut = threshold === undefined ? 0.35 : threshold
	let minX = image.width
	let minY = image.height
	let maxX = 0
	let maxY = 0

	for (let y = 0; y < image.height; y++) {
		for (let x = 0; x < image.width; x++) {
			if (!opaque(image.rgba, image.width, x, y))
				continue
			if (x < minX) minX = x
			if (y < minY) minY = y
			if (x > maxX) maxX = x
			if (y > maxY) maxY = y
		}
	}
	if (minX > maxX)
		throw new Error(file + " has no opaque pixels")
	const spanW = maxX - minX + 1
	const spanH = maxY - minY + 1
	const destH = height
	const destW = Math.max(1, Math.round(spanW * destH / spanH))
	const on = []

	for (let y = 0; y < destH; y++) {
		const row = []
		const sy0 = minY + y * spanH / destH
		const sy1 = minY + (y + 1) * spanH / destH

		for (let x = 0; x < destW; x++) {
			const sx0 = minX + x * spanW / destW
			const sx1 = minX + (x + 1) * spanW / destW
			let lit = 0
			let total = 0

			for (let sy = Math.floor(sy0); sy < Math.min(image.height, Math.ceil(sy1)); sy++) {
				for (let sx = Math.floor(sx0); sx < Math.min(image.width, Math.ceil(sx1)); sx++) {
					total++
					if (opaque(image.rgba, image.width, sx, sy))
						lit++
				}
			}
			row.push(total > 0 && lit / total >= cut)
		}
		on.push(row)
	}
	const stride = Math.ceil(destW / 8)
	const bytes = []

	for (let y = 0; y < destH; y++) {
		for (let group = 0; group < stride; group++) {
			let value = 0

			for (let bit = 0; bit < 8; bit++) {
				const x = group * 8 + bit

				if (x < destW && on[y][x])
					value |= 0x80 >> bit
			}
			bytes.push(value)
		}
	}
	const hex = []

	for (let index = 0; index < bytes.length; index += 12)
		hex.push("\t" + bytes.slice(index, index + 12).map((byte) => "0x" + byte.toString(16).padStart(2, "0")).join(", ") + ",")
	const c = [
		"/* generated from " + (source || file) + " — do not edit */",
		"#define " + widthName + " " + destW,
		"#define " + heightName + " " + destH,
		"static const unsigned char " + symbol + "[] = {",
		hex.join("\n"),
		"};",
		"",
	].join("\n")
	return { c, width: destW, height: destH }
}
