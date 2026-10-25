export const phi = {
	kind: "module",
	name: "DMA",
	label: "DMA",
	description: "Life of a buffer moved by an opaque engine. A chip driver asks the bus it already holds. map, start, done, unmap. The CPU copy is a task; a controller or a bus master supplies its own engine. One segment and no cyclic mode until that engine says otherwise.",
	define: "SUPPORT_COMMON_DMA",
	sources: ["dma.c"],
	headers: ["dma.h"],
}
