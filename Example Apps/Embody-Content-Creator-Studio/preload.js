const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
    // Save a recorded video clip via native file save dialog
    // buffer: ArrayBuffer of the clip data, extension: 'webm' or 'mp4'
    saveClip: (defaultName, buffer, extension) =>
        ipcRenderer.invoke('save-clip', { defaultName, buffer, extension }),

    // Trigger the exe browse dialog (used from the setup screen)
    browseForExe: () =>
        ipcRenderer.invoke('browse-for-exe'),

    // Restart the Embody process
    restartEmbody: () =>
        ipcRenderer.invoke('restart-embody'),

    // Get running status
    getStatus: () =>
        ipcRenderer.invoke('get-status'),

    // Check if running inside Electron (vs browser)
    isElectron: true,
});
