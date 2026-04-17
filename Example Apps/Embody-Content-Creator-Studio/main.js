const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const { spawn, execFile, execFileSync } = require('child_process');
const http = require('http');
const Store = require('electron-store');

const store = new Store();

// ─── Paths ────────────────────────────────────────────────────────────────────

const SIGNALLING_DIR   = path.join(__dirname, 'signalling');
const SIGNALLING_ENTRY = path.join(SIGNALLING_DIR, 'dist', 'index.js');
const PLAYER_PORT   = 8080;
const STREAMER_PORT = 8888;

// ─── Child processes ──────────────────────────────────────────────────────────

let signalingProc = null;
let embodyProc    = null;
let mainWindow    = null;

const IS_WIN = process.platform === 'win32';

// ─── Helpers ──────────────────────────────────────────────────────────────────

function waitForPort(port, timeoutMs = 30000) {
    return new Promise((resolve, reject) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://localhost:${port}`, (res) => {
                res.resume();
                resolve();
            }).on('error', () => {
                if (Date.now() > deadline) {
                    reject(new Error(`Port ${port} not ready after ${timeoutMs}ms`));
                } else {
                    setTimeout(check, 500);
                }
            });
        };
        check();
    });
}

function getEmbodyExePath() {
    return store.get('embodyExePath', null);
}

function killProc(proc) {
    if (!proc) return;
    const pid = proc.pid;
    try { proc.kill(); } catch (_) {}
    if (pid) {
        if (IS_WIN) {
            try { execFileSync('taskkill', ['/F', '/T', '/PID', String(pid)], { timeout: 5000 }); } catch (_) {}
        } else {
            try { execFileSync('pkill', ['-KILL', '-P', String(pid)], { timeout: 5000 }); } catch (_) {}
        }
    }
}

function killChildren() {
    const ep = embodyProc;
    const sp = signalingProc;
    embodyProc    = null;
    signalingProc = null;
    killProc(ep);
    killProc(sp);
}

process.on('exit', killChildren);
process.on('SIGINT', () => { killChildren(); process.exit(0); });
process.on('SIGTERM', () => { killChildren(); process.exit(0); });

// ─── Start sequence ───────────────────────────────────────────────────────────

async function startSignallingServer() {
    if (!fs.existsSync(SIGNALLING_ENTRY)) {
        throw new Error(`SignallingWebServer not found at:\n${SIGNALLING_ENTRY}\n\nRun the setup script first.`);
    }

    signalingProc = spawn(process.execPath, [
        SIGNALLING_ENTRY,
        '--serve',
        `--player_port=${PLAYER_PORT}`,
        `--streamer_port=${STREAMER_PORT}`,
        '--http_root', path.join(SIGNALLING_DIR, 'www'),
        '--homepage', 'content-creator.html',
        '--console_messages', 'basic',
    ], {
        cwd: SIGNALLING_DIR,
        stdio: 'pipe',
    });

    signalingProc.stdout.on('data', d => console.log('[Signalling]', d.toString().trim()));
    signalingProc.stderr.on('data', d => console.error('[Signalling ERR]', d.toString().trim()));
    signalingProc.on('exit', code => console.log('[Signalling] exited with code', code));

    await waitForPort(PLAYER_PORT, 15000);
    console.log(`[Main] Signalling server ready on :${PLAYER_PORT}`);
}

function waitForStreamer(timeoutMs = 120000) {
    return new Promise((resolve) => {
        if (!signalingProc) { resolve(); return; }

        const timer = setTimeout(() => {
            signalingProc?.stdout.off('data', onData);
            console.log('[Main] Streamer connect timeout — showing window anyway');
            resolve();
        }, timeoutMs);

        const onData = (chunk) => {
            if (chunk.toString().includes('endpointIdConfirm')) {
                clearTimeout(timer);
                signalingProc?.stdout.off('data', onData);
                console.log('[Main] Streamer connected — opening window');
                resolve();
            }
        };

        signalingProc.stdout.on('data', onData);
    });
}

async function startEmbody(exePath) {
    embodyProc = spawn(exePath, [
        `-PixelStreamingConnectionURL=ws://localhost:${STREAMER_PORT}`,
        '-RenderOffScreen',
        '-Unattended',
        '-ForceRes',
        '-ResX=1920',
        '-ResY=1080',
        '-AudioMixer',
    ], {
        detached: false,
        stdio: 'pipe',
    });

    embodyProc.stdout.on('data', d => console.log('[Embody]', d.toString().trim()));
    embodyProc.stderr.on('data', d => console.error('[Embody ERR]', d.toString().trim()));
    embodyProc.on('exit', code => {
        console.log('[Embody] process exited with code', code);
        embodyProc = null;
    });

    console.log('[Main] Embody launched (PID', embodyProc.pid + ')');
}

// ─── Window ───────────────────────────────────────────────────────────────────

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1600,
        height: 960,
        minWidth: 1000,
        minHeight: 600,
        title: 'Embody Content Creator Studio',
        backgroundColor: '#0a0a0a',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
        },
        show: false,
    });

    mainWindow.once('ready-to-show', () => mainWindow.show());
    mainWindow.on('closed', () => { mainWindow = null; });

    mainWindow.loadURL(`http://localhost:${PLAYER_PORT}/content-creator.html`);
}

function showSetupWindow(errorMsg) {
    const setupWin = new BrowserWindow({
        width: 560,
        height: 340,
        title: 'Embody Content Creator Studio — Setup',
        backgroundColor: '#0a0a0a',
        resizable: false,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
        },
    });

    const html = `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0a0a0a;
    color: rgba(255,255,255,0.8);
    font-family: monospace;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    height: 100vh;
    gap: 20px;
    padding: 32px;
  }
  h2 { font-size: 14px; letter-spacing: 0.15em; text-transform: uppercase; color: #fff; }
  p { font-size: 12px; color: rgba(255,255,255,0.4); text-align: center; line-height: 1.6; }
  .error { font-size: 11px; color: #ff6b6b; text-align: center; }
  button {
    padding: 10px 28px;
    background: transparent;
    border: 1px solid rgba(255,255,255,0.3);
    color: rgba(255,255,255,0.7);
    font-family: monospace;
    font-size: 12px;
    letter-spacing: 0.1em;
    text-transform: uppercase;
    cursor: pointer;
    transition: all 0.2s;
  }
  button:hover { border-color: #fff; color: #fff; }
</style>
</head>
<body>
  <h2>Embody Content Creator Studio</h2>
  <p>Point the studio to your packaged Embody executable to get started.</p>
  ${errorMsg ? `<p class="error">${errorMsg}</p>` : ''}
  <button id="browse">Browse for Embody</button>
</body>
<script>
  document.getElementById('browse').addEventListener('click', () => {
    window.electronAPI.browseForExe();
  });
</script>
</html>`;

    setupWin.loadURL(`data:text/html;charset=utf-8,${encodeURIComponent(html)}`);
    return setupWin;
}

// ─── App lifecycle ────────────────────────────────────────────────────────────

app.whenReady().then(async () => {
    const exePath = getEmbodyExePath();

    if (!exePath || !fs.existsSync(exePath)) {
        showSetupWindow(exePath ? `Exe not found at stored path:\n${exePath}` : null);
        return;
    }

    try {
        await startSignallingServer();
        await startEmbody(exePath);
        await waitForStreamer();
        createWindow();
    } catch (err) {
        console.error('[Main] Startup failed:', err.message);
        showSetupWindow(err.message);
    }
});

app.on('window-all-closed', () => {
    killChildren();
    if (process.platform !== 'darwin') app.quit();
});

app.on('before-quit', killChildren);

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
});

// ─── IPC handlers ─────────────────────────────────────────────────────────────

// Save a recorded video clip to disk
ipcMain.handle('save-clip', async (_event, { defaultName, buffer, extension }) => {
    const ext = extension || 'webm';
    const { filePath, canceled } = await dialog.showSaveDialog(mainWindow, {
        title: 'Save Video Clip',
        defaultPath: defaultName ? `${defaultName}.${ext}` : `clip.${ext}`,
        filters: [
            { name: 'Video', extensions: [ext, 'webm', 'mp4'] },
        ],
    });
    if (canceled || !filePath) return { success: false };
    fs.writeFileSync(filePath, Buffer.from(buffer));
    return { success: true, filePath };
});

// Browse for Embody executable on first launch
ipcMain.handle('browse-for-exe', async () => {
    const filters = IS_WIN ? [{ name: 'Executable', extensions: ['exe'] }] : [];
    const { filePaths, canceled } = await dialog.showOpenDialog({
        title: 'Select Embody Executable',
        filters,
        properties: ['openFile'],
    });
    if (canceled || !filePaths.length) return;

    const exePath = filePaths[0];
    store.set('embodyExePath', exePath);

    BrowserWindow.getAllWindows().forEach(w => w.close());

    try {
        await startSignallingServer();
        await startEmbody(exePath);
        await waitForStreamer();
        createWindow();
    } catch (err) {
        showSetupWindow(err.message);
    }
});

// Restart Embody
ipcMain.handle('restart-embody', async () => {
    const exePath = getEmbodyExePath();
    if (!exePath) return { success: false, error: 'No exe path configured' };
    if (embodyProc) {
        try { embodyProc.kill(); } catch (_) {}
        embodyProc = null;
        await new Promise(r => setTimeout(r, 1000));
    }
    await startEmbody(exePath);
    return { success: true };
});

// Get current status
ipcMain.handle('get-status', () => ({
    embodyRunning: !!embodyProc,
    signallingRunning: !!signalingProc,
    exePath: getEmbodyExePath(),
}));
