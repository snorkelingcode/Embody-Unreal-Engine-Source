const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
    // Save an animation (.emanim / .json) via native file save dialog
    saveAnimation: (defaultName, data) =>
        ipcRenderer.invoke('save-animation', { defaultName, data }),

    // Load an animation from disk via native file open dialog
    loadAnimation: () =>
        ipcRenderer.invoke('load-animation'),

    // Trigger the exe browse dialog (used from the setup screen)
    browseForExe: () =>
        ipcRenderer.invoke('browse-for-exe'),

    // Restart the Embody.exe process
    restartEmbody: () =>
        ipcRenderer.invoke('restart-embody'),

    // Get running status
    getStatus: () =>
        ipcRenderer.invoke('get-status'),

    // Check if running inside Electron (vs browser)
    isElectron: true,
});
