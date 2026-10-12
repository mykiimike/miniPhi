const { contextBridge, ipcRenderer } = require("electron")

contextBridge.exposeInMainWorld("phi", {
	init: () => ipcRenderer.invoke("phi:init"),
	reload: () => ipcRenderer.invoke("phi:reload"),
	evaluate: (selection) => ipcRenderer.invoke("phi:evaluate", selection),
	board: (name, previous) => ipcRenderer.invoke("phi:board", name, previous),
	write: (selection) => ipcRenderer.invoke("phi:write", selection),
	make: (target) => ipcRenderer.invoke("phi:make", target),
	saveBoard: (request) => ipcRenderer.invoke("phi:save-board", request),
	browse: (current) => ipcRenderer.invoke("phi:browse", current),
	onMakeOutput: (callback) => ipcRenderer.on("phi:make-output", (_event, chunk) => callback(chunk)),
})
