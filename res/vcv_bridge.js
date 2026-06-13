(function() {
    console.log("VCV Rack WebMIDI & WebSerial Shim loaded.");
    const params = new URLSearchParams(window.location.search);
    const instance = params.get('instance');
    if (!instance) {
        console.warn("No VCV module instance pointer provided in query parameters.");
        return;
    }

    // Determine WebSocket port from current location
    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${wsProtocol}//${window.location.host}/ws?instance=${instance}`;
    const ws = new WebSocket(wsUrl);

    // Message queues for connection initialization period
    const midiSendQueue = [];
    const serialSendQueue = [];

    // Track active MIDI listeners
    const midiListeners = new Set();
    const isDrumDrum = window.location.pathname.indexOf("drumdrum") !== -1;
    const deviceName = isDrumDrum ? "MTMComputer Input (DrumDrum)" : "MTMComputer Input";
    const deviceNameOut = isDrumDrum ? "MTMComputer Output (DrumDrum)" : "MTMComputer Output";

    const mockMidiInput = {
        name: deviceName,
        id: "vcv_midi_in",
        type: "input",
        state: "connected",
        connection: "open",
        open() {
            return Promise.resolve(this);
        },
        close() {
            return Promise.resolve(this);
        },
        addEventListener(type, listener) {
            if (type === 'midimessage') midiListeners.add(listener);
        },
        removeEventListener(type, listener) {
            if (type === 'midimessage') midiListeners.delete(listener);
        },
        _onmidimessage: null,
        get onmidimessage() {
            return this._onmidimessage;
        },
        set onmidimessage(handler) {
            if (this._onmidimessage) {
                midiListeners.delete(this._onmidimessage);
            }
            this._onmidimessage = handler;
            if (handler) {
                midiListeners.add(handler);
            }
        }
    };

    const mockMidiOutput = {
        name: deviceNameOut,
        id: "vcv_midi_out",
        type: "output",
        state: "connected",
        connection: "open",
        open() {
            return Promise.resolve(this);
        },
        close() {
            return Promise.resolve(this);
        },
        send(data, timestamp) {
            if (ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: "midi",
                    data: Array.from(data)
                }));
            } else {
                midiSendQueue.push(Array.from(data));
            }
        }
    };

    // Override requestMIDIAccess to return maps
    const mockMidiAccess = {
        inputs: {
            values() { return [mockMidiInput]; },
            forEach(callback) { callback(mockMidiInput, "vcv_midi_in"); },
            get(id) { return id === "vcv_midi_in" ? mockMidiInput : null; },
            has(id) { return id === "vcv_midi_in"; }
        },
        outputs: {
            values() { return [mockMidiOutput]; },
            forEach(callback) { callback(mockMidiOutput, "vcv_midi_out"); },
            get(id) { return id === "vcv_midi_out" ? mockMidiOutput : null; },
            has(id) { return id === "vcv_midi_out"; }
        },
        sysexEnabled: true,
        addEventListener(type, listener) {},
        removeEventListener(type, listener) {}
    };

    // Override requestMIDIAccess directly, waiting for WebSocket connection to open first
    navigator.requestMIDIAccess = function(options) {
        if (ws.readyState === WebSocket.OPEN) {
            return Promise.resolve(mockMidiAccess);
        } else {
            return new Promise((resolve) => {
                ws.addEventListener('open', () => {
                    resolve(mockMidiAccess);
                }, { once: true });
            });
        }
    };

    // Override navigator.serial for WebSerial compatibility
    let serialController = null;
    let serialReadable = null;
    let serialWritable = null;
    let nornsWs = null;
    let nornsMode = 'binary';

    function createSerialReadable() {
        return new ReadableStream({
            start(controller) {
                serialController = controller;
            },
            cancel() {
                serialController = null;
            }
        });
    }

    const mockSerialPort = {
        open(options) {
            console.log("Mock serial port opened:", options);
            serialReadable = createSerialReadable();
            serialWritable = new WritableStream({
                write(chunk) {
                    if (ws.readyState === WebSocket.OPEN) {
                        ws.send(JSON.stringify({
                            type: "serial",
                            data: Array.from(chunk)
                        }));
                    } else {
                        serialSendQueue.push(Array.from(chunk));
                    }
                    return Promise.resolve();
                }
            });
            return Promise.resolve();
        },
        close() {
            if (serialController) {
                try { serialController.close(); } catch(e) {}
                serialController = null;
            }
            serialWritable = null;
            serialReadable = null;
            return Promise.resolve();
        },
        get readable() {
            return serialReadable;
        },
        get writable() {
            return serialWritable;
        },
        getInfo() {
            return { usbVendorId: 0x2e8a, usbProductId: 0x000a };
        }
    };

    const mockSerialAccess = {
        getPorts() { return Promise.resolve([mockSerialPort]); },
        requestPort(options) { return Promise.resolve(mockSerialPort); },
        addEventListener(type, listener) {},
        removeEventListener(type, listener) {}
    };

    if (typeof navigator.serial === 'undefined') {
        Object.defineProperty(navigator, 'serial', {
            value: mockSerialAccess,
            configurable: true,
            writable: true
        });
    } else {
        try {
            Object.defineProperty(navigator.serial, 'getPorts', { value: () => Promise.resolve([mockSerialPort]), configurable: true, writable: true });
            Object.defineProperty(navigator.serial, 'requestPort', { value: (options) => Promise.resolve(mockSerialPort), configurable: true, writable: true });
        } catch(e) {
            console.error("Failed to define property on navigator.serial directly, trying to overwrite navigator.serial", e);
            try {
                Object.defineProperty(navigator, 'serial', {
                    value: mockSerialAccess,
                    configurable: true,
                    writable: true
                });
            } catch(err) {
                console.error("Failed to override navigator.serial", err);
            }
        }
    }

    // Handle incoming WebSocket messages
    ws.onmessage = function(event) {
        try {
            const msg = JSON.parse(event.data);
            if (msg.type === 'midi') {
                const midiEvent = {
                    data: new Uint8Array(msg.data),
                    receivedTime: performance.now(),
                    target: mockMidiInput
                };
                for (const listener of midiListeners) {
                    try { listener(midiEvent); } catch(e) { console.error(e); }
                }
            } else if (msg.type === 'serial') {
                console.log("WebSerial data received from VCV:", msg.data);
                if (nornsWs && nornsWs.readyState === WebSocket.OPEN) {
                    const dataArray = new Uint8Array(msg.data);
                    if (nornsMode === 'text') {
                        nornsWs.send(new TextDecoder().decode(dataArray));
                    } else {
                        nornsWs.send(dataArray);
                    }
                }
                if (serialController) {
                    serialController.enqueue(new Uint8Array(msg.data));
                }
            }
        } catch(e) {
            console.error("WebSocket message parsing error:", e);
        }
    };

    ws.onopen = function() {
        console.log("WebSocket connection established with VCV Rack.");
        while (midiSendQueue.length > 0) {
            ws.send(JSON.stringify({
                type: "midi",
                data: midiSendQueue.shift()
            }));
        }
        while (serialSendQueue.length > 0) {
            ws.send(JSON.stringify({
                type: "serial",
                data: serialSendQueue.shift()
            }));
        }
    };

    ws.onclose = function() {
        console.log("WebSocket connection closed.");
    };

    // Intercept Picoboot module for virtual flashing
    try {
        import("./lib/picoflash/index.js").then((m) => {
            console.log("Mocking Picoboot flashing library...");
            m.Picoboot.requestDevice = function() {
                const mockPicobootInstance = {
                    connect() { return Promise.resolve(); },
                    disconnect() { return Promise.resolve(); },
                    isConnected() { return true; },
                    reboot(delay) {
                        console.log("Mock reboot requested");
                        return Promise.resolve();
                    },
                    flashEraseAndWrite(address, data, progressCallback) {
                        console.log("Mock Picoboot flashing address:", address, data.byteLength);
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.send(JSON.stringify({
                                type: "flash",
                                address: address,
                                data: Array.from(new Uint8Array(data))
                            }));
                        }
                        
                        return new Promise((resolve) => {
                            let pct = 0;
                            const interval = setInterval(() => {
                                pct += 10;
                                if (progressCallback) {
                                    progressCallback(pct, `Flashing simulated memory... (${pct}%)`);
                                }
                                if (pct >= 100) {
                                    clearInterval(interval);
                                    resolve();
                                }
                            }, 50);
                        });
                    }
                };
                return Promise.resolve(mockPicobootInstance);
            };
        }).catch(err => {
            // It's fine if the card doesn't have Picoboot
        });
    } catch (e) {
        // Safe catch for browsers that don't support dynamic import in this context
    }

    // --- NORNS BRIDGE INJECTION ---
    const isBlackbird = window.location.pathname.indexOf("blackbird") !== -1 ||
                        window.location.pathname.indexOf("krell") !== -1 ||
                        window.location.pathname.indexOf("duo_midi") !== -1;
    if (isBlackbird) {
        window.addEventListener('DOMContentLoaded', () => {
            const headerLeft = document.querySelector('.header-left');
            if (!headerLeft) {
                console.warn("Norns Bridge: .header-left element not found, skipping UI injection");
                return;
            }

            // Create styles
            const style = document.createElement('style');
            style.textContent = `
                #nornsPopover {
                    position: absolute;
                    top: 35px;
                    left: 210px;
                    background-color: var(--bg-surface);
                    border: 1px solid var(--neutral-trim);
                    border-radius: 4px;
                    padding: 12px;
                    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
                    z-index: 2000;
                    min-width: 240px;
                    font-family: monospace;
                    display: none;
                }
                #nornsPopover.visible {
                    display: block;
                }
                .norns-popover-title {
                    font-size: 0.9rem;
                    font-weight: bold;
                    color: var(--neutral-heavy);
                    margin-bottom: 10px;
                    border-bottom: 1px solid var(--neutral-trim);
                    padding-bottom: 6px;
                    display: flex;
                    justify-content: space-between;
                    align-items: center;
                }
                .norns-close-btn {
                    background: none;
                    border: none;
                    color: var(--neutral-medium);
                    cursor: pointer;
                    font-size: 1.2rem;
                    line-height: 1;
                }
                .norns-close-btn:hover {
                    color: var(--interactive-selected);
                }
                .norns-field-group {
                    display: flex;
                    flex-direction: column;
                    gap: 4px;
                    margin-bottom: 8px;
                }
                .norns-field-label {
                    font-size: 0.75rem;
                    color: var(--neutral-medium);
                    text-transform: lowercase;
                }
                .norns-input {
                    background-color: var(--bg-default);
                    border: 1px solid var(--neutral-trim);
                    color: var(--neutral-heavy);
                    font-family: monospace;
                    font-size: 0.85rem;
                    padding: 4px 8px;
                    border-radius: 2px;
                    outline: none;
                }
                .norns-input:focus {
                    border-color: var(--interactive-selected);
                }
                .norns-row-group {
                    display: flex;
                    gap: 8px;
                    margin-bottom: 12px;
                }
                .norns-row-group .norns-field-group {
                    flex: 1;
                    margin-bottom: 0;
                }
                .norns-connect-btn {
                    background-color: var(--neutral-trim);
                    border: 1px solid var(--neutral-trim);
                    color: var(--neutral-heavy);
                    font-family: monospace;
                    font-size: 0.85rem;
                    padding: 6px 12px;
                    cursor: pointer;
                    width: 100%;
                    transition: background-color 0.2s, color 0.2s;
                    border-radius: 2px;
                }
                .norns-connect-btn:hover {
                    background-color: var(--bg-subdued);
                    color: var(--interactive-selected);
                }
                .norns-connect-btn.connected {
                    background-color: #2ed573;
                    color: #1e1e1e;
                    font-weight: bold;
                    border-color: #2ed573;
                }
                .norns-status-row {
                    display: flex;
                    align-items: center;
                    gap: 8px;
                    font-size: 0.8rem;
                    margin-top: 8px;
                    color: var(--neutral-medium);
                }
                .norns-status-dot {
                    width: 6px;
                    height: 6px;
                    border-radius: 50%;
                    background-color: var(--neutral-medium);
                }
                .norns-status-dot.connecting {
                    background-color: #ffd700;
                    animation: nornsPulse 1s infinite alternate;
                }
                .norns-status-dot.connected {
                    background-color: #2ed573;
                    box-shadow: 0 0 6px #2ed573;
                }
                .repl-connection-btn.norns-btn-active {
                    color: #2ed573 !important;
                    font-weight: bold;
                }
                @keyframes nornsPulse {
                    from { opacity: 0.4; }
                    to { opacity: 1; }
                }
            `;
            document.head.appendChild(style);

            // Create norns button
            const nornsBtn = document.createElement('button');
            nornsBtn.id = 'nornsBtn';
            nornsBtn.className = 'repl-connection-btn';
            nornsBtn.textContent = 'norns';
            headerLeft.appendChild(nornsBtn);

            // Create popover
            const popover = document.createElement('div');
            popover.id = 'nornsPopover';
            popover.innerHTML = `
                <div class="norns-popover-title">
                    <span>Norns Bridge</span>
                    <button class="norns-close-btn" id="nornsCloseBtn">&times;</button>
                </div>
                <div class="norns-field-group">
                    <label class="norns-field-label">Norns IP / Host</label>
                    <input type="text" id="nornsHost" class="norns-input" value="norns.local">
                </div>
                <div class="norns-row-group">
                    <div class="norns-field-group">
                        <label class="norns-field-label">Port</label>
                        <input type="text" id="nornsPort" class="norns-input" value="5556">
                    </div>
                    <div class="norns-field-group">
                        <label class="norns-field-label">Mode</label>
                        <select id="nornsMode" class="norns-input" style="height:25px;">
                            <option value="binary">Binary</option>
                            <option value="text">Text</option>
                        </select>
                    </div>
                </div>
                <button class="norns-connect-btn" id="nornsConnectBtn">Connect</button>
                <div class="norns-status-row">
                    <div class="norns-status-dot" id="nornsStatusDot"></div>
                    <span id="nornsStatusText">Disconnected</span>
                </div>
            `;
            headerLeft.appendChild(popover);

            // UI event listeners
            nornsBtn.addEventListener('click', () => {
                popover.classList.toggle('visible');
            });

            document.getElementById('nornsCloseBtn').addEventListener('click', () => {
                popover.classList.remove('visible');
            });

            const connectBtn = document.getElementById('nornsConnectBtn');
            const statusDot = document.getElementById('nornsStatusDot');
            const statusText = document.getElementById('nornsStatusText');
            const hostInput = document.getElementById('nornsHost');
            const portInput = document.getElementById('nornsPort');
            const modeSelect = document.getElementById('nornsMode');

            function updateUIState(state) {
                if (state === 'disconnected') {
                    statusDot.className = 'norns-status-dot';
                    statusText.textContent = 'Disconnected';
                    connectBtn.textContent = 'Connect';
                    connectBtn.classList.remove('connected');
                    nornsBtn.classList.remove('norns-btn-active');
                    hostInput.disabled = false;
                    portInput.disabled = false;
                    modeSelect.disabled = false;
                } else if (state === 'connecting') {
                    statusDot.className = 'norns-status-dot connecting';
                    statusText.textContent = 'Connecting...';
                    connectBtn.textContent = 'Cancel';
                    hostInput.disabled = true;
                    portInput.disabled = true;
                    modeSelect.disabled = true;
                } else if (state === 'connected') {
                    statusDot.className = 'norns-status-dot connected';
                    statusText.textContent = 'Connected';
                    connectBtn.textContent = 'Disconnect';
                    connectBtn.classList.add('connected');
                    nornsBtn.classList.add('norns-btn-active');
                }
            }

            connectBtn.addEventListener('click', () => {
                if (nornsWs) {
                    console.log("Norns Bridge: disconnecting...");
                    nornsWs.close();
                    nornsWs = null;
                    return;
                }

                const host = hostInput.value.trim();
                const port = portInput.value.trim();
                nornsMode = modeSelect.value;

                if (!host || !port) {
                    alert("Please enter host and port.");
                    return;
                }

                updateUIState('connecting');
                console.log(`Norns Bridge: connecting to ws://${host}:${port} (${nornsMode} mode)`);

                try {
                    nornsWs = new WebSocket(`ws://${host}:${port}`);
                    if (nornsMode === 'binary') {
                        nornsWs.binaryType = 'arraybuffer';
                    }

                    nornsWs.onopen = () => {
                        console.log("Norns Bridge: connected successfully!");
                        updateUIState('connected');
                    };

                    nornsWs.onclose = () => {
                        console.log("Norns Bridge: connection closed.");
                        nornsWs = null;
                        updateUIState('disconnected');
                    };

                    nornsWs.onerror = (err) => {
                        console.error("Norns Bridge error:", err);
                        if (serialController) {
                            serialController.enqueue(new TextEncoder().encode("\n[norns bridge error: failed to connect]\n"));
                        }
                    };

                    nornsWs.onmessage = (event) => {
                        let bytes;
                        if (event.data instanceof ArrayBuffer) {
                            bytes = new Uint8Array(event.data);
                        } else if (event.data instanceof Blob) {
                            event.data.arrayBuffer().then(buf => {
                                const b = new Uint8Array(buf);
                                forwardNornsToVcv(b);
                            });
                            return;
                        } else if (typeof event.data === 'string') {
                            bytes = new TextEncoder().encode(event.data);
                        }

                        if (bytes) {
                            forwardNornsToVcv(bytes);
                        }
                    };

                    function forwardNornsToVcv(bytes) {
                        // Forward to VCV Rack
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.send(JSON.stringify({
                                type: "serial",
                                data: Array.from(bytes)
                            }));
                        }
                        // Echo to local serial controller (druid app terminal view)
                        if (serialController) {
                            serialController.enqueue(bytes);
                        }
                    }

                } catch (e) {
                    console.error("Norns Bridge connection exception:", e);
                    updateUIState('disconnected');
                    nornsWs = null;
                }
            });
        });
    }
    // -------------------------------
})();
