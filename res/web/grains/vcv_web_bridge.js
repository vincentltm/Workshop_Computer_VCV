(function() {
    if (window.__vcv_web_bridge_installed) return;
    window.__vcv_web_bridge_installed = true;

    const urlParams = new URLSearchParams(window.location.search);
    const instance = urlParams.get('instance');
    const path = location.pathname.toLowerCase();

    // ── Determine Active Environment (VCV Rack WebSocket vs Patchnotes iframe / Wasm) ──
    const getWasmModule = function() {
        if (window.Module && typeof window.Module._send_midi_to_card === "function") return window.Module;
        if (window.parent && window.parent.Module && typeof window.parent.Module._send_midi_to_card === "function") return window.parent.Module;
        return null;
    };

    const isIframe = (window.parent && window.parent !== window);
    const isPatchnotes = isIframe || !!getWasmModule();
    console.log("[VCV WebBridge] Mode:", isPatchnotes ? "Patchnotes Web (iframe/Wasm)" : "VCV Rack Desktop WebSocket", "path:", location.pathname, "instance:", instance);

    // Exact card-specific primary device name required by each card's JS parser
    let cardDeviceName = "Workshop Computer MIDI";
    let isSerialCard = false;

    if (path.includes("clockwork")) {
        cardDeviceName = "Clockwork MIDI";
    } else if (path.includes("flux")) {
        cardDeviceName = "Flux Workshop Computer MIDI";
    } else if (path.includes("grains")) {
        cardDeviceName = "Grains RP2040 USB Serial";
        isSerialCard = true;
    } else if (path.includes("degenerator")) {
        cardDeviceName = "Degenerator RP2040 USB Serial";
        isSerialCard = true;
    } else if (path.includes("lens")) {
        cardDeviceName = "Lens Workshop Computer MIDI";
    } else if (path.includes("computer_grids")) {
        cardDeviceName = "Computer Grids MIDI";
    } else if (path.includes("cosmik")) {
        cardDeviceName = "Cosmik C1zzl3 MIDI";
    } else if (path.includes("fr330hfr33")) {
        cardDeviceName = "Fr330hfr33 MIDI";
    } else if (path.includes("fragments")) {
        cardDeviceName = "Fragments MTMComputer MIDI";
    } else if (path.includes("turing_matrix")) {
        cardDeviceName = "Turing Matrix Workshop Computer MIDI";
    } else if (path.includes("turing_machine")) {
        cardDeviceName = "MTMComputer Turing Machine MIDI";
    } else if (path.includes("usb_audio")) {
        cardDeviceName = "Workshop Computer Pico TinyUSB MIDI";
    } else if (path.includes("resonator")) {
        cardDeviceName = "Resonator Workshop Computer Serial";
        isSerialCard = true;
    } else if (path.includes("reverb")) {
        cardDeviceName = "Reverb Workshop Computer MIDI";
    } else if (path.includes("drumdrum")) {
        cardDeviceName = "Drumdrum Workshop Computer MIDI";
    }

    let ws = null;
    let wsReady = false;
    let midiListeners = [];
    let serialRxControllers = [];

    if (!isPatchnotes) {
        // ── VCV Rack WebSocket Connection ──
        const wsProtocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsHost = location.host || '127.0.0.1:8000';
        const wsUrl = `${wsProtocol}//${wsHost}/ws` + (instance ? `?instance=${encodeURIComponent(instance)}` : '');
        
        function connectWs() {
            try {
                ws = new WebSocket(wsUrl);
                ws.onopen = function() {
                    console.log("[VCV WebBridge] Connected to VCV Rack WebSocket Server at " + wsUrl);
                    wsReady = true;
                    if (window.onVcvBridgeConnected) window.onVcvBridgeConnected();
                };
                ws.onmessage = function(event) {
                    try {
                        const msg = JSON.parse(event.data);
                        if (msg.type === "midi" && Array.isArray(msg.data)) {
                            const dataArr = new Uint8Array(msg.data);
                            const evt = { data: dataArr };
                            midiListeners.forEach(cb => {
                                try { cb(evt); } catch(e) { console.error(e); }
                            });
                        } else if (msg.type === "serial" && Array.isArray(msg.data)) {
                            const dataArr = new Uint8Array(msg.data);
                            serialRxControllers.forEach(ctrl => {
                                try { ctrl.enqueue(dataArr); } catch(e) {}
                            });
                        }
                    } catch(e) {}
                };
                ws.onclose = function() {
                    wsReady = false;
                    setTimeout(connectWs, 2000);
                };
                ws.onerror = function() {
                    wsReady = false;
                };
            } catch(e) {
                console.error("[VCV WebBridge] WebSocket creation error:", e);
            }
        }
        connectWs();
    } else {
        // ── Patchnotes Wasm & postMessage Bridge Connection ──
        wsReady = true;

        // Listen for postMessage from parent Patchnotes window
        window.addEventListener('message', function(event) {
            const msg = event.data;
            if (!msg || !msg.type) return;
            if (msg.type === 'midi_from_card' && Array.isArray(msg.data)) {
                const dataArr = new Uint8Array(msg.data);
                const evt = { data: dataArr };
                midiListeners.forEach(cb => {
                    try { cb(evt); } catch(e) {}
                });
            } else if (msg.type === 'serial_from_card' && Array.isArray(msg.data)) {
                const dataArr = new Uint8Array(msg.data);
                serialRxControllers.forEach(ctrl => {
                    try { ctrl.enqueue(dataArr); } catch(e) {}
                });
            }
        });

        // Direct Wasm module memory polling if available
        setInterval(function() {
            const mod = getWasmModule();
            if (!mod) return;

            // Poll Wasm MIDI TX queue
            const midiBuf = mod._malloc(256);
            const midiRead = mod._read_midi_from_card(midiBuf, 256);
            if (midiRead > 0) {
                const dataArr = new Uint8Array(mod.HEAPU8.buffer, midiBuf, midiRead).slice();
                const evt = { data: dataArr };
                midiListeners.forEach(cb => {
                    try { cb(evt); } catch(e) {}
                });
            }
            mod._free(midiBuf);

            // Poll Wasm Serial TX queue
            const serialBuf = mod._malloc(256);
            const serialRead = mod._read_serial_from_card(serialBuf, 256);
            if (serialRead > 0) {
                const dataArr = new Uint8Array(mod.HEAPU8.buffer, serialBuf, serialRead).slice();
                serialRxControllers.forEach(ctrl => {
                    try { ctrl.enqueue(dataArr); } catch(e) {}
                });
            }
            mod._free(serialBuf);
        }, 10);
    }

    // Helper to send MIDI bytes
    function sendMidiBytes(bytes) {
        const arr = Array.from(bytes);
        if (isPatchnotes) {
            if (isIframe) {
                window.parent.postMessage({ type: 'midi_to_card', data: arr }, '*');
            }
            const mod = getWasmModule();
            if (mod) {
                for (let i = 0; i < bytes.length; i += 4) {
                    const b0 = bytes[i] || 0;
                    const b1 = bytes[i+1] || 0;
                    const b2 = bytes[i+2] || 0;
                    const b3 = bytes[i+3] || 0;
                    mod._send_midi_to_card(b0, b1, b2, b3);
                }
            }
        } else if (ws && wsReady) {
            ws.send(JSON.stringify({ type: "midi", data: arr }));
        }
    }

    // Helper to send Serial bytes
    function sendSerialBytes(bytes) {
        const arr = Array.from(bytes);
        if (isPatchnotes) {
            if (isIframe) {
                window.parent.postMessage({ type: 'serial_to_card', data: arr }, '*');
            }
            const mod = getWasmModule();
            if (mod) {
                const ptr = mod._malloc(bytes.length);
                mod.HEAPU8.set(bytes, ptr);
                mod._send_serial_to_card(ptr, bytes.length);
                mod._free(ptr);
            }
        } else if (ws && wsReady) {
            ws.send(JSON.stringify({ type: "serial", data: arr }));
        }
    }

    // ── 1. Polyfill WebMIDI ───────────────────────────────────────────────────
    const origRequestMIDIAccess = navigator.requestMIDIAccess ? navigator.requestMIDIAccess.bind(navigator) : null;

    navigator.requestMIDIAccess = async function(options) {
        let nativeAccess = null;
        try {
            if (origRequestMIDIAccess) {
                nativeAccess = await origRequestMIDIAccess(options);
            }
        } catch(e) {}

        function createVirtualInput(id, name) {
            const vInput = {
                id: id,
                name: name,
                manufacturer: "Music Thing Modular",
                state: "connected",
                type: "input",
                _onmidimessage: null,
                addEventListener: function(type, listener) {
                    if (type === 'midimessage') midiListeners.push(listener);
                },
                removeEventListener: function(type, listener) {
                    if (type === 'midimessage') {
                        midiListeners = midiListeners.filter(l => l !== listener);
                    }
                }
            };
            Object.defineProperty(vInput, 'onmidimessage', {
                get: function() { return this._onmidimessage; },
                set: function(fn) {
                    this._onmidimessage = fn;
                    if (fn && !midiListeners.includes(fn)) midiListeners.push(fn);
                }
            });
            return vInput;
        }

        function createVirtualOutput(id, name) {
            return {
                id: id,
                name: name,
                manufacturer: "Music Thing Modular",
                state: "connected",
                type: "output",
                send: function(data) {
                    sendMidiBytes(new Uint8Array(data));
                }
            };
        }

        const inputsMap = new Map();
        const outputsMap = new Map();

        if (nativeAccess && nativeAccess.inputs) {
            nativeAccess.inputs.forEach((val, key) => inputsMap.set(key, val));
            nativeAccess.outputs.forEach((val, key) => outputsMap.set(key, val));
        }

        // Add primary card-matched virtual port
        const pIn = createVirtualInput("vcv-card-midi-in", cardDeviceName);
        const pOut = createVirtualOutput("vcv-card-midi-out", cardDeviceName);
        inputsMap.set(pIn.id, pIn);
        outputsMap.set(pOut.id, pOut);

        // Add generic fallback virtual port
        if (cardDeviceName !== "Workshop Computer MIDI") {
            const fIn = createVirtualInput("vcv-fallback-midi-in", "Workshop Computer MIDI");
            const fOut = createVirtualOutput("vcv-fallback-midi-out", "Workshop Computer MIDI");
            inputsMap.set(fIn.id, fIn);
            outputsMap.set(fOut.id, fOut);
        }

        return {
            inputs: inputsMap,
            outputs: outputsMap,
            onstatechange: null
        };
    };

    // ── 2. Polyfill WebSerial ─────────────────────────────────────────────────
    if (!navigator.serial) {
        navigator.serial = {};
    }

    const virtualSerialPort = {
        getInfo: function() {
            return {
                usbVendorId: 0x2e8a, // Raspberry Pi Pico USB VID expected by Picoboot
                usbProductId: 0x000a,
                productName: cardDeviceName
            };
        },
        open: async function(options) {
            this.readable = new ReadableStream({
                start(controller) {
                    serialRxControllers.push(controller);
                },
                cancel() {
                    serialRxControllers = serialRxControllers.filter(c => c !== controller);
                }
            });
            this.writable = new WritableStream({
                write(chunk) {
                    sendSerialBytes(new Uint8Array(chunk));
                }
            });
            return Promise.resolve();
        },
        close: async function() {
            return Promise.resolve();
        }
    };

    const origRequestPort = navigator.serial.requestPort ? navigator.serial.requestPort.bind(navigator.serial) : null;
    navigator.serial.requestPort = async function(options) {
        try {
            if (origRequestPort) return await origRequestPort(options);
        } catch(e) {}
        return virtualSerialPort;
    };

    const origGetPorts = navigator.serial.getPorts ? navigator.serial.getPorts.bind(navigator.serial) : null;
    navigator.serial.getPorts = async function() {
        let nativePorts = [];
        try {
            if (origGetPorts) nativePorts = await origGetPorts();
        } catch(e) {}
        return [virtualSerialPort, ...nativePorts];
    };

})();
