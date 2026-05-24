document.addEventListener('DOMContentLoaded', () => {
    window.hardwareLimits = { master: 0, slaves: [] };
    // UI Elements
    const btnPower = document.getElementById('btnPower');
    const btnSettings = document.getElementById('btnSettings');
    const briSlider = document.getElementById('briSlider');
    const whiteSlider = document.getElementById('whiteSlider');
    const speedSlider = document.getElementById('speedSlider');
    const effectsGrid = document.getElementById('effectsGrid');
    
    const EFFECT_NAMES = [
        "Solid", "Breathe", "Rainbow", "Chase", "Fire", "Color Wipe", "Scanner", "Twinkle", "Meteor", "Matrix Rain"
    ];
    
    // Generate effect buttons
    effectsGrid.innerHTML = '';
    EFFECT_NAMES.forEach((name, idx) => {
        const btn = document.createElement('button');
        btn.className = 'effect-btn';
        btn.dataset.id = idx;
        btn.innerText = name;
        effectsGrid.appendChild(btn);
    });
    
    const effectBtns = document.querySelectorAll('.effect-btn');
    
    // Screens & Modals
    const mainControls = document.getElementById('mainControls');
    const apSetupScreen = document.getElementById('apSetupScreen');
    const settingsModal = document.getElementById('settingsModal');
    const closeSettings = document.getElementById('closeSettings');
    const apWifiContainer = document.getElementById('apWifiContainer');
    const modalWifiContainer = document.getElementById('modalWifiContainer');
    const imageUpload = document.getElementById('imageUpload');
    const btnUploadImage = document.getElementById('btnUploadImage');
    const pixelCanvas = document.getElementById('pixelCanvas');
    const btnStreamMatrix = document.getElementById('btnStreamMatrix');
    
    // Tabs
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');

    // Settings UI
    const ledCount = document.getElementById('ledCount');
    const ledType = document.getElementById('ledType');
    const ablEnable = document.getElementById('ablEnable');
    const ablMaxMa = document.getElementById('ablMaxMa');
    const ablRecommendedText = document.getElementById('ablRecommendedText');
    const matrixEnable = document.getElementById('matrixEnable');
    const matrixConfigGroup = document.getElementById('matrixConfigGroup');
    const matrixWidth = document.getElementById('matrixWidth');
    const matrixHeight = document.getElementById('matrixHeight');
    const matrixLayout = document.getElementById('matrixLayout');
    const ledPins = [
        document.getElementById('ledPin0'),
        document.getElementById('ledPin1'),
        document.getElementById('ledPin2'),
        document.getElementById('ledPin3'),
        document.getElementById('ledPin4')
    ];
    const groupPins = [
        document.getElementById('groupPin0'),
        document.getElementById('groupPin1'),
        document.getElementById('groupPin2'),
        document.getElementById('groupPin3'),
        document.getElementById('groupPin4')
    ];
    const btnSaveConfig = document.getElementById('btnSaveConfig');
    
    // WiFi Elements
    const btnScanNetworks = document.getElementById('btnScanNetworks');
    const wifiList = document.getElementById('wifiList');
    const wifiSelect = document.getElementById('wifiSelect');
    const wifiSsid = document.getElementById('wifiSsid');
    const wifiPass = document.getElementById('wifiPass');
    const btnSaveWifi = document.getElementById('btnSaveWifi');

    // MQTT Elements
    const mqttEnable = document.getElementById('mqttEnable');
    const mqttServer = document.getElementById('mqttServer');
    const mqttPort = document.getElementById('mqttPort');
    const mqttUser = document.getElementById('mqttUser');
    const mqttPass = document.getElementById('mqttPass');
    const mqttTopic = document.getElementById('mqttTopic');
    const btnTestMqtt = document.getElementById('btnTestMqtt');
    const btnSaveMqtt = document.getElementById('btnSaveMqtt');

    // Segments Elements
    const segmentSelector = document.getElementById('segmentSelector');
    const segmentsListContainer = document.getElementById('segmentsListContainer');
    const btnAddSegment = document.getElementById('btnAddSegment');
    const btnSaveSegments = document.getElementById('btnSaveSegments');
    const btnSegPower = document.getElementById('btnSegPower');
    const btnSegSettings = document.getElementById('btnSegSettings');

    // System Elements
    const sysVersion = document.getElementById('sysVersion');
    const btnCheckUpdate = document.getElementById('btnCheckUpdate');
    const btnSaveButtons = document.getElementById('btnSaveButtons');
    const btn1Active = document.getElementById('btn1Active');
    const btn1Type = document.getElementById('btn1Type');
    const btn2Active = document.getElementById('btn2Active');
    const btn2Type = document.getElementById('btn2Type');
    const btnUpdateLocal = document.getElementById('btnUpdateLocal');
    const updateFile = document.getElementById('updateFile');
    const fileUploadLabel = document.getElementById('fileUploadLabel');
    const updateStatus = document.getElementById('updateStatus');
    const btnFactoryReset = document.getElementById('btnFactoryReset');
    const otaProgressContainer = document.getElementById('otaProgressContainer');
    const otaProgressBar = document.getElementById('otaProgressBar');

    updateFile.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
            let names = Array.from(e.target.files).map(f => f.name).join(' + ');
            fileUploadLabel.innerText = names;
            fileUploadLabel.style.color = '#10b981';
            fileUploadLabel.style.borderColor = '#10b981';
        } else {
            fileUploadLabel.innerText = 'Datei auswählen';
            fileUploadLabel.style.color = '';
            fileUploadLabel.style.borderColor = '';
        }
    });

    // State
    let segments = [];
    let currentSegmentId = 0;
    
    let state = {
        on: true,
        bri: 255,
        effect: 0,
        color: '#ff0000',
        white: 0,
        speed: 128
    };
    
    let isAPMode = false;
    let isInteracting = false;

    function throttle(func, limit) {
        let lastRan = 0;
        let lastFunc;
        return function() {
            const context = this;
            const args = arguments;
            const now = Date.now();
            if (now - lastRan >= limit) {
                func.apply(context, args);
                lastRan = now;
            } else {
                clearTimeout(lastFunc);
                lastFunc = setTimeout(function() {
                    func.apply(context, args);
                    lastRan = Date.now();
                }, limit - (now - lastRan));
            }
        }
    }

    const throttledSendState = throttle((updates) => {
        sendState(updates);
    }, 150);

    // Initialization Flow
    fetchStatus().then(() => {
        if (isAPMode) {
            mainControls.style.display = 'none';
            btnSettings.style.display = 'none';
            btnPower.style.display = 'none';
            apSetupScreen.style.display = 'block';
            apWifiContainer.appendChild(modalWifiContainer);
            modalWifiContainer.style.display = 'block';
        } else {
            apSetupScreen.style.display = 'none';
            fetchState();
            setInterval(fetchState, 2000);
            fetchConfig();
            fetchMqtt();
        }
    });

    // Initialize iro.js ColorPicker
    const colorWheel = new iro.ColorPicker("#colorWheel", {
        width: 250,
        color: state.color,
        borderWidth: 2,
        borderColor: "rgba(255,255,255,0.2)",
        layout: [
            { component: iro.ui.Wheel }
        ]
    });

    const setInteracting = () => isInteracting = true;
    const clearInteracting = () => {
        isInteracting = false;
    };

    colorWheel.on('input:start', setInteracting);
    
    colorWheel.on('color:change', (color) => {
        state.color = color.hexString;
        // ensure throttle is defined before this is called!
        // wait, we defined throttle below... we need to move throttle up!
        if (typeof throttledSendState !== 'undefined') {
            throttledSendState({ color: state.color });
        }
    });

    colorWheel.on('input:end', (color) => {
        isInteracting = false;
        state.color = color.hexString;
        sendState({ color: state.color });
    });

    briSlider.addEventListener('mousedown', setInteracting);
    briSlider.addEventListener('touchstart', setInteracting, {passive: true});
    briSlider.addEventListener('mouseup', clearInteracting);
    briSlider.addEventListener('touchend', clearInteracting, {passive: true});

    whiteSlider.addEventListener('mousedown', setInteracting);
    whiteSlider.addEventListener('touchstart', setInteracting, {passive: true});
    whiteSlider.addEventListener('mouseup', clearInteracting);
    whiteSlider.addEventListener('touchend', clearInteracting, {passive: true});

    speedSlider.addEventListener('mousedown', setInteracting);
    speedSlider.addEventListener('touchstart', setInteracting, {passive: true});
    speedSlider.addEventListener('mouseup', clearInteracting);
    speedSlider.addEventListener('touchend', clearInteracting, {passive: true});

    // Quick Swatches
    const swatches = document.querySelectorAll('.color-swatch');
    swatches.forEach(swatch => {
        swatch.addEventListener('click', (e) => {
            const newColor = e.target.getAttribute('data-color');
            colorWheel.color.hexString = newColor;
            state.color = newColor;
            sendState({ color: state.color });
        });
    });

    function hasWhiteChannel(type) {
        const whiteTypes = [30, 32, 33, 34, 35, 36, 37, 41, 42, 44, 45];
        return whiteTypes.includes(type);
    }

    function updatePinUI() {
        const type = parseInt(ledType.value);
        
        // Toggle white slider
        const whiteSliderWrapper = document.getElementById('whiteSliderWrapper');
        if (whiteSliderWrapper) {
            whiteSliderWrapper.style.display = hasWhiteChannel(type) ? 'block' : 'none';
        }

        let numPins = 1;
        let labels = ["Data Pin"];

        // Logic for digital vs PWM
        if (type >= 50 && type <= 54) { // 2-Wire SPI LEDs
            numPins = 2;
            labels = ["Data Pin", "Clock Pin"];
        } 
        else if (type == 40) { numPins = 1; labels = ["On/Off Pin"]; }
        else if (type == 41) { numPins = 1; labels = ["White Pin"]; }
        else if (type == 42) { numPins = 2; labels = ["Warm", "Kalt"]; }
        else if (type == 43) { numPins = 3; labels = ["Rot", "Grün", "Blau"]; }
        else if (type == 44) { numPins = 4; labels = ["Rot", "Grün", "Blau", "Weiß"]; }
        else if (type == 45) { numPins = 5; labels = ["Rot", "Grün", "Blau", "Warm", "Kalt"]; }

        for(let i=0; i<5; i++) {
            if(i < numPins) {
                groupPins[i].style.display = 'block';
                document.getElementById('labelPin' + i).innerText = labels[i] || ("Pin " + i);
            } else {
                groupPins[i].style.display = 'none';
            }
        }
    }

    ledType.addEventListener('change', updatePinUI);

    function updateAblUI() {
        const count = parseInt(ledCount.value) || 30;
        const totalA = ((count * 55) / 1000).toFixed(1);
        ablRecommendedText.innerHTML = `Total LEDs: ${count}<br>Empfohlenes Netzteil für maximales Weiß: <strong style="color: #fff;">~${totalA} A</strong>`;
    }
    
    ledCount.addEventListener('input', updateAblUI);
    ledCount.addEventListener('change', updateAblUI);

    // Modal Logic
    btnSettings.addEventListener('click', () => {
        settingsModal.classList.add('show');
    });

    closeSettings.addEventListener('click', () => {
        settingsModal.classList.remove('show');
    });

    window.addEventListener('click', (e) => {
        if (e.target === settingsModal) {
            settingsModal.classList.remove('show');
        }
    });

    // Tab Logic
    tabBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            tabBtns.forEach(btn => btn.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));
            
            e.target.classList.add('active');
            const targetId = e.target.getAttribute('data-tab');
            document.getElementById(targetId).classList.add('active');
            
            if (targetId === 'tab-slaves') {
                fetchSlaves();
            }
            if (targetId === 'tab-segments') {
                fetchHardwareLimitsForSegments(() => renderSegmentsSettings());
            }
            if (targetId === 'modalWifiContainer') {
                fetchWlanStatus();
            }
        });
    });

    function fetchHardwareLimitsForSegments(callback) {
        fetch('/api/slaves')
            .then(res => res.json())
            .then(data => {
                window.hardwareLimits.master = parseInt(ledCount.value) || 30;
                window.hardwareLimits.slaves = data.filter(s => s.id !== 254);
                if (callback) callback();
            })
            .catch(() => { console.error("Could not fetch slaves limits"); });
    }

    // Event Listeners - State
    btnPower.addEventListener('click', () => {
        const anyOn = segments.some(s => s.on);
        const newState = !anyOn;
        
        segments.forEach(s => s.on = newState);
        state.on = newState;
        updateUI();
        
        if (btnSegPower) {
            if (segments[currentSegmentId].on) btnSegPower.classList.add('on');
            else btnSegPower.classList.remove('on');
        }
        
        const updates = segments.map((s, idx) => ({ id: idx, on: newState }));
        fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ seg: updates })
        }).catch(e => console.error("Global power error", e));
    });

    if (btnSegPower) {
        btnSegPower.addEventListener('click', () => {
            state.on = !state.on;
            updateUI();
            sendState({ on: state.on });
        });
    }

    if (btnSegSettings) {
        btnSegSettings.addEventListener('click', () => {
            fetchHardwareLimitsForSegments(() => {
                renderSegmentsSettings();
                settingsModal.classList.add('show');
                document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
                document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
                document.querySelector('.tab-btn[data-tab="tab-segments"]').classList.add('active');
                document.getElementById('tab-segments').classList.add('active');
            });
        });
    }

    const syncCheckbox = document.getElementById('syncSegments');
    if (syncCheckbox) {
        syncCheckbox.addEventListener('change', (e) => {
            if (e.target.checked) {
                const activeSeg = segments[currentSegmentId];
                if (!activeSeg) return;
                
                let segUpdates = [];
                for (let i = 0; i < segments.length; i++) {
                    segments[i].on = activeSeg.on;
                    segments[i].bri = activeSeg.bri;
                    segments[i].effect = activeSeg.effect;
                    segments[i].speed = activeSeg.speed !== undefined ? activeSeg.speed : 128;
                    segments[i].color = activeSeg.color;
                    
                    // Convert color to hex for sending if it's stored as int
                    let c = segments[i].color;
                    if (typeof c === 'number') {
                        c = c.toString(16);
                        while(c.length < 6) c = "0" + c;
                        c = "#" + c.substring(c.length - 6);
                    }
                    
                    segUpdates.push({
                        id: i,
                        on: segments[i].on,
                        bri: segments[i].bri,
                        effect: segments[i].effect,
                        speed: segments[i].speed,
                        color: c
                    });
                }
                
                fetch('/api/state', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ sync: true, seg: segUpdates })
                });
                
                updateUI();
            }
        });
    }

    briSlider.addEventListener('input', (e) => {
        state.bri = parseInt(e.target.value);
        throttledSendState({ bri: state.bri });
    });
    
    briSlider.addEventListener('change', (e) => {
        state.bri = parseInt(e.target.value);
        sendState({ bri: state.bri });
    });
    
    whiteSlider.addEventListener('input', (e) => {
        state.white = parseInt(e.target.value);
        throttledSendState({ white: state.white });
    });
    
    whiteSlider.addEventListener('change', (e) => {
        state.white = parseInt(e.target.value);
        sendState({ white: state.white });
    });

    effectsGrid.addEventListener('click', (e) => {
        if(e.target.classList.contains('effect-btn')) {
            state.effect = parseInt(e.target.dataset.id);
            updateUI();
            sendState({ effect: state.effect });
        }
    });

    speedSlider.addEventListener('mousedown', setInteracting);
    speedSlider.addEventListener('touchstart', setInteracting, {passive: true});
    speedSlider.addEventListener('mouseup', clearInteracting);
    speedSlider.addEventListener('touchend', clearInteracting, {passive: true});

    speedSlider.addEventListener('input', (e) => {
        state.speed = parseInt(e.target.value);
        throttledSendState({ speed: state.speed });
    });
    
    speedSlider.addEventListener('change', (e) => {
        state.speed = parseInt(e.target.value);
        sendState({ speed: state.speed });
    });

    // Event Listeners - Config
    btnSaveConfig.addEventListener('click', () => {
        const payload = {
            pins: ledPins.map(el => parseInt(el.value)),
            count: parseInt(ledCount.value),
            type: parseInt(ledType.value),
            abl_en: ablEnable.checked,
            abl_ma: parseInt(ablMaxMa.value)
        };
        
        fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        }).then(res => {
            if(res.ok) {
                const originalText = btnSaveConfig.innerText;
                const originalBg = btnSaveConfig.style.backgroundColor;
                btnSaveConfig.innerText = 'Gespeichert!';
                btnSaveConfig.style.backgroundColor = '#10b981';
                setTimeout(() => {
                    btnSaveConfig.innerText = originalText;
                    btnSaveConfig.style.backgroundColor = originalBg || '';
                    settingsModal.classList.remove('show');
                }, 1000);
            }
        });
    });

    // Event Listeners - WiFi
    btnScanNetworks.addEventListener('click', () => {
        btnScanNetworks.innerText = 'Scanne...';
        btnScanNetworks.disabled = true;
        fetch('/api/scan').then(() => {
            setTimeout(fetchScanResults, 3000);
        });
    });

    function fetchScanResults() {
        fetch('/api/scan_results')
            .then(res => res.json())
            .then(data => {
                if (data.length === 0) {
                    setTimeout(fetchScanResults, 2000);
                    return;
                }
                btnScanNetworks.innerText = 'Netzwerke Scannen';
                btnScanNetworks.disabled = false;
                
                let html = '<option>Wähle...</option>';
                data.forEach(net => {
                    const opt = document.createElement('option');
                    opt.value = net.ssid;
                    opt.text = `${net.ssid} (${net.rssi}dBm)`;
                    wifiSelect.appendChild(opt);
                });
                wifiList.style.display = 'block';
            });
    }

    wifiSelect.addEventListener('change', (e) => {
        if(e.target.value !== 'Wähle...') {
            wifiSsid.value = e.target.value;
        }
    });

    btnSaveWifi.addEventListener('click', () => {
        const formData = new URLSearchParams();
        formData.append('ssid', wifiSsid.value);
        formData.append('password', wifiPass.value);
        
        btnSaveWifi.innerText = 'Speichern...';
        fetch('/api/save_wifi', {
            method: 'POST',
            body: formData
        }).then(res => {
            if(res.ok) {
                const originalText = btnSaveWifi.innerText;
                btnSaveWifi.innerText = 'Gespeichert!';
                btnSaveWifi.style.backgroundColor = '#10b981';
                setTimeout(() => {
                    btnSaveWifi.innerText = originalText;
                    btnSaveWifi.style.backgroundColor = '';
                    settingsModal.classList.remove('show');
                }, 1500);
            }
        });
    });

    // Event Listeners - MQTT
    btnSaveMqtt.addEventListener('click', () => {
        const payload = new URLSearchParams();
        payload.append('enabled', mqttEnable.checked ? 'true' : 'false');
        payload.append('server', mqttServer.value);
        payload.append('port', mqttPort.value);
        payload.append('user', mqttUser.value);
        payload.append('pass', mqttPass.value);
        payload.append('topic', mqttTopic.value);
        
        btnSaveMqtt.innerText = 'Speichern...';
        fetch('/api/mqtt', {
            method: 'POST',
            body: payload
        }).then(res => {
            if(res.ok) {
                const originalText = btnSaveMqtt.innerText;
                btnSaveMqtt.innerText = 'Gespeichert! Neustart...';
                btnSaveMqtt.style.backgroundColor = '#10b981';
                setTimeout(() => {
                    btnSaveMqtt.innerText = originalText;
                    btnSaveMqtt.style.backgroundColor = '';
                    settingsModal.classList.remove('show');
                    location.reload();
                }, 2000);
            }
        }).catch(e => {
            const originalText = btnSaveMqtt.innerText;
            btnSaveMqtt.innerText = 'Gespeichert! Neustart...';
            btnSaveMqtt.style.backgroundColor = '#10b981';
            setTimeout(() => {
                btnSaveMqtt.innerText = originalText;
                btnSaveMqtt.style.backgroundColor = '';
                settingsModal.classList.remove('show');
                location.reload();
            }, 2000);
        });
    });

    // Event Listeners - Segments
    function renderSegmentsSettings() {
        segmentsListContainer.innerHTML = '';
        segments.forEach((seg, idx) => {
            const div = document.createElement('div');
            div.style.padding = '10px';
            div.style.background = 'rgba(255,255,255,0.05)';
            div.style.borderRadius = '8px';
            div.style.display = 'flex';
            div.style.alignItems = 'center';
            div.style.gap = '10px';
            div.style.flexWrap = 'wrap';
            
            let minStart = 1;
            let maxStop = window.hardwareLimits.master || 30;
            let currentTarget = "master";
            
            if (seg.isSlave) {
                currentTarget = "slave_" + seg.slaveId;
                let offset = window.hardwareLimits.master || 30;
                for (let i = 0; i < window.hardwareLimits.slaves.length; i++) {
                    const s = window.hardwareLimits.slaves[i];
                    if (s.id === seg.slaveId) {
                        minStart = offset + 1;
                        maxStop = offset + s.ledCount;
                        break;
                    }
                    offset += s.ledCount;
                }
            }
            
            let targetOptions = `<option style="background: #1f2937; color: white;" value="master" ${currentTarget === 'master' ? 'selected' : ''}>Master</option>`;
            if (window.hardwareLimits && window.hardwareLimits.slaves) {
                window.hardwareLimits.slaves.forEach(s => {
                    const val = "slave_" + s.id;
                    const displayName = s.name !== 'Unknown' && s.name ? s.name : 'Slave ' + s.id;
                    targetOptions += `<option style="background: #1f2937; color: white;" value="${val}" ${currentTarget === val ? 'selected' : ''}>${displayName}</option>`;
                });
            }
            
            div.innerHTML = `
                <div style="flex: 1; min-width: 120px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">Zielgerät</label>
                    <select class="seg-target" data-idx="${idx}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                        ${targetOptions}
                    </select>
                </div>
                <div style="flex: 2; min-width: 120px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">Name</label>
                    <input type="text" class="seg-name" data-idx="${idx}" value="${seg.name || ('Segment ' + idx)}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                </div>
                <div style="flex: 1; min-width: 80px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">Start (${minStart}-${maxStop})</label>
                    <input type="number" class="seg-start" data-idx="${idx}" min="${minStart}" max="${maxStop}" value="${seg.start + 1}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                </div>
                <div style="flex: 1; min-width: 80px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">Stop (${minStart}-${maxStop})</label>
                    <input type="number" class="seg-stop" data-idx="${idx}" min="${minStart}" max="${maxStop}" value="${seg.stop}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                </div>
                <button class="icon-btn btn-del-seg" data-idx="${idx}" style="color: #ff4757; background: rgba(255,71,87,0.1); margin-top: 15px; width: 34px; height: 34px; flex-shrink: 0;">
                    <svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round" style="pointer-events: none;">
                        <polyline points="3 6 5 6 21 6"></polyline>
                        <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
                    </svg>
                </button>
            `;
            segmentsListContainer.appendChild(div);
        });
        
        document.querySelectorAll('.seg-target').forEach(i => i.addEventListener('change', e => {
            const idx = e.target.dataset.idx;
            const val = e.target.value;
            if (val === 'master') {
                segments[idx].isSlave = false;
                segments[idx].slaveId = 0;
                segments[idx].start = 0;
                segments[idx].stop = window.hardwareLimits.master || 30;
            } else {
                segments[idx].isSlave = true;
                const newId = parseInt(val.replace('slave_', ''));
                segments[idx].slaveId = newId;
                
                let offset = window.hardwareLimits.master || 30;
                for (let j = 0; j < window.hardwareLimits.slaves.length; j++) {
                    const s = window.hardwareLimits.slaves[j];
                    if (s.id === newId) {
                        segments[idx].start = offset;
                        segments[idx].stop = offset + s.ledCount;
                        break;
                    }
                    offset += s.ledCount;
                }
            }
            renderSegmentsSettings(); // Re-render to update min/max bounds
        }));
        document.querySelectorAll('.seg-name').forEach(i => i.addEventListener('change', e => {
            segments[e.target.dataset.idx].name = e.target.value;
        }));
        document.querySelectorAll('.seg-start').forEach(i => i.addEventListener('change', e => {
            const v = parseInt(e.target.value);
            const max = parseInt(e.target.max);
            const min = parseInt(e.target.min);
            segments[e.target.dataset.idx].start = Math.max(min - 1, Math.min(v - 1, max - 1));
            renderSegmentsSettings();
        }));
        document.querySelectorAll('.seg-stop').forEach(i => i.addEventListener('change', e => {
            const v = parseInt(e.target.value);
            const max = parseInt(e.target.max);
            const min = parseInt(e.target.min);
            segments[e.target.dataset.idx].stop = Math.max(min, Math.min(v, max));
            renderSegmentsSettings();
        }));
        document.querySelectorAll('.btn-del-seg').forEach(btn => btn.addEventListener('click', e => {
            segments.splice(e.target.dataset.idx, 1);
            renderSegmentsSettings();
        }));
    }

    btnAddSegment.addEventListener('click', () => {
        let lastStop = segments.length > 0 ? segments[segments.length - 1].stop : 0;
        segments.push({name: 'Segment ' + segments.length, start: lastStop, stop: lastStop + 30, on: true, bri: 255, effect: 0, color: '#ff0000'});
        renderSegmentsSettings();
    });

    btnSaveSegments.addEventListener('click', () => {
        btnSaveSegments.innerText = 'Speichern...';
        fetch('/api/segments', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(segments)
        }).then(res => {
            if(res.ok) {
                const originalText = btnSaveSegments.innerText;
                btnSaveSegments.innerText = 'Gespeichert!';
                btnSaveSegments.style.backgroundColor = '#10b981';
                setTimeout(() => {
                    btnSaveSegments.innerText = originalText;
                    btnSaveSegments.style.backgroundColor = '';
                    settingsModal.classList.remove('show');
                    fetchState();
                }, 1000);
            }
        });
    });

    if (btnSaveButtons) {
        btnSaveButtons.addEventListener('click', () => {
            const payload = {
                btn1: { active: btn1Active.checked, type: btn1Type.value },
                btn2: { active: btn2Active.checked, type: btn2Type.value }
            };
            fetch('/api/buttons', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            }).then(res => {
                if(res.ok) {
                    const originalText = btnSaveButtons.innerText;
                    btnSaveButtons.innerText = 'Gespeichert! Neustart...';
                    btnSaveButtons.style.backgroundColor = '#10b981';
                    setTimeout(() => {
                        btnSaveButtons.innerText = originalText;
                        btnSaveButtons.style.backgroundColor = '';
                        settingsModal.classList.remove('show');
                        location.reload();
                    }, 2000);
                }
            });
        });
    }

    function renderSegmentsSelector() {
        if (!segmentSelector) return;
        segmentSelector.innerHTML = '';
        segments.forEach((seg, idx) => {
            const btn = document.createElement('button');
            btn.className = idx === currentSegmentId ? 'effect-btn active' : 'btn btn-primary';
            btn.style.width = '100%';
            btn.style.textAlign = 'left';
            btn.innerText = `${seg.name || ('Segment ' + idx)} (${seg.start + 1} - ${seg.stop})`;
            btn.addEventListener('click', () => {
                currentSegmentId = idx;
                updateStateFromSegment();
                renderSegmentsSelector();
            });
            segmentSelector.appendChild(btn);
        });
    }

    function updateStateFromSegment() {
        if (segments.length > 0 && currentSegmentId < segments.length) {
            let s = segments[currentSegmentId];
            state.on = s.on;
            state.bri = s.bri;
            state.effect = s.effect;
            state.speed = s.speed !== undefined ? s.speed : 128;
            
            if (btnSegPower) {
                if (s.on) btnSegPower.classList.add('on');
                else btnSegPower.classList.remove('on');
            }
            
            let c = s.color.toString(16);
            while (c.length < 6) c = "0" + c;
            state.color = "#" + c.substring(c.length - 6);
            state.white = (s.color >> 24) & 0xFF;
            
            updateUI();
        }
    }

    // --- Slaves UI ---
    const btnRefreshSlaves = document.getElementById('btnRefreshSlaves');
    const slavesList = document.getElementById('slavesList');

    if (btnRefreshSlaves) {
        btnRefreshSlaves.addEventListener('click', fetchSlaves);
    }

    function fetchSlaves() {
        if (!slavesList) return;
        slavesList.innerHTML = '<div style="text-align: center; padding: 20px; color: var(--text-muted);">Scanne Netzwerk...</div>';
        
        fetch('/api/slaves')
            .then(res => res.json())
            .then(data => {
                slavesList.innerHTML = '';
                if (data.length === 0) {
                    slavesList.innerHTML = '<div style="text-align: center; padding: 20px; color: var(--text-muted);">Keine Slaves gefunden. Überprüfe die UART-Verbindung.</div>';
                    return;
                }
                
                data.forEach(slave => {
                    const isConfigured = slave.id !== 254;
                    const card = document.createElement('div');
                    card.style.cssText = 'background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; border: 1px solid rgba(255,255,255,0.1);';
                    
                    const freePins = [0, 1, 2, 3, 4, 5, 6, 7, 15, 18, 19];
                    let pinOptions = '';
                    freePins.forEach(p => {
                        pinOptions += `<option value="${p}" ${p === 4 ? 'selected' : ''}>GPIO ${p}</option>`;
                    });
                    
                    const typeOptions = document.getElementById('ledType').innerHTML;
                    
                    card.innerHTML = `
                        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                            <div style="display: flex; align-items: center; gap: 10px;">
                                <h4 style="margin: 0; color: ${isConfigured ? 'var(--primary)' : '#fbbf24'};">${isConfigured ? 'Konfigurierter Slave (ID ' + slave.id + ')' : 'Neuer Slave gefunden!'}</h4>
                                <span style="font-size: 11px; padding: 2px 6px; border-radius: 4px; background: ${slave.isWireless ? 'rgba(59, 130, 246, 0.2)' : 'rgba(99, 102, 241, 0.2)'}; color: ${slave.isWireless ? '#60a5fa' : '#818cf8'}; border: 1px solid ${slave.isWireless ? 'rgba(59, 130, 246, 0.4)' : 'rgba(99, 102, 241, 0.4)'};">${slave.isWireless ? 'Wifi' : 'UART'}</span>
                            </div>
                            <span style="font-size: 12px; color: var(--text-muted);">Zuletzt gesehen: vor ${Math.round(slave.lastSeenAge / 1000)}s</span>
                        </div>
                        <div style="font-size: 12px; color: var(--text-muted); margin-bottom: 10px;">
                            Software Version: <strong style="color: white;">${slave.version || 'Unbekannt'}</strong>
                        </div>
                        <div class="form-group" style="margin-bottom: 10px;">
                            <label>Name</label>
                            <input type="text" id="slaveName_${slave.id}" value="${slave.name === 'Unknown' ? 'Slave ' + slave.id : slave.name}">
                        </div>
                        <div class="form-group" style="margin-bottom: 10px;">
                            <label>LED Typ</label>
                            <select id="slaveType_${slave.id}" onchange="document.getElementById('groupSlavePin2_${slave.id}').style.display = (this.value >= 50 && this.value <= 54) ? 'block' : 'none';">
                                ${typeOptions}
                            </select>
                        </div>
                        <div style="display: flex; gap: 10px; margin-bottom: 10px;">
                            <div class="form-group" style="flex: 1;">
                                <label>Data Pin</label>
                                <select id="slavePin_${slave.id}">
                                    ${pinOptions}
                                </select>
                            </div>
                            <div class="form-group" style="flex: 1; display: none;" id="groupSlavePin2_${slave.id}">
                                <label>Clock Pin</label>
                                <select id="slavePin2_${slave.id}">
                                    ${pinOptions.replace('selected', '')}
                                </select>
                            </div>
                            <div class="form-group" style="flex: 1;">
                                <label>Anzahl LEDs</label>
                                <input type="number" id="slaveLeds_${slave.id}" min="0" max="1000" value="${slave.ledCount}">
                            </div>
                        </div>
                        <button id="btnSaveSlave_${slave.id}" class="btn-primary" onclick="configureSlave(${slave.id})" style="width: 100%; padding: 8px;">Konfiguration an Slave senden</button>
                    `;
                    slavesList.appendChild(card);
                    
                    // Set correct type if it was already configured
                    // (Requires backend to send type, but for now defaults to current master type or user selects)
                    // We can just trigger onchange to update pin2 visibility
                    document.getElementById(`slaveType_${slave.id}`).dispatchEvent(new Event('change'));
                });
            })
            .catch(err => {
                slavesList.innerHTML = '<div style="text-align: center; padding: 20px; color: #ef4444;">Fehler beim Laden der Slaves.</div>';
            });
    }

    window.configureSlave = function(currentId) {
        const name = document.getElementById(`slaveName_${currentId}`).value;
        const pin = parseInt(document.getElementById(`slavePin_${currentId}`).value);
        let pin2 = 255;
        const pin2El = document.getElementById(`slavePin2_${currentId}`);
        if(pin2El) pin2 = parseInt(pin2El.value);
        const ledCount = parseInt(document.getElementById(`slaveLeds_${currentId}`).value);
        const typeEl = document.getElementById(`slaveType_${currentId}`);
        const type = typeEl ? parseInt(typeEl.value) : parseInt(document.getElementById('ledType').value);
        const newId = currentId === 254 ? Math.floor(Math.random() * 253) + 1 : currentId; // Assign random ID 1-253 if new
        
        fetch('/api/slaves/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ currentId, newId, pin, pin2, ledCount, type, name })
        }).then(res => {
            const btn = document.getElementById(`btnSaveSlave_${currentId}`);
            if (res.ok) {
                const originalText = btn.innerText;
                btn.innerText = 'Gesendet!';
                btn.style.backgroundColor = '#10b981';
                setTimeout(() => {
                    btn.innerText = originalText;
                    btn.style.backgroundColor = '';
                    settingsModal.classList.remove('show');
                    fetchSlaves();
                }, 1000);
            } else {
                const originalText = btn.innerText;
                btn.innerText = 'Fehler!';
                btn.style.backgroundColor = '#ef4444';
                setTimeout(() => {
                    btn.innerText = originalText;
                    btn.style.backgroundColor = '';
                }, 1000);
            }
        });
    };

    // Event Listeners - System / OTA
    const btnUpdateSlaves = document.getElementById('btnUpdateSlaves');
    if (btnUpdateSlaves) {
        btnUpdateSlaves.addEventListener('click', () => {
            if (confirm("Möchtest du allen Slaves das Kommando für ein WLAN-Update (GitHub) senden?")) {
                btnUpdateSlaves.innerText = "Suche Update...";
                btnUpdateSlaves.disabled = true;
                
                fetch('https://api.github.com/repos/KaelanTesseract/HyperLED-Slave/releases/latest')
                    .then(r => { if(r.ok) return r.json(); throw new Error('No release found'); })
                    .then(ghData => {
                        let downloadUrl = "";
                        if (ghData.assets && ghData.assets.length > 0) {
                            let asset = ghData.assets.find(a => a.name === "firmware.bin");
                            if (!asset) asset = ghData.assets[0];
                            downloadUrl = asset.browser_download_url;
                        }
                        
                        if (!downloadUrl) {
                            alert("Konnte keine firmware.bin im aktuellsten Release finden!");
                            btnUpdateSlaves.innerText = "Slaves jetzt aktualisieren";
                            btnUpdateSlaves.disabled = false;
                            return;
                        }
                        
                        btnUpdateSlaves.innerText = "Sende Kommando...";
                        fetch('/api/slaves/update', { 
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({ url: downloadUrl })
                        })
                        .then(res => {
                            if (res.ok) alert("Kommando gesendet! Die Slaves werden sich nun mit dem WLAN verbinden und das Update laden.");
                            else alert("Fehler beim Senden des Kommandos an den Master.");
                            btnUpdateSlaves.innerText = "Slaves jetzt aktualisieren";
                            btnUpdateSlaves.disabled = false;
                        })
                        .catch(() => {
                            alert("Verbindungsfehler.");
                            btnUpdateSlaves.innerText = "Slaves jetzt aktualisieren";
                            btnUpdateSlaves.disabled = false;
                        });
                    })
                    .catch(e => {
                        alert("Konnte neuestes Release auf GitHub nicht finden. Hast du ein Release erstellt?");
                        btnUpdateSlaves.innerText = "Slaves jetzt aktualisieren";
                        btnUpdateSlaves.disabled = false;
                    });
            }
        });
    }

    if (btnFactoryReset) {
        btnFactoryReset.addEventListener('click', () => {
            if (confirm("Wirklich auf Werkseinstellungen zurücksetzen? Alle Konfigurationen gehen verloren!")) {
                fetch('/api/factory_reset', { method: 'POST' }).then(() => {
                    alert("System wird zurückgesetzt und startet neu...");
                    setTimeout(() => location.reload(), 3000);
                });
            }
        });
    }

    function fetchVersion() {
        sysVersion.innerText = 'Lade...';
        fetch('/api/info')
            .then(res => res.json())
            .then(data => {
                const currentVer = data.version || '0.1.001';
                sysVersion.innerText = currentVer;
                const mainVer = document.getElementById('mainVersionDisplay');
                if (mainVer) mainVer.innerText = currentVer;
                
                // GitHub OTA Check
                const GITHUB_USER = 'KaelanTesseract';
                const GITHUB_REPO = 'HyperLED';
                
                fetch(`https://api.github.com/repos/${GITHUB_USER}/${GITHUB_REPO}/releases/latest`)
                    .then(r => { if(r.ok) return r.json(); throw new Error('No release found'); })
                    .then(ghData => {
                        let latestVer = ghData.tag_name;
                        let displayVer = latestVer;
                        if (displayVer.startsWith('v')) displayVer = displayVer.substring(1);
                        
                        if (displayVer !== currentVer) {
                            sysVersion.innerHTML = `<span style="color: #ef4444;">${currentVer}</span><br><span style="font-size: 14px; color: var(--text-muted);">Aktuellste: ${displayVer}</span>`;
                            btnCheckUpdate.innerText = 'Update installieren';
                            btnCheckUpdate.disabled = false;
                            btnCheckUpdate.onclick = () => startOnlineUpdate(latestVer);
                            
                            const banner = document.getElementById('updateBanner');
                            if (banner) {
                                banner.style.display = 'block';
                                document.getElementById('updateBannerTitle').innerText = 'Master Update verfügbar!';
                                document.getElementById('updateBannerText').innerText = `Version ${displayVer} ist bereit zur Installation.`;
                            }
                        } else {
                            sysVersion.innerHTML = `<span style="color: #10b981;">${currentVer}</span>`;
                            btnCheckUpdate.innerText = 'System ist aktuell';
                            btnCheckUpdate.disabled = true;
                        }
                    })
                    .catch(err => {
                        console.log('GitHub API error or no releases yet', err);
                        sysVersion.innerHTML = `<span style="color: #10b981;">${currentVer}</span>`;
                        btnCheckUpdate.innerText = 'System ist aktuell';
                        btnCheckUpdate.disabled = true;
                    });
            })
            .catch(() => { sysVersion.innerText = 'Fehler'; });
    }
    
    function checkSlaveUpdates() {
        fetch('/api/slaves')
            .then(res => res.json())
            .then(slaves => {
                const activeSlaves = slaves.filter(s => s.id !== 254);
                if (activeSlaves.length === 0) return;
                
                fetch('https://api.github.com/repos/KaelanTesseract/HyperLED-Slave/releases/latest')
                    .then(r => { if(r.ok) return r.json(); throw new Error('No release found'); })
                    .then(ghData => {
                        let latestVer = ghData.tag_name;
                        let displayVer = latestVer;
                        if (displayVer.startsWith('v')) displayVer = displayVer.substring(1);
                        
                        let needsUpdate = activeSlaves.some(s => s.version && s.version !== displayVer);
                        if (needsUpdate) {
                            const banner = document.getElementById('updateBanner');
                            if (banner) {
                                if (banner.style.display === 'block') {
                                    document.getElementById('updateBannerTitle').innerText = 'System Updates verfügbar!';
                                    document.getElementById('updateBannerText').innerText = 'Es gibt Updates für Master & Slave. Klicke hier.';
                                } else {
                                    banner.style.display = 'block';
                                    document.getElementById('updateBannerTitle').innerText = 'Slave Update verfügbar!';
                                    document.getElementById('updateBannerText').innerText = `Slave Version ${displayVer} ist bereit zur Installation.`;
                                }
                            }
                        }
                    })
                    .catch(e => console.log("Slave update check failed", e));
            })
            .catch(e => console.log("Failed to fetch slaves for update check"));
    }
    
    if (!isAPMode) {
        fetchVersion();
        checkSlaveUpdates();
    }
    
    function startOnlineUpdate(version) {
        btnCheckUpdate.disabled = true;
        btnCheckUpdate.innerText = 'Starte Update...';
        document.getElementById('onlineProgressContainer').style.display = 'block';
        
        fetch('/api/update_online', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ version: version })
        }).then(res => {
            if(res.ok) {
                pollUpdateProgress();
            } else {
                alert('Fehler beim Starten des Updates.');
                btnCheckUpdate.disabled = false;
                btnCheckUpdate.innerText = 'Update installieren';
                document.getElementById('onlineProgressContainer').style.display = 'none';
            }
        });
    }

    function pollUpdateProgress() {
        fetch('/api/update_progress')
            .then(res => res.json())
            .then(data => {
                const bar = document.getElementById('onlineProgressBar');
                bar.style.width = data.progress + '%';
                updateStatus.innerText = data.status || `Lade herunter... ${data.progress}%`;
                
                if (data.progress < 100 && data.status !== 'error') {
                    setTimeout(pollUpdateProgress, 1000);
                } else if (data.status === 'error') {
                    alert('Update fehlgeschlagen!');
                    btnCheckUpdate.disabled = false;
                    btnCheckUpdate.innerText = 'Update installieren';
                } else {
                    updateStatus.innerText = 'Update erfolgreich! Neustart...';
                    setTimeout(() => location.reload(), 5000);
                }
            })
            .catch(() => setTimeout(pollUpdateProgress, 1000));
    }

    btnUpdateLocal.addEventListener('click', () => {
        if (updateFile.files.length === 0) {
            alert('Bitte wähle zuerst eine .bin Datei aus!');
            return;
        }

        btnUpdateLocal.disabled = true;
        btnUpdateLocal.innerText = 'Wird hochgeladen...';
        const formData = new FormData();
        for (let i = 0; i < updateFile.files.length; i++) {
            const file = updateFile.files[i];
            formData.append('update[]', file, file.name);
        }

        const request = new XMLHttpRequest();
        request.open('POST', '/update');
        
        otaProgressContainer.style.display = 'block';

        request.upload.addEventListener('progress', (e) => {
            const percent = (e.loaded / e.total) * 100;
            otaProgressBar.style.width = percent + '%';
        });

        request.addEventListener('load', () => {
            if (request.status === 200) {
                btnUpdateLocal.innerText = 'Update erfolgreich! Neustart...';
                setTimeout(() => {
                    location.reload();
                }, 3000);
            } else {
                alert('Update fehlgeschlagen!');
                otaProgressContainer.style.display = 'none';
                otaProgressBar.style.width = '0%';
                btnUpdateLocal.disabled = false;
                btnUpdateLocal.innerText = 'Update starten';
            }
        });

        request.send(formData);
    });

    btnCheckUpdate.addEventListener('click', () => {
        btnCheckUpdate.disabled = true;
        updateStatus.innerText = "Prüfe auf Updates...";
        
        fetch('/api/internet_update', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                if (data.status === "started") {
                    updateStatus.innerText = "Update läuft... Das Gerät startet bei Erfolg automatisch neu.";
                } else if (data.status === "up_to_date") {
                    updateStatus.innerText = "Du hast bereits die neueste Version.";
                    btnCheckUpdate.disabled = false;
                } else {
                    updateStatus.innerText = "Fehler: " + data.message;
                    btnCheckUpdate.disabled = false;
                }
            })
            .catch(e => {
                updateStatus.innerText = "Verbindungsfehler.";
                btnCheckUpdate.disabled = false;
            });
    });

    // API Helpers
    async function fetchStatus() {
        try {
            const res = await fetch('/api/status');
            const data = await res.json();
            isAPMode = data.ap_mode;
        } catch (e) {
            console.error("Status fetch failed", e);
        }
    }

    function fetchState() {
        if (isInteracting) return;
        if (settingsModal.classList.contains('show')) return;
        
        fetch('/api/state')
            .then(res => res.json())
            .then(data => {
                if (isInteracting) return;
                if (data.seg && Array.isArray(data.seg)) {
                    segments = data.seg;
                    if (currentSegmentId >= segments.length) currentSegmentId = 0;
                    renderSegmentsSettings();
                    renderSegmentsSelector();
                    updateStateFromSegment();
                }
            });
    }

    async function fetchConfig() {
        try {
            const res = await fetch('/api/config');
            if (res.ok) {
                const data = await res.json();
                if(data.pins) {
                    for(let i=0; i<5; i++) {
                        if(data.pins[i] !== undefined) ledPins[i].value = data.pins[i];
                    }
                }
                ledCount.value = data.count;
                ledType.value = data.type;
                if(data.abl_en !== undefined) ablEnable.checked = data.abl_en;
                if(data.abl_ma !== undefined) ablMaxMa.value = data.abl_ma;
                
                if(data.matrix_en !== undefined) {
                    matrixEnable.checked = data.matrix_en;
                    matrixWidth.value = data.matrix_w;
                    matrixHeight.value = data.matrix_h;
                    matrixLayout.value = data.matrix_l;
                    matrixConfigGroup.style.display = data.matrix_en ? 'block' : 'none';
                }
                
                updatePinUI();
                updateAblUI();
            }
        } catch (e) {
            console.error("Config fetch error:", e);
        }
    }

    async function fetchMqtt() {
        try {
            const res = await fetch('/api/mqtt');
            if (res.ok) {
                const data = await res.json();
                mqttEnable.checked = data.enabled;
                mqttServer.value = data.server || "";
                mqttPort.value = data.port || 1883;
                mqttUser.value = data.user || "";
                mqttPass.value = data.pass || "";
                mqttTopic.value = data.topic || "hyperled/device";
            }
        } catch (e) {
            console.error("MQTT config fetch error:", e);
        }
    }
    
    // fetchZigbee removed
    
    function fetchWlanStatus() {
        fetch('/api/wifi/status')
            .then(res => {
                if(res.ok) return res.json();
                throw new Error('Not connected');
            })
            .then(data => {
                document.getElementById('wlanStatusContainer').style.display = 'block';
                document.getElementById('wlanCurrentSsid').innerText = data.ssid;
                document.getElementById('wlanCurrentIp').innerText = data.ip;
            })
            .catch(() => {
                document.getElementById('wlanStatusContainer').style.display = 'none';
            });
    }

    btnSettings.addEventListener('click', fetchWlanStatus);

    function sendState(updates) {
        if (isAPMode) return;
        
        // Apply local state updates immediately
        if (updates.on !== undefined) state.on = updates.on;
        if (updates.bri !== undefined) state.bri = updates.bri;
        if (updates.effect !== undefined) state.effect = updates.effect;
        if (updates.color !== undefined) state.color = updates.color;
        if (updates.white !== undefined) state.white = updates.white;
        if (updates.speed !== undefined) state.speed = updates.speed;
        
        let segUpdates = [];
        const syncCheckbox = document.getElementById('syncSegments');
        
        if (syncCheckbox && syncCheckbox.checked) {
            for (let i = 0; i < segments.length; i++) {
                segUpdates.push({ id: i, ...updates });
                // Update local segment representations
                if (segments[i]) {
                    if (updates.on !== undefined) segments[i].on = updates.on;
                    if (updates.bri !== undefined) segments[i].bri = updates.bri;
                    if (updates.effect !== undefined) segments[i].effect = updates.effect;
                    if (updates.speed !== undefined) segments[i].speed = updates.speed;
                }
            }
        } else {
            segUpdates.push({ id: currentSegmentId, ...updates });
            // Update local segment representation
            if (segments[currentSegmentId]) {
                if (updates.on !== undefined) segments[currentSegmentId].on = updates.on;
                if (updates.bri !== undefined) segments[currentSegmentId].bri = updates.bri;
                if (updates.effect !== undefined) segments[currentSegmentId].effect = updates.effect;
                if (updates.speed !== undefined) segments[currentSegmentId].speed = updates.speed;
            }
        }
        
        const payload = {
            sync: (syncCheckbox && syncCheckbox.checked) ? true : false,
            seg: segUpdates
        };
        
        fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
    }

    function updateUI() {
        const anyOn = segments.length > 0 ? segments.some(s => s.on) : state.on;
        if (anyOn) {
            btnPower.classList.add('on');
        } else {
            btnPower.classList.remove('on');
        }
        
        colorWheel.color.hexString = state.color;
        briSlider.value = state.bri;
        if (speedSlider) speedSlider.value = state.speed;
        
        effectBtns.forEach(btn => {
            if (parseInt(btn.dataset.id) === state.effect) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });
    }

    // Matrix & Pixel Art UI Logic
    matrixEnable.addEventListener('change', () => {
        matrixConfigGroup.style.display = matrixEnable.checked ? 'block' : 'none';
        saveMatrixConfig();
    });
    matrixWidth.addEventListener('change', saveMatrixConfig);
    matrixHeight.addEventListener('change', saveMatrixConfig);
    matrixLayout.addEventListener('change', saveMatrixConfig);

    function saveMatrixConfig() {
        fetch('/api/matrix_config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                isMatrix: matrixEnable.checked,
                width: parseInt(matrixWidth.value),
                height: parseInt(matrixHeight.value),
                layout: parseInt(matrixLayout.value)
            })
        });
    }


    btnUploadImage.addEventListener('click', () => {
        imageUpload.click();
    });

    imageUpload.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;

        const reader = new FileReader();
        reader.onload = (event) => {
            const img = new Image();
            img.onload = () => {
                const w = parseInt(matrixWidth.value) || 16;
                const h = parseInt(matrixHeight.value) || 16;
                
                pixelCanvas.width = w;
                pixelCanvas.height = h;
                const ctx = pixelCanvas.getContext('2d');
                
                ctx.fillStyle = '#000';
                ctx.fillRect(0, 0, w, h);
                ctx.drawImage(img, 0, 0, w, h);
                
                pixelCanvas.style.display = 'block';
                btnStreamMatrix.style.display = 'block';
                btnStreamMatrix.disabled = false;
            };
            img.src = event.target.result;
        };
        reader.readAsDataURL(file);
    });

    btnStreamMatrix.addEventListener('click', () => {
        btnStreamMatrix.disabled = true;
        btnStreamMatrix.innerText = 'Sende...';
        
        const w = pixelCanvas.width;
        const h = pixelCanvas.height;
        const ctx = pixelCanvas.getContext('2d');
        const imgData = ctx.getImageData(0, 0, w, h).data;
        
        let pixelArray = [];
        for (let i = 0; i < imgData.length; i += 4) {
            let r = imgData[i];
            let g = imgData[i+1];
            let b = imgData[i+2];
            let hex = (r << 16) | (g << 8) | b;
            pixelArray.push(hex);
        }
        
        fetch('/api/matrix', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(pixelArray)
        }).then(() => {
            btnStreamMatrix.innerText = 'Gesendet!';
            setTimeout(() => {
                btnStreamMatrix.innerText = 'Auf Matrix streamen';
                btnStreamMatrix.disabled = false;
            }, 2000);
        }).catch(() => {
            btnStreamMatrix.innerText = 'Fehler!';
            setTimeout(() => {
                btnStreamMatrix.innerText = 'Auf Matrix streamen';
                btnStreamMatrix.disabled = false;
            }, 2000);
        });
    });

});
