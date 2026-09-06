document.addEventListener('DOMContentLoaded', () => {
    window.hardwareLimits = { master: 0, slaves: [] };
    function translateSegmentName(name) {
        if (!name) return name;
        if (name === "Master") return typeof t === 'function' ? t('dyn_master') : name;
        if (name.startsWith("Slave ")) {
            return (typeof t === 'function' ? t('dyn_slave') : "Slave") + " " + name.substring(6);
        }
        return name;
    }
    
    // UI Elements
    const btnPower = document.getElementById('btnPower');
    const btnSettings = document.getElementById('btnSettings');
    const briSlider = document.getElementById('briSlider');
    const whiteSlider = document.getElementById('whiteSlider');
    const speedSlider = document.getElementById('speedSlider');
    const intensitySlider = document.getElementById('intensitySlider');
    const color2EnabledToggle = document.getElementById('color2Enabled');
    const color2WheelWrapper = document.getElementById('color2WheelWrapper');
    const effectsGrid = document.getElementById('effectsGrid');
    const paletteSelect = document.getElementById('paletteSelect');
    const textEffectControls = document.getElementById('textEffectControls');
    const btnOpenTextWidgets = document.getElementById('btnOpenTextWidgets');

    const EFFECT_NAMES = [
        "Solid", "Breathe", "Rainbow", "Chase", "Fire", "Color Wipe", "Scanner", "Twinkle", "Meteor", "Matrix Rain", "Nur Weiß",
        "Strobe", "Bounce", "Palette Rainbow",
        "Sinelon", "Confetti", "Juggle", "BPM", "Theater Chase Rainbow", "Running Lights", "Color Waves",
        "Plasma", "Ripple", "Fire 2D", "Pacifica", "Bild",
        "Fireworks", "Starfield", "Bouncing Balls", "Uhr / Text"
    ];
    const EFFECT_TEXT_ID = 29;
    // Effects at/after this index only make sense (and are only shown as
    // selectable) once the LED type is set to HUB75 - see EFFECT_HUB75_SHOWCASE_START
    // in LEDManager.h.
    const EFFECT_HUB75_SHOWCASE_START = 26;

    // Generate effect buttons
    effectsGrid.innerHTML = '';
    EFFECT_NAMES.forEach((name, idx) => {
        const btn = document.createElement('button');
        btn.className = 'effect-btn';
        btn.dataset.id = idx;
        const key = 'eff_' + idx;
        btn.dataset.i18n = key;
        const translated = typeof t === 'function' ? t(key) : key;
        btn.innerText = translated === key ? name : translated;
        effectsGrid.appendChild(btn);
    });

    const effectBtns = document.querySelectorAll('.effect-btn');

    // HUB75 showcase effects only make visual sense (and are only selectable) on
    // an actual HUB75 scan-matrix panel - hide those buttons for every other LED type.
    function updateEffectVisibility(ledTypeValue) {
        const isHub75 = parseInt(ledTypeValue) === 60;
        effectBtns.forEach(btn => {
            if (parseInt(btn.dataset.id) >= EFFECT_HUB75_SHOWCASE_START) {
                btn.style.display = isHub75 ? '' : 'none';
            }
        });
    }
    updateEffectVisibility(0); // hidden by default until fetchConfig() reports the real type

    // Palette 0 ("Solid") must match LEDManager's PALETTE_NAMES order.
    const PALETTE_NAMES = ["Solid", "Rainbow", "Fire", "Ocean", "Forest"];

    if (paletteSelect) {
        paletteSelect.innerHTML = '';
        PALETTE_NAMES.forEach((name, idx) => {
            const opt = document.createElement('option');
            opt.value = idx;
            opt.style.background = '#1f2937';
            opt.style.color = 'white';
            const key = 'pal_' + idx;
            const translated = typeof t === 'function' ? t(key) : key;
            opt.innerText = translated === key ? name : translated;
            paletteSelect.appendChild(opt);
        });
    }
    
    // Screens & Modals
    const mainControls = document.getElementById('mainControls');
    const apSetupScreen = document.getElementById('apSetupScreen');
    const settingsModal = document.getElementById('settingsModal');
    const closeSettings = document.getElementById('closeSettings');
    const apWifiContainer = document.getElementById('apWifiContainer');
    const modalWifiContainer = document.getElementById('modalWifiContainer');
    const mqttSettingsSection = document.getElementById('mqttSettingsSection');
    const apConnectStatus = document.getElementById('apConnectStatus');
    const apConnectMessage = document.getElementById('apConnectMessage');
    const apConnectResult = document.getElementById('apConnectResult');
    const apConnectIp = document.getElementById('apConnectIp');
    const btnApFinish = document.getElementById('btnApFinish');
    const imageUpload = document.getElementById('imageUpload');
    const btnUploadImage = document.getElementById('btnUploadImage');
    const pixelCanvas = document.getElementById('pixelCanvas');
    const btnStreamMatrix = document.getElementById('btnStreamMatrix');
    
    // Tabs
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');
    const settingsTabsEl = document.getElementById('settingsTabs');
    const tabsWrapperEl = settingsTabsEl ? settingsTabsEl.closest('.tabs-wrapper') : null;

    // Shows a fade hint on whichever side of the scrollable tab bar has more
    // tabs hidden, so users notice there's more to scroll to (e.g. WLAN/MQTT).
    function updateTabScrollShadow() {
        if (!settingsTabsEl || !tabsWrapperEl) return;
        const maxScroll = settingsTabsEl.scrollWidth - settingsTabsEl.clientWidth;
        tabsWrapperEl.classList.toggle('scroll-left', settingsTabsEl.scrollLeft > 4);
        tabsWrapperEl.classList.toggle('scroll-right', settingsTabsEl.scrollLeft < maxScroll - 4);
    }

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
    const canvasPanelsList = document.getElementById('canvasPanelsList');
    const btnAddCanvasPanel = document.getElementById('btnAddCanvasPanel');
    const btnSaveCanvasPanels = document.getElementById('btnSaveCanvasPanels');
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

    // Presets & Playlist Elements
    const presetsListContainer = document.getElementById('presetsListContainer');
    const newPresetName = document.getElementById('newPresetName');
    const btnSaveNewPreset = document.getElementById('btnSaveNewPreset');
    const playlistListContainer = document.getElementById('playlistListContainer');
    const playlistEnabled = document.getElementById('playlistEnabled');
    const btnAddPlaylistEntry = document.getElementById('btnAddPlaylistEntry');
    const btnSavePlaylist = document.getElementById('btnSavePlaylist');

    // Schedules Elements
    const schedulesListContainer = document.getElementById('schedulesListContainer');
    const btnAddSchedule = document.getElementById('btnAddSchedule');
    const btnSaveSchedules = document.getElementById('btnSaveSchedules');
    const scheduleTimezone = document.getElementById('scheduleTimezone');
    const scheduleTimeStatus = document.getElementById('scheduleTimeStatus');
    let currentPresets = {};

    // Onboard status LED Elements
    const statusLedOn = document.getElementById('statusLedOn');
    const statusLedColor = document.getElementById('statusLedColor');
    const statusLedBri = document.getElementById('statusLedBri');
    const statusLedOptions = document.getElementById('statusLedOptions');

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
        speed: 128,
        palette: 0,
        intensity: 128,
        color2: '#0000ff',
        color2Enabled: false
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
            // MQTT can only be set up meaningfully once the device is on the real network,
            // so it stays hidden during the initial AP-mode WLAN setup.
            if (mqttSettingsSection) mqttSettingsSection.style.display = 'none';
        } else {
            apSetupScreen.style.display = 'none';
            fetchState();
            setInterval(fetchState, 2000);
            fetchConfig();
            fetchMqtt();
            fetchStatusLed();
            fetchCanvasPanels();
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

    // Second color wheel (Farbe 2), same styling as the primary one, only shown
    // once the user enables it - keeps the app's own design instead of the
    // browser's native (and un-themeable) <input type="color"> picker.
    const color2Wheel = new iro.ColorPicker("#color2Wheel", {
        width: 180,
        color: state.color2,
        borderWidth: 2,
        borderColor: "rgba(255,255,255,0.2)",
        layout: [
            { component: iro.ui.Wheel }
        ]
    });

    color2Wheel.on('input:start', setInteracting);

    color2Wheel.on('color:change', (color) => {
        state.color2 = color.hexString;
        if (typeof throttledSendState !== 'undefined') {
            throttledSendState({ color2: state.color2 });
        }
    });

    color2Wheel.on('input:end', (color) => {
        isInteracting = false;
        state.color2 = color.hexString;
        sendState({ color2: state.color2 });
    });

    if (color2EnabledToggle) {
        color2EnabledToggle.addEventListener('change', (e) => {
            state.color2Enabled = e.target.checked;
            if (color2WheelWrapper) color2WheelWrapper.style.display = e.target.checked ? 'flex' : 'none';
            sendState({ color2Enabled: state.color2Enabled });
        });
    }

    briSlider.addEventListener('mousedown', setInteracting);
    briSlider.addEventListener('touchstart', setInteracting, {passive: true});
    
    whiteSlider.addEventListener('mousedown', setInteracting);
    whiteSlider.addEventListener('touchstart', setInteracting, {passive: true});
    
    speedSlider.addEventListener('mousedown', setInteracting);
    speedSlider.addEventListener('touchstart', setInteracting, {passive: true});

    window.addEventListener('mouseup', clearInteracting);
    window.addEventListener('touchend', clearInteracting, {passive: true});

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
    
    function hasDualWhiteChannel(type) {
        return type == 34 || type == 35 || type == 42 || type == 45; // WS2805, SM16825, PWM CCT, PWM RGB+CCT
    }

        function updatePinUI() {
        const type = parseInt(ledType.value);
        
        // Toggle white slider
        const whiteSliderWrapper = document.getElementById('whiteSliderWrapper');
        if (whiteSliderWrapper) {
            whiteSliderWrapper.style.display = hasWhiteChannel(type) ? 'block' : 'none';
        }
        const nurWeissBtn = document.querySelector('.effect-btn[data-id="10"]');
        if (nurWeissBtn) {
            nurWeissBtn.style.display = hasWhiteChannel(type) ? 'inline-block' : 'none';
        }
        
                const groupingGroup = document.getElementById('groupingGroup');
        if (groupingGroup) {
            // Grouping makes sense for digital strips (types < 40 or >= 50), not for PWM/Analog
            const isDigital = (type < 40 || type >= 50);
            groupingGroup.style.display = isDigital ? 'block' : 'none';
        }

        const whiteOnlyToggleWrapper = document.getElementById('whiteOnlyToggleWrapper');
        if (whiteOnlyToggleWrapper) {
            whiteOnlyToggleWrapper.style.display = hasDualWhiteChannel(type) ? 'flex' : 'none';
        }
        let numPins = 1;
        let labels = [t('lbl_data_pin')];

        // Toggle backup pin hint
        const hintEl = document.getElementById('backupPinHint');
        if (hintEl) {
            // Types that often feature a Backup Data line: 22(WS281x), 29(TM1914), 34(WS2805)
            hintEl.style.display = (type == 22 || type == 29 || type == 34) ? 'block' : 'none';
        }

        // Logic for digital vs PWM
        if (type >= 50 && type <= 54) { // 2-Wire SPI LEDs
            numPins = 2;
            labels = [t('lbl_data_pin'), t('lbl_clk_pin')];
        } 
        else if (type == 40) { numPins = 1; labels = [t('lbl_on_off')]; }
        else if (type == 41) { numPins = 1; labels = [t('lbl_white_pin')]; }
        else if (type == 42) { numPins = 2; labels = [t('lbl_warm'), t('lbl_kalt')]; }
        else if (type == 43) { numPins = 3; labels = [t('lbl_rot'), t('lbl_gruen'), t('lbl_blau')]; }
        else if (type == 44) { numPins = 4; labels = [t('lbl_rot'), t('lbl_gruen'), t('lbl_blau'), t('lbl_weiss')]; }
        else if (type == 45) { numPins = 5; labels = [t('lbl_rot'), t('lbl_gruen'), t('lbl_blau'), t('lbl_warm'), t('lbl_kalt')]; }

        // HUB75 has its own fixed 14-pin wiring (shown as a read-only pinout below)
        // instead of the normal 1-5 pin dropdowns.
        const isHub75 = (type === 60);
        const hub75Group = document.getElementById('hub75Group');
        const pinContainer = document.getElementById('pinContainer');
        if (hub75Group) hub75Group.style.display = isHub75 ? 'block' : 'none';
        if (pinContainer) pinContainer.style.display = isHub75 ? 'none' : 'block';
        if (isHub75) {
            renderHub75Pinout();
            numPins = 0;
        }
        if (typeof updateEffectVisibility === 'function') updateEffectVisibility(type);

        for(let i=0; i<5; i++) {
            if(i < numPins) {
                groupPins[i].style.display = 'block';
                document.getElementById('labelPin' + i).innerText = labels[i] || ("Pin " + i);
            } else {
                groupPins[i].style.display = 'none';
            }
        }
    }

    // Mirrors the fixed HUB75_PIN_* constants in Config.h - these pins are not
    // user-configurable (HUB75 needs 14 GPIOs at once), so this is purely a
    // read-only wiring reference shown in the UI.
    const HUB75_PINS = [
        ['R1', 1], ['G1', 2], ['B1', 4],
        ['R2', 5], ['G2', 6], ['B2', 7],
        ['A', 8], ['B', 9], ['C', 10], ['D', 11], ['E', 12],
        ['CLK', 13], ['LAT', 14], ['OE', 15]
    ];

    function renderHub75Pinout(tableId) {
        const table = document.getElementById(tableId || 'hub75PinoutTable');
        if (!table) return;
        table.innerHTML = '';
        HUB75_PINS.forEach(([signal, gpio]) => {
            const sig = document.createElement('div');
            sig.style.color = 'var(--text-muted)';
            sig.innerText = signal;
            const pin = document.createElement('div');
            pin.style.color = 'white';
            pin.innerText = 'GPIO ' + gpio;
            table.appendChild(sig);
            table.appendChild(pin);
        });
    }

    ledType.addEventListener('change', updatePinUI);

    function updateAblUI() {
        let count = parseInt(ledCount.value) || 30;
        let additionalEspPower = 0;
        
        if (segments && segments.length > 0) {
            segments.forEach(seg => {
                if (seg.isSlave && seg.sharesPower) {
                    count += (seg.stop - seg.start);
                    additionalEspPower += 0.1; // 100mA for Slave ESP32
                }
            });
        }
        
        const totalA = (((count * 55) / 1000) + additionalEspPower).toFixed(1);
        ablRecommendedText.innerHTML = `${t('abl_total_leds_slaves')}: ${count}<br>${t('abl_recommended_psu')}: <strong style="color: #fff;">~${totalA} A</strong>`;
    }
    
    ledCount.addEventListener('input', updateAblUI);
    ledCount.addEventListener('change', updateAblUI);

    // Modal Logic
    btnSettings.addEventListener('click', () => {
        settingsModal.classList.add('show');
        setTimeout(updateTabScrollShadow, 50);
    });

    if (settingsTabsEl) {
        settingsTabsEl.addEventListener('scroll', updateTabScrollShadow);
        window.addEventListener('resize', updateTabScrollShadow);

        // Lets a normal (vertical) mouse wheel scroll the tab bar horizontally,
        // since desktop users have no touch/trackpad gesture for this otherwise.
        settingsTabsEl.addEventListener('wheel', (e) => {
            if (Math.abs(e.deltaY) <= Math.abs(e.deltaX)) return;
            e.preventDefault();
            settingsTabsEl.scrollLeft += e.deltaY;
        }, { passive: false });
    }

    const tabsScrollLeftBtn = document.getElementById('tabsScrollLeft');
    const tabsScrollRightBtn = document.getElementById('tabsScrollRight');
    if (tabsScrollLeftBtn && settingsTabsEl) {
        tabsScrollLeftBtn.addEventListener('click', () => {
            settingsTabsEl.scrollBy({ left: -140, behavior: 'smooth' });
        });
    }
    if (tabsScrollRightBtn && settingsTabsEl) {
        tabsScrollRightBtn.addEventListener('click', () => {
            settingsTabsEl.scrollBy({ left: 140, behavior: 'smooth' });
        });
    }

    closeSettings.addEventListener('click', () => {
        settingsModal.classList.remove('show');
        stopMatrixPreview();
    });

    window.addEventListener('click', (e) => {
        if (e.target === settingsModal) {
            settingsModal.classList.remove('show');
            stopMatrixPreview();
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

            if (targetId !== 'tab-matrix') stopMatrixPreview();

            if (targetId === 'tab-slaves') {
                fetchSlaves();
            }
            if (targetId === 'tab-segments') {
                fetchHardwareLimitsForSegments(() => renderSegmentsSettings());
            }
            if (targetId === 'tab-matrix') {
                fetchHardwareLimitsForSegments(() => renderCanvasPanelsList());
                initEditorCanvas();
                startMatrixPreview();
                fetchWeatherStatus();
                fetch('/api/segments').then(res => res.json()).then(data => {
                    if (Array.isArray(data)) segments = data;
                    renderTextWidgetEditor();
                }).catch(() => {});
            }
            if (targetId === 'modalWifiContainer') {
                fetchWlanStatus();
            }
            if (targetId === 'tab-presets') {
                fetchPresets();
                fetchPlaylist();
            }
            if (targetId === 'tab-schedules') {
                fetchSchedules();
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
        if (state.effect === 10) {
            state.cct = parseInt(e.target.value);
            throttledSendState({ cct: state.cct });
        } else {
            state.white = parseInt(e.target.value);
            throttledSendState({ white: state.white });
        }
    });
    
    whiteSlider.addEventListener('change', (e) => {
        if (state.effect === 10) {
            state.cct = parseInt(e.target.value);
            sendState({ cct: state.cct });
        } else {
            state.white = parseInt(e.target.value);
            sendState({ white: state.white });
        }
    });
    
    const whiteOnlyToggle = document.getElementById('whiteOnlyToggle');
    if (whiteOnlyToggle) {
        whiteOnlyToggle.addEventListener('change', (e) => {
            sendState({ whiteOnly: e.target.checked });
        });
    }

    if (paletteSelect) {
        paletteSelect.addEventListener('change', (e) => {
            state.palette = parseInt(e.target.value);
            sendState({ palette: state.palette });
        });
    }

    if (btnOpenTextWidgets) {
        btnOpenTextWidgets.addEventListener('click', () => {
            settingsModal.classList.add('show');
            document.querySelector('.tab-btn[data-tab="tab-matrix"]').click();
        });
    }

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

    if (intensitySlider) {
        intensitySlider.addEventListener('mousedown', setInteracting);
        intensitySlider.addEventListener('touchstart', setInteracting, {passive: true});
        intensitySlider.addEventListener('mouseup', clearInteracting);
        intensitySlider.addEventListener('touchend', clearInteracting, {passive: true});

        intensitySlider.addEventListener('input', (e) => {
            state.intensity = parseInt(e.target.value);
            throttledSendState({ intensity: state.intensity });
        });

        intensitySlider.addEventListener('change', (e) => {
            state.intensity = parseInt(e.target.value);
            sendState({ intensity: state.intensity });
        });
    }

    // Event Listeners - Config
    btnSaveConfig.addEventListener('click', () => {
        const hub75Sel = document.getElementById('hub75ShiftDriver');
        const payload = {
            pins: ledPins.map(el => parseInt(el.value)),
            count: parseInt(ledCount.value),
            type: parseInt(ledType.value),
            ledsPerIC: parseInt(document.getElementById("ledsPerIC").value),
            abl_en: ablEnable.checked,
            abl_ma: parseInt(ablMaxMa.value),
            hub75_shift_driver: hub75Sel ? parseInt(hub75Sel.value) : 0
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

    // In AP mode the device tests the credentials while the AP stays up, so we can poll for
    // the result and show the new IP before anything reboots. Network errors during polling
    // are expected and ignored: the SoftAP briefly follows the router's channel while the
    // station connects, which can drop a request or two.
    function pollWifiSetupStatus(deadline) {
        fetch('/api/wifi/setup_status')
            .then(res => res.json())
            .then(data => {
                if (data.state === 'success') {
                    apConnectStatus.style.background = 'rgba(16, 185, 129, 0.1)';
                    apConnectStatus.style.border = '1px solid rgba(16, 185, 129, 0.3)';
                    apConnectMessage.style.color = '#10b981';
                    apConnectMessage.innerText = t('ap_connect_success') + (data.ssid ? ' (' + data.ssid + ')' : '');
                    apConnectIp.innerText = 'http://' + data.ip;
                    apConnectResult.style.display = 'block';
                    btnSaveWifi.innerText = t('btn_save_wlan');
                    btnSaveWifi.disabled = false;
                    return;
                }
                if (data.state === 'failed') {
                    apConnectStatus.style.background = 'rgba(239, 68, 68, 0.1)';
                    apConnectStatus.style.border = '1px solid rgba(239, 68, 68, 0.5)';
                    apConnectMessage.style.color = '#ef4444';
                    apConnectMessage.innerText = t('ap_connect_failed');
                    apConnectResult.style.display = 'none';
                    btnSaveWifi.innerText = t('btn_save_wlan');
                    btnSaveWifi.disabled = false;
                    return;
                }
                if (Date.now() < deadline) setTimeout(() => pollWifiSetupStatus(deadline), 1000);
            })
            .catch(() => {
                if (Date.now() < deadline) setTimeout(() => pollWifiSetupStatus(deadline), 1000);
            });
    }

    btnSaveWifi.addEventListener('click', () => {
        const formData = new URLSearchParams();
        formData.append('ssid', wifiSsid.value);
        formData.append('password', wifiPass.value);

        btnSaveWifi.innerText = t('btn_saving');

        if (isAPMode) {
            btnSaveWifi.disabled = true;
            apConnectStatus.style.display = 'block';
            apConnectStatus.style.background = 'rgba(255,255,255,0.05)';
            apConnectStatus.style.border = '1px solid rgba(255,255,255,0.1)';
            apConnectMessage.style.color = 'var(--text-muted)';
            apConnectMessage.innerText = t('ap_connect_testing');
            apConnectResult.style.display = 'none';
        }

        fetch('/api/save_wifi', {
            method: 'POST',
            body: formData
        }).then(res => {
            if (!res.ok) return;
            if (isAPMode) {
                // Give the device up to ~40s: the attempt itself runs for ~10s, plus retries
                // while the AP channel settles.
                pollWifiSetupStatus(Date.now() + 40000);
                return;
            }
            const originalText = btnSaveWifi.innerText;
            btnSaveWifi.innerText = 'Gespeichert!';
            btnSaveWifi.style.backgroundColor = '#10b981';
            setTimeout(() => {
                btnSaveWifi.innerText = originalText;
                btnSaveWifi.style.backgroundColor = '';
                settingsModal.classList.remove('show');
            }, 1500);
        });
    });

    if (btnApFinish) {
        btnApFinish.addEventListener('click', () => {
            btnApFinish.disabled = true;
            btnApFinish.innerText = t('ap_connect_restarting');
            fetch('/api/wifi/setup_finish', { method: 'POST' }).catch(() => {});
            apConnectMessage.innerText = t('ap_connect_restarting');
        });
    }

    // Event Listeners - MQTT
    btnSaveMqtt.addEventListener('click', () => {
        const payload = new URLSearchParams();
        payload.append('enabled', mqttEnable.checked ? 'true' : 'false');
        payload.append('server', mqttServer.value);
        payload.append('port', mqttPort.value);
        payload.append('user', mqttUser.value);
        payload.append('pass', mqttPass.value);
        payload.append('topic', mqttTopic.value);
        
        btnSaveMqtt.innerText = t('btn_saving');
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
            
            let targetOptions = `<option style="background: #1f2937; color: white;" value="master" ${currentTarget === 'master' ? 'selected' : ''}>${t('dyn_master')}</option>`;
            if (window.hardwareLimits && window.hardwareLimits.slaves) {
                window.hardwareLimits.slaves.forEach(s => {
                    const val = "slave_" + s.id;
                    const displayName = s.name !== 'Unknown' && s.name ? s.name : t('dyn_slave') + ' ' + s.id;
                    targetOptions += `<option style="background: #1f2937; color: white;" value="${val}" ${currentTarget === val ? 'selected' : ''}>${displayName}</option>`;
                });
            }
            
            div.innerHTML = `
                <div style="flex: 1; min-width: 120px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">${t('seg_target')}</label>
                    <select class="seg-target" data-idx="${idx}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                        ${targetOptions}
                    </select>
                </div>
                <div style="flex: 2; min-width: 120px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">${t('seg_name')}</label>
                    <input type="text" class="seg-name" data-idx="${idx}" value="${translateSegmentName(seg.name) || (t('dyn_segment') + ' ' + idx)}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                </div>
                <div style="flex: 1; min-width: 80px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">${t('seg_start')} (${minStart}-${maxStop})</label>
                    <input type="number" class="seg-start" data-idx="${idx}" min="${minStart}" max="${maxStop}" value="${seg.start + 1}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                </div>
                <div style="flex: 1; min-width: 80px;">
                    <label style="font-size: 12px; margin-bottom: 2px;">${t('seg_stop')} (${minStart}-${maxStop})</label>
                    <input type="number" class="seg-stop" data-idx="${idx}" min="${minStart}" max="${maxStop}" value="${seg.stop}" style="padding: 8px; width: 100%; min-width: 0; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;">
                </div>
                <button class="icon-btn btn-del-seg" data-idx="${idx}" style="color: #ff4757; background: rgba(255,71,87,0.1); margin-top: 15px; width: 34px; height: 34px; flex-shrink: 0;">
                    <svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round" style="pointer-events: none;">
                        <polyline points="3 6 5 6 21 6"></polyline>
                        <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
                    </svg>
                </button>
                ` + (seg.isSlave ? `
                <div style="flex: 100%; margin-top: 10px; margin-bottom: 5px;">
                    <label style="display: flex; align-items: center; gap: 8px; font-size: 13px; color: var(--text-muted); cursor: pointer;">
                        <input type="checkbox" class="seg-shares-power" data-idx="${idx}" ${seg.sharesPower ? 'checked' : ''} style="transform: scale(1.2);">
                        ${t('seg_shares')}
                    </label>
                </div>
                ` : '') + `
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
        document.querySelectorAll('.seg-shares-power').forEach(i => i.addEventListener('change', e => {
            segments[e.target.dataset.idx].sharesPower = e.target.checked;
            updateAblUI();
        }));
        document.querySelectorAll('.btn-del-seg').forEach(btn => btn.addEventListener('click', e => {
            segments.splice(e.target.dataset.idx, 1);
            renderSegmentsSettings();
        }));
    }

    btnAddSegment.addEventListener('click', () => {
        let lastStop = segments.length > 0 ? segments[segments.length - 1].stop : 0;
        segments.push({name: t('dyn_segment') + ' ' + segments.length, start: lastStop, stop: lastStop + 30, on: true, bri: 255, effect: 0, color: '#ff0000'});
        renderSegmentsSettings();
    });

    btnSaveSegments.addEventListener('click', () => {
        btnSaveSegments.innerText = t('btn_saving');
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

    // --- Presets ---
    function fetchPresets() {
        fetch('/presets.json')
            .then(res => res.json())
            .then(data => {
                currentPresets = data || {};
                renderPresetsList();
                renderPlaylistEntryOptions();
            })
            .catch(() => { currentPresets = {}; renderPresetsList(); });
    }

    function renderPresetsList() {
        presetsListContainer.innerHTML = '';
        const ids = Object.keys(currentPresets).sort((a, b) => parseInt(a) - parseInt(b));

        if (ids.length === 0) {
            const empty = document.createElement('div');
            empty.style.cssText = 'color: var(--text-muted); font-size: 13px; padding: 10px 0;';
            empty.setAttribute('data-i18n', 'preset_none');
            empty.innerText = t('preset_none') || 'Noch keine Presets gespeichert.';
            presetsListContainer.appendChild(empty);
            return;
        }

        ids.forEach(id => {
            const preset = currentPresets[id];
            const row = document.createElement('div');
            row.style.cssText = 'display: flex; align-items: center; gap: 8px; background: rgba(255,255,255,0.05); border-radius: 8px; padding: 8px 10px;';

            const nameSpan = document.createElement('span');
            nameSpan.style.cssText = 'flex: 1; font-size: 14px;';
            nameSpan.innerText = preset.name || ('Preset ' + id);
            row.appendChild(nameSpan);

            const btnApply = document.createElement('button');
            btnApply.className = 'btn btn-secondary';
            btnApply.style.cssText = 'padding: 5px 10px; font-size: 12px;';
            btnApply.innerText = t('preset_btn_apply') || 'Anwenden';
            btnApply.addEventListener('click', () => {
                fetch('/api/presets/apply', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ id: parseInt(id) })
                }).then(() => fetchState());
            });
            row.appendChild(btnApply);

            const btnOverwrite = document.createElement('button');
            btnOverwrite.className = 'btn btn-secondary';
            btnOverwrite.style.cssText = 'padding: 5px 10px; font-size: 12px;';
            btnOverwrite.innerText = t('preset_btn_overwrite') || 'Überschreiben';
            btnOverwrite.addEventListener('click', () => {
                fetch('/api/presets/save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ id: parseInt(id), name: preset.name })
                }).then(() => fetchPresets());
            });
            row.appendChild(btnOverwrite);

            const btnDelete = document.createElement('button');
            btnDelete.className = 'icon-btn';
            btnDelete.style.cssText = 'color: #ff4757; background: rgba(255,71,87,0.1); width: 30px; height: 30px; flex-shrink: 0;';
            btnDelete.innerHTML = '<svg viewBox="0 0 24 24" width="14" height="14" stroke="currentColor" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round" style="pointer-events: none;"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>';
            btnDelete.addEventListener('click', () => {
                fetch('/api/presets/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ id: parseInt(id) })
                }).then(() => { fetchPresets(); fetchPlaylist(); });
            });
            row.appendChild(btnDelete);

            presetsListContainer.appendChild(row);
        });
    }

    if (btnSaveNewPreset) {
        btnSaveNewPreset.addEventListener('click', () => {
            const name = newPresetName.value.trim() || 'Preset';
            fetch('/api/presets/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name: name })
            }).then(res => res.json()).then(() => {
                newPresetName.value = '';
                fetchPresets();
            });
        });
    }

    // --- Playlist ---
    let currentPlaylist = { enabled: false, entries: [] };

    function fetchPlaylist() {
        fetch('/api/playlist')
            .then(res => res.json())
            .then(data => {
                currentPlaylist = { enabled: data.enabled || false, entries: data.entries || [] };
                playlistEnabled.checked = currentPlaylist.enabled;
                renderPlaylistList();
            })
            .catch(() => {});
    }

    function renderPlaylistEntryOptions() {
        // Re-render so preset <select> options reflect the current preset list
        renderPlaylistList();
    }

    function renderPlaylistList() {
        playlistListContainer.innerHTML = '';
        const presetIds = Object.keys(currentPresets).sort((a, b) => parseInt(a) - parseInt(b));

        currentPlaylist.entries.forEach((entry, idx) => {
            const row = document.createElement('div');
            row.style.cssText = 'display: flex; align-items: center; gap: 8px;';

            const select = document.createElement('select');
            select.style.cssText = 'flex: 1; padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;';
            presetIds.forEach(id => {
                const opt = document.createElement('option');
                opt.style.cssText = 'background: #1f2937; color: white;';
                opt.value = id;
                opt.innerText = currentPresets[id].name || ('Preset ' + id);
                if (parseInt(id) === entry.id) opt.selected = true;
                select.appendChild(opt);
            });
            select.addEventListener('change', (e) => { entry.id = parseInt(e.target.value); });
            row.appendChild(select);

            const durationInput = document.createElement('input');
            durationInput.type = 'number';
            durationInput.min = '1';
            durationInput.value = entry.duration;
            durationInput.style.cssText = 'width: 70px; padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;';
            durationInput.addEventListener('change', (e) => { entry.duration = parseInt(e.target.value) || 10; });
            row.appendChild(durationInput);

            const secLabel = document.createElement('span');
            secLabel.style.cssText = 'font-size: 12px; color: var(--text-muted);';
            secLabel.innerText = 's';
            row.appendChild(secLabel);

            const btnRemove = document.createElement('button');
            btnRemove.className = 'icon-btn';
            btnRemove.style.cssText = 'color: #ff4757; background: rgba(255,71,87,0.1); width: 30px; height: 30px; flex-shrink: 0;';
            btnRemove.innerHTML = '<svg viewBox="0 0 24 24" width="14" height="14" stroke="currentColor" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round" style="pointer-events: none;"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>';
            btnRemove.addEventListener('click', () => {
                currentPlaylist.entries.splice(idx, 1);
                renderPlaylistList();
            });
            row.appendChild(btnRemove);

            playlistListContainer.appendChild(row);
        });
    }

    if (btnAddPlaylistEntry) {
        btnAddPlaylistEntry.addEventListener('click', () => {
            const presetIds = Object.keys(currentPresets);
            if (presetIds.length === 0) {
                alert(t('preset_none') || 'Noch keine Presets gespeichert.');
                return;
            }
            currentPlaylist.entries.push({ id: parseInt(presetIds[0]), duration: 10 });
            renderPlaylistList();
        });
    }

    if (btnSavePlaylist) {
        btnSavePlaylist.addEventListener('click', () => {
            currentPlaylist.enabled = playlistEnabled.checked;
            fetch('/api/playlist', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentPlaylist)
            }).then(res => {
                if (res.ok) {
                    const originalText = btnSavePlaylist.innerText;
                    btnSavePlaylist.innerText = 'Gespeichert!';
                    btnSavePlaylist.style.backgroundColor = '#10b981';
                    setTimeout(() => {
                        btnSavePlaylist.innerText = originalText;
                        btnSavePlaylist.style.backgroundColor = '';
                    }, 1000);
                }
            });
        });
    }

    // --- Schedules (NTP-based time schedules) ---
    // Day buttons are displayed Monday-first, but the bitmask always follows
    // struct tm's tm_wday (0 = Sunday .. 6 = Saturday) to match the firmware.
    const SCHEDULE_DAY_ORDER = [1, 2, 3, 4, 5, 6, 0];
    let currentSchedules = { timezone: 'CET-1CEST,M3.5.0,M10.5.0/3', entries: [] };

    function fetchSchedules() {
        fetch('/api/schedules')
            .then(res => res.json())
            .then(data => {
                currentSchedules = { timezone: data.timezone || currentSchedules.timezone, entries: data.entries || [] };
                if (scheduleTimezone) scheduleTimezone.value = currentSchedules.timezone;
                renderSchedulesList();
            })
            .catch(() => {});
        fetchScheduleTime();
    }

    function fetchScheduleTime() {
        if (!scheduleTimeStatus) return;
        fetch('/api/time')
            .then(res => res.json())
            .then(data => {
                if (data.synced) {
                    scheduleTimeStatus.innerText = (t('schedule_device_time') || 'Geräte-Uhrzeit: ') + data.localTime;
                } else {
                    scheduleTimeStatus.innerText = t('schedule_not_synced') || 'Noch nicht mit einem Zeitserver synchronisiert (nur im WLAN-Betrieb möglich).';
                }
            })
            .catch(() => {});
    }

    function renderSchedulesList() {
        if (!schedulesListContainer) return;
        schedulesListContainer.innerHTML = '';
        const presetIds = Object.keys(currentPresets).sort((a, b) => parseInt(a) - parseInt(b));

        currentSchedules.entries.forEach((entry, idx) => {
            const row = document.createElement('div');
            row.style.cssText = 'display: flex; align-items: center; gap: 8px; background: rgba(255,255,255,0.05); border-radius: 8px; padding: 8px 10px; flex-wrap: wrap;';

            const enabledCb = document.createElement('input');
            enabledCb.type = 'checkbox';
            enabledCb.checked = entry.enabled !== false;
            enabledCb.style.cssText = 'transform: scale(1.2); flex-shrink: 0;';
            enabledCb.addEventListener('change', (e) => { entry.enabled = e.target.checked; });
            row.appendChild(enabledCb);

            const timeInput = document.createElement('input');
            timeInput.type = 'time';
            timeInput.value = String(entry.hour).padStart(2, '0') + ':' + String(entry.minute).padStart(2, '0');
            timeInput.style.cssText = 'padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;';
            timeInput.addEventListener('change', (e) => {
                const parts = e.target.value.split(':');
                entry.hour = parseInt(parts[0]) || 0;
                entry.minute = parseInt(parts[1]) || 0;
            });
            row.appendChild(timeInput);

            const daysWrapper = document.createElement('div');
            daysWrapper.style.cssText = 'display: flex; gap: 3px;';
            SCHEDULE_DAY_ORDER.forEach(wday => {
                const dayBit = 1 << wday;
                const dayBtn = document.createElement('button');
                dayBtn.type = 'button';
                dayBtn.innerText = (t('day_' + wday) || ['So', 'Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa'][wday]);
                const active = (entry.days & dayBit) !== 0;
                dayBtn.style.cssText = 'padding: 5px 7px; font-size: 11px; border-radius: 5px; border: 1px solid var(--border-color); cursor: pointer; background: ' + (active ? 'var(--primary)' : 'rgba(0,0,0,0.2)') + '; color: white;';
                dayBtn.addEventListener('click', () => {
                    entry.days = entry.days ^ dayBit;
                    renderSchedulesList();
                });
                daysWrapper.appendChild(dayBtn);
            });
            row.appendChild(daysWrapper);

            const actionSelect = document.createElement('select');
            actionSelect.style.cssText = 'padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;';
            [[0, 'schedule_action_on', 'An'], [1, 'schedule_action_off', 'Aus'], [2, 'schedule_action_preset', 'Preset']].forEach(([val, key, fallback]) => {
                const opt = document.createElement('option');
                opt.style.cssText = 'background: #1f2937; color: white;';
                opt.value = val;
                opt.innerText = t(key) || fallback;
                if (entry.action === val) opt.selected = true;
                actionSelect.appendChild(opt);
            });
            actionSelect.addEventListener('change', (e) => {
                entry.action = parseInt(e.target.value);
                renderSchedulesList();
            });
            row.appendChild(actionSelect);

            if (entry.action === 2) {
                const presetSelect = document.createElement('select');
                presetSelect.style.cssText = 'flex: 1; min-width: 100px; padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none;';
                if (presetIds.length === 0) {
                    const opt = document.createElement('option');
                    opt.innerText = t('preset_none') || 'Noch keine Presets gespeichert.';
                    presetSelect.appendChild(opt);
                } else {
                    presetIds.forEach(id => {
                        const opt = document.createElement('option');
                        opt.style.cssText = 'background: #1f2937; color: white;';
                        opt.value = id;
                        opt.innerText = currentPresets[id].name || ('Preset ' + id);
                        if (parseInt(id) === entry.presetId) opt.selected = true;
                        presetSelect.appendChild(opt);
                    });
                }
                presetSelect.addEventListener('change', (e) => { entry.presetId = parseInt(e.target.value) || 0; });
                row.appendChild(presetSelect);
            }

            const btnRemove = document.createElement('button');
            btnRemove.className = 'icon-btn';
            btnRemove.style.cssText = 'color: #ff4757; background: rgba(255,71,87,0.1); width: 30px; height: 30px; flex-shrink: 0;';
            btnRemove.innerHTML = '<svg viewBox="0 0 24 24" width="14" height="14" stroke="currentColor" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round" style="pointer-events: none;"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>';
            btnRemove.addEventListener('click', () => {
                currentSchedules.entries.splice(idx, 1);
                renderSchedulesList();
            });
            row.appendChild(btnRemove);

            schedulesListContainer.appendChild(row);
        });
    }

    if (btnAddSchedule) {
        btnAddSchedule.addEventListener('click', () => {
            currentSchedules.entries.push({ hour: 20, minute: 0, days: 0x7F, action: 0, presetId: 0, enabled: true });
            renderSchedulesList();
        });
    }

    if (btnSaveSchedules) {
        btnSaveSchedules.addEventListener('click', () => {
            currentSchedules.timezone = scheduleTimezone.value;
            fetch('/api/schedules', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentSchedules)
            }).then(res => {
                if (res.ok) {
                    const originalText = btnSaveSchedules.innerText;
                    btnSaveSchedules.innerText = 'Gespeichert!';
                    btnSaveSchedules.style.backgroundColor = '#10b981';
                    setTimeout(() => {
                        btnSaveSchedules.innerText = originalText;
                        btnSaveSchedules.style.backgroundColor = '';
                    }, 1000);
                    fetchScheduleTime();
                }
            });
        });
    }

    // --- Multi-Panel Canvas (2D effects spanning Master + Slave matrices) ---
    let currentCanvasPanels = [];

    function fetchCanvasPanels() {
        fetch('/api/canvas_panels')
            .then(res => res.json())
            .then(data => {
                currentCanvasPanels = data || [];
                renderCanvasPanelsList();
            })
            .catch(() => {});
    }

    function renderCanvasPanelsList() {
        if (!canvasPanelsList) return;
        canvasPanelsList.innerHTML = '';
        const slaves = (window.hardwareLimits && window.hardwareLimits.slaves) ? window.hardwareLimits.slaves : [];

        if (currentCanvasPanels.length === 0) {
            const empty = document.createElement('div');
            empty.style.cssText = 'color: var(--text-muted); font-size: 12px; padding: 5px 0;';
            empty.setAttribute('data-i18n', 'canvas_none');
            empty.innerText = t('canvas_none') || 'Keine zusätzlichen Panels konfiguriert.';
            canvasPanelsList.appendChild(empty);
            return;
        }

        currentCanvasPanels.forEach((panel, idx) => {
            const row = document.createElement('div');
            row.style.cssText = 'background: rgba(255,255,255,0.05); border-radius: 8px; padding: 8px; display: flex; flex-wrap: wrap; gap: 6px; align-items: flex-end;';

            const slaveSelect = document.createElement('select');
            slaveSelect.style.cssText = 'flex: 1; min-width: 100px; padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none; font-size: 12px;';
            if (slaves.length === 0) {
                const opt = document.createElement('option');
                opt.innerText = t('dyn_slave') || 'Slave';
                slaveSelect.appendChild(opt);
            } else {
                slaves.forEach(s => {
                    const opt = document.createElement('option');
                    opt.style.cssText = 'background: #1f2937; color: white;';
                    opt.value = s.id;
                    opt.innerText = (s.name && s.name !== 'Unknown') ? s.name : ((t('dyn_slave') || 'Slave') + ' ' + s.id);
                    if (panel.slaveId === s.id) opt.selected = true;
                    slaveSelect.appendChild(opt);
                });
            }
            slaveSelect.addEventListener('change', (e) => { panel.slaveId = parseInt(e.target.value); });
            row.appendChild(slaveSelect);

            const makeNumberInput = (label, value, onChange, width) => {
                const wrap = document.createElement('div');
                wrap.style.cssText = 'width: ' + width + 'px;';
                const lbl = document.createElement('label');
                lbl.style.cssText = 'font-size: 10px; color: var(--text-muted); display: block;';
                lbl.innerText = label;
                wrap.appendChild(lbl);
                const input = document.createElement('input');
                input.type = 'number';
                input.min = '0';
                input.value = value;
                input.style.cssText = 'width: 100%; padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none; font-size: 12px;';
                input.addEventListener('change', (e) => onChange(parseInt(e.target.value) || 0));
                wrap.appendChild(input);
                return wrap;
            };

            row.appendChild(makeNumberInput(t('canvas_width') || 'Breite', panel.width, v => panel.width = v, 55));
            row.appendChild(makeNumberInput(t('canvas_height') || 'Höhe', panel.height, v => panel.height = v, 55));
            row.appendChild(makeNumberInput('X', panel.offsetX, v => panel.offsetX = v, 50));
            row.appendChild(makeNumberInput('Y', panel.offsetY, v => panel.offsetY = v, 50));

            const layoutSelect = document.createElement('select');
            layoutSelect.style.cssText = 'padding: 6px; background: rgba(0,0,0,0.2); border: 1px solid var(--border-color); border-radius: 6px; color: white; outline: none; font-size: 12px;';
            [[0, t('matrix_layout_serpentine') || 'Serpentine'], [1, t('matrix_layout_progressive') || 'Linear']].forEach(([val, label]) => {
                const opt = document.createElement('option');
                opt.style.cssText = 'background: #1f2937; color: white;';
                opt.value = val;
                opt.innerText = label;
                if (panel.layout === val) opt.selected = true;
                layoutSelect.appendChild(opt);
            });
            layoutSelect.addEventListener('change', (e) => { panel.layout = parseInt(e.target.value); });
            row.appendChild(layoutSelect);

            const btnRemove = document.createElement('button');
            btnRemove.className = 'icon-btn';
            btnRemove.style.cssText = 'color: #ff4757; background: rgba(255,71,87,0.1); width: 30px; height: 30px; flex-shrink: 0;';
            btnRemove.innerHTML = '<svg viewBox="0 0 24 24" width="14" height="14" stroke="currentColor" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round" style="pointer-events: none;"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>';
            btnRemove.addEventListener('click', () => {
                currentCanvasPanels.splice(idx, 1);
                renderCanvasPanelsList();
            });
            row.appendChild(btnRemove);

            canvasPanelsList.appendChild(row);
        });
    }

    if (btnAddCanvasPanel) {
        btnAddCanvasPanel.addEventListener('click', () => {
            const slaves = (window.hardwareLimits && window.hardwareLimits.slaves) ? window.hardwareLimits.slaves : [];
            const defaultSlaveId = slaves.length > 0 ? slaves[0].id : 0;
            currentCanvasPanels.push({ slaveId: defaultSlaveId, width: 16, height: 16, layout: 0, offsetX: 0, offsetY: 0 });
            renderCanvasPanelsList();
        });
    }

    if (btnSaveCanvasPanels) {
        btnSaveCanvasPanels.addEventListener('click', () => {
            fetch('/api/canvas_panels', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentCanvasPanels)
            }).then(res => {
                if (res.ok) {
                    const originalText = btnSaveCanvasPanels.innerText;
                    btnSaveCanvasPanels.innerText = 'Gespeichert!';
                    btnSaveCanvasPanels.style.backgroundColor = '#10b981';
                    setTimeout(() => {
                        btnSaveCanvasPanels.innerText = originalText;
                        btnSaveCanvasPanels.style.backgroundColor = '';
                    }, 1000);
                }
            });
        });
    }

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
            btn.style.cursor = 'grab';
            btn.innerText = `${translateSegmentName(seg.name) || (t('dyn_segment') + ' ' + idx)} (${seg.start + 1} - ${seg.stop})`;
            
            // Drag and Drop Logic
            btn.draggable = true;
            btn.addEventListener('dragstart', (e) => {
                e.dataTransfer.setData('text/plain', idx);
                btn.style.opacity = '0.5';
            });
            btn.addEventListener('dragend', () => {
                btn.style.opacity = '1';
            });
            btn.addEventListener('dragover', (e) => {
                e.preventDefault();
                btn.style.borderTop = '2px solid #10b981';
            });
            btn.addEventListener('dragleave', () => {
                btn.style.borderTop = '';
            });
            btn.addEventListener('drop', (e) => {
                e.preventDefault();
                btn.style.borderTop = '';
                const fromIdx = parseInt(e.dataTransfer.getData('text/plain'));
                if (fromIdx !== idx) {
                    const movedItem = segments.splice(fromIdx, 1)[0];
                    segments.splice(idx, 0, movedItem);
                    
                    // Adjust currentSegmentId
                    if (currentSegmentId === fromIdx) {
                        currentSegmentId = idx;
                    } else if (fromIdx < currentSegmentId && idx >= currentSegmentId) {
                        currentSegmentId--;
                    } else if (fromIdx > currentSegmentId && idx <= currentSegmentId) {
                        currentSegmentId++;
                    }
                    
                    // Recalculate start/stop logically so backend sorts them correctly
                    let currentPos = 0;
                    segments.forEach(s => {
                        let len = s.stop - s.start;
                        s.start = currentPos;
                        s.stop = currentPos + len;
                        currentPos = s.stop;
                    });
                    
                    renderSegmentsSelector();
                    
                    // Save to backend instantly
                    fetch('/api/segments', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(segments)
                    }).then(() => fetchState());
                }
            });

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
            state.palette = s.palette !== undefined ? s.palette : 0;
            if (paletteSelect) paletteSelect.value = state.palette;
            state.intensity = s.intensity !== undefined ? s.intensity : 128;
            if (intensitySlider) intensitySlider.value = state.intensity;
            if (s.color2 !== undefined && s.color2 !== null) {
                let c2 = (s.color2 & 0xFFFFFF).toString(16);
                while (c2.length < 6) c2 = "0" + c2;
                state.color2 = "#" + c2;
                if (color2Wheel) color2Wheel.color.hexString = state.color2;
            }
            state.color2Enabled = s.color2Enabled !== undefined ? s.color2Enabled : false;
            if (color2EnabledToggle) color2EnabledToggle.checked = state.color2Enabled;
            if (color2WheelWrapper) color2WheelWrapper.style.display = state.color2Enabled ? 'flex' : 'none';

            const whiteSliderLabel = document.getElementById('whiteSliderLabel');
            const cwEl = document.getElementById('colorWheel');
            if (s.effect === 10) {
                if (whiteSlider) whiteSlider.value = s.cct;
                if (whiteSliderLabel) whiteSliderLabel.innerText = t('dash_white_cct') || 'Weißton (Warm -> Kalt)';
                if (cwEl) {
                    cwEl.style.opacity = '0.3';
                    cwEl.style.pointerEvents = 'none';
                }
            } else {
                if (whiteSlider) whiteSlider.value = (s.color >> 24) & 0xFF;
                if (whiteSliderLabel) whiteSliderLabel.innerText = t('dash_white_channel') || 'Weiß-Kanal';
                if (cwEl) {
                    cwEl.style.opacity = '1';
                    cwEl.style.pointerEvents = 'auto';
                }
            }
            
            if (btnSegPower) {
                if (s.on) btnSegPower.classList.add('on');
                else btnSegPower.classList.remove('on');
            }
            
            if (s.color !== undefined && s.color !== null) {
                let c = s.color.toString(16);
                while (c.length < 6) c = "0" + c;
                state.color = "#" + c.substring(c.length - 6);
                state.white = (s.color >> 24) & 0xFF;
            }
            
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
                    slavesList.innerHTML = `<div style="text-align: center; padding: 20px; color: var(--text-muted);">${t('dyn_no_slaves_found')}</div>`;
                    return;
                }
                
                data.forEach(slave => {
                    const isConfigured = slave.id !== 254;
                    const card = document.createElement('div');
                    card.style.cssText = 'background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; border: 1px solid rgba(255,255,255,0.1);';
                    
                    // Excludes GPIO16/17/18/38 (HyperBus UART, always active regardless of LED
                    // type) and the board's other reserved pins (strapping, native USB, PSRAM,
                    // onboard WS2812, debug UART0 - see include/Config.h). HUB75's own pins
                    // (1,2,4-15) are fine to offer here since they're only reserved while HUB75
                    // is the selected type.
                    const freePins = [1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 39, 40, 41, 42];
                    let pinOptions = '';
                    freePins.forEach(p => {
                        pinOptions += `<option value="${p}" ${p === 4 ? 'selected' : ''}>GPIO ${p}</option>`;
                    });
                    
                    const typeOptions = document.getElementById('ledType').innerHTML;
                    
                    card.innerHTML = `
                        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                            <div style="display: flex; align-items: center; gap: 10px;">
                                <h4 style="margin: 0; color: ${isConfigured ? 'var(--primary)' : '#fbbf24'};">${isConfigured ? t('dyn_slave_configured') + ' (ID ' + slave.id + ')' : t('dyn_slave_new')}</h4>
                                <span style="font-size: 11px; padding: 2px 6px; border-radius: 4px; background: ${slave.isWireless ? 'rgba(59, 130, 246, 0.2)' : 'rgba(99, 102, 241, 0.2)'}; color: ${slave.isWireless ? '#60a5fa' : '#818cf8'}; border: 1px solid ${slave.isWireless ? 'rgba(59, 130, 246, 0.4)' : 'rgba(99, 102, 241, 0.4)'};">${slave.isWireless ? 'Wifi' : 'UART'}</span>
                            </div>
                            <span style="font-size: 12px; color: var(--text-muted);">${t('dyn_slave_seen')}${Math.round(slave.lastSeenAge / 1000)}${t('dyn_slave_seen_suffix')}</span>
                        </div>
                        <div style="font-size: 12px; color: var(--text-muted); margin-bottom: 10px;">
                            ${t('dyn_slave_version')} <strong style="color: white;">${slave.version || t('dyn_slave_unknown_ver')}</strong>
                        </div>
                        <div class="form-group" style="margin-bottom: 10px;">
                            <label>${t('dyn_slave_name')}</label>
                            <input type="text" id="slaveName_${slave.id}" value="${slave.name === 'Unknown' ? t('dyn_slave') + ' ' + slave.id : slave.name}">
                        </div>
                        <div class="form-group" style="margin-bottom: 10px;">
                            <label>${t('dyn_slave_led_type')}</label>
                            <select id="slaveType_${slave.id}" onchange="onSlaveTypeChange(${slave.id})">
                                ${typeOptions}
                            </select>
                        </div>
                        <div id="groupSlaveNormal_${slave.id}" style="display: flex; gap: 10px; margin-bottom: 10px;">
                            <div class="form-group" style="flex: 1;">
                                <label>${t('dyn_slave_data_pin')}</label>
                                <select id="slavePin_${slave.id}">
                                    ${pinOptions}
                                </select>
                            </div>
                            <div class="form-group" style="flex: 1; display: none;" id="groupSlavePin2_${slave.id}">
                                <label>${t('dyn_slave_clock_pin')}</label>
                                <select id="slavePin2_${slave.id}">
                                    ${pinOptions.replace('selected', '')}
                                </select>
                            </div>
                            <div class="form-group" style="flex: 1;">
                                <label>${t('dyn_slave_led_count')}</label>
                                <input type="number" id="slaveLeds_${slave.id}" min="0" max="1000" value="${slave.ledCount}">
                            </div>
                        </div>
                        <div id="groupSlaveHub75_${slave.id}" style="display: none; margin-bottom: 10px;">
                            <p style="font-size: 12px; color: var(--text-muted); margin-bottom: 8px;">${t('dyn_slave_hub75_hint')}</p>
                            <div style="background: rgba(0,0,0,0.2); border-radius: 8px; padding: 10px; margin-bottom: 10px;">
                                <h5 style="margin: 0 0 8px 0; color: var(--primary); font-size: 13px;">${t('hub75_pinout_title')}</h5>
                                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 4px 16px; font-size: 12px; font-family: monospace;" id="slaveHub75Pinout_${slave.id}">
                                    <!-- Filled below from the same fixed HUB75_PIN_* list as the Master -->
                                </div>
                            </div>
                            <div style="display: flex; gap: 10px; margin-bottom: 8px;">
                                <div class="form-group" style="flex: 1;">
                                    <label>${t('matrix_width') || 'Breite'}</label>
                                    <input type="number" id="slaveMatW_${slave.id}" min="1" max="128" value="16">
                                </div>
                                <div class="form-group" style="flex: 1;">
                                    <label>${t('matrix_height') || 'Höhe'}</label>
                                    <input type="number" id="slaveMatH_${slave.id}" min="1" max="128" value="16">
                                </div>
                            </div>
                            <div class="form-group">
                                <label>${t('hub75_shift_driver') || 'Treiber-Chip'}</label>
                                <select id="slaveShiftDriver_${slave.id}">
                                    <option value="0">Generic</option>
                                    <option value="1">FM6126A</option>
                                    <option value="2">ICN2038S</option>
                                    <option value="3">FM6124</option>
                                    <option value="4">MBI5124</option>
                                    <option value="5">DP3246</option>
                                </select>
                            </div>
                        </div>
                        <button id="btnSaveSlave_${slave.id}" class="btn-primary" onclick="configureSlave(${slave.id})" style="width: 100%; padding: 8px;">${t('dyn_send_config')}</button>

                        <div style="margin-top: 15px; padding-top: 12px; border-top: 1px solid rgba(255,255,255,0.1);">
                            <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;">
                                <label for="slaveLedOn_${slave.id}" style="margin: 0; font-size: 14px;">${t('slave_led_title')}</label>
                                <input type="checkbox" id="slaveLedOn_${slave.id}" checked style="width: auto; transform: scale(1.3);" onchange="sendSlaveStatusLed(${slave.id})">
                            </div>
                            <div style="display: flex; gap: 10px; align-items: center;">
                                <input type="color" id="slaveLedColor_${slave.id}" value="#00ff00" style="width: 50px; height: 34px; padding: 3px; cursor: pointer;" onchange="sendSlaveStatusLed(${slave.id})">
                                <input type="range" id="slaveLedBri_${slave.id}" min="1" max="255" value="40" style="flex: 1;" onchange="sendSlaveStatusLed(${slave.id})">
                            </div>
                            <p style="font-size: 11px; color: var(--text-muted); margin: 8px 0 0 0;">${t('slave_led_hint')}</p>
                        </div>
                    `;
                    slavesList.appendChild(card);

                    // Set correct type if it was already configured
                    // (Requires backend to send type, but for now defaults to current master type or user selects)
                    // We can just trigger onchange to update pin2/HUB75 group visibility
                    if (slave.configPending) {
                        const pending = document.createElement('p');
                        pending.style.cssText = 'font-size: 12px; color: var(--primary); margin: 8px 0 0 0;';
                        pending.innerText = t('slave_config_pending');
                        card.appendChild(pending);
                    }
                    renderHub75Pinout(`slaveHub75Pinout_${slave.id}`);
                    document.getElementById(`slaveType_${slave.id}`).dispatchEvent(new Event('change'));
                });
            })
            .catch(err => {
                slavesList.innerHTML = `<div style="text-align: center; padding: 20px; color: #ef4444;">${t('dyn_err_load_slaves')}</div>`;
            });
    }

    // Sends only the "everything is fine" appearance. The Slave still signals problems
    // (unconfigured, connection lost) by itself, even when the LED is switched off here.
    window.sendSlaveStatusLed = function(slaveId) {
        const onEl = document.getElementById(`slaveLedOn_${slaveId}`);
        const colorEl = document.getElementById(`slaveLedColor_${slaveId}`);
        const briEl = document.getElementById(`slaveLedBri_${slaveId}`);
        if (!onEl || !colorEl || !briEl) return;
        fetch('/api/slaves/statusled', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                id: slaveId,
                on: onEl.checked,
                color: parseInt(colorEl.value.slice(1), 16),
                bri: parseInt(briEl.value)
            })
        }).catch(e => console.error("Slave status LED error:", e));
    };

    window.onSlaveTypeChange = function(slaveId) {
        const typeEl = document.getElementById(`slaveType_${slaveId}`);
        const type = typeEl ? parseInt(typeEl.value) : 0;
        const pin2Group = document.getElementById(`groupSlavePin2_${slaveId}`);
        if (pin2Group) pin2Group.style.display = (type >= 50 && type <= 54) ? 'block' : 'none';
        const isHub75 = type === 60;
        const normalGroup = document.getElementById(`groupSlaveNormal_${slaveId}`);
        const hub75Group = document.getElementById(`groupSlaveHub75_${slaveId}`);
        if (normalGroup) normalGroup.style.display = isHub75 ? 'none' : 'flex';
        if (hub75Group) hub75Group.style.display = isHub75 ? 'block' : 'none';
    };

    window.configureSlave = function(currentId) {
        const name = document.getElementById(`slaveName_${currentId}`).value;
        const typeEl = document.getElementById(`slaveType_${currentId}`);
        const type = typeEl ? parseInt(typeEl.value) : parseInt(document.getElementById('ledType').value);
        const isHub75 = type === 60;

        let pin = 4, pin2 = 255, ledCount = 0, matrixWidth = 16, matrixHeight = 16, hub75ShiftDriver = 0;
        if (isHub75) {
            matrixWidth = parseInt(document.getElementById(`slaveMatW_${currentId}`).value) || 16;
            matrixHeight = parseInt(document.getElementById(`slaveMatH_${currentId}`).value) || 16;
            hub75ShiftDriver = parseInt(document.getElementById(`slaveShiftDriver_${currentId}`).value) || 0;
            ledCount = matrixWidth * matrixHeight; // matches how the Master derives its own HUB75 pixel count
        } else {
            pin = parseInt(document.getElementById(`slavePin_${currentId}`).value);
            const pin2El = document.getElementById(`slavePin2_${currentId}`);
            if (pin2El) pin2 = parseInt(pin2El.value);
            ledCount = parseInt(document.getElementById(`slaveLeds_${currentId}`).value);
        }
        const newId = currentId === 254 ? Math.floor(Math.random() * 253) + 1 : currentId; // Assign random ID 1-253 if new

        fetch('/api/slaves/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ currentId, newId, pin, pin2, ledCount, type, name, matrixWidth, matrixHeight, hub75ShiftDriver })
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

    // Compares two dotted numeric version strings (e.g. "0.1.112"). Returns >0 if a > b,
    // <0 if a < b, 0 if equal. Missing/non-numeric parts are treated as 0.
    function compareVersions(a, b) {
        const pa = String(a).split('.').map(n => parseInt(n) || 0);
        const pb = String(b).split('.').map(n => parseInt(n) || 0);
        const len = Math.max(pa.length, pb.length);
        for (let i = 0; i < len; i++) {
            const diff = (pa[i] || 0) - (pb[i] || 0);
            if (diff !== 0) return diff;
        }
        return 0;
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
                        
                        if (compareVersions(displayVer, currentVer) > 0) {
                            sysVersion.innerHTML = `<span style="color: #ef4444;">${currentVer}</span> <span style="color: var(--text-muted);">(${t('dyn_latest')}${latestVer})</span>`;
                            btnCheckUpdate.innerText = `Update auf ${latestVer}`;
                            btnCheckUpdate.disabled = false;
                            btnCheckUpdate.onclick = () => startOnlineUpdate(latestVer);
                            
                            // Show update banner
                            const banner = document.getElementById('updateBanner');
                            const bannerTitle = document.getElementById('updateBannerTitle');
                            const bannerText = document.getElementById('updateBannerText');
                            if (banner && bannerTitle && bannerText) {
                                bannerTitle.innerText = `Update ${latestVer} verfügbar!`;
                                bannerText.innerText = 'Klicke hier, um das Update zu starten.';
                                banner.style.display = 'block';
                            }
                        } else {
                            sysVersion.innerHTML = `<span style="color: #10b981;">${currentVer}</span>`;
                            btnCheckUpdate.innerText = t('dyn_firmware_up_to_date');
                            btnCheckUpdate.disabled = true;
                        }
                    })
                    .catch(err => {
                        console.log('GitHub API error or no releases yet', err);
                        sysVersion.innerHTML = `<span style="color: #10b981;">${currentVer}</span>`;
                        btnCheckUpdate.innerText = t('dyn_firmware_up_to_date');
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
                        
                        let needsUpdate = activeSlaves.some(s => s.version && compareVersions(displayVer, s.version) > 0);
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
                alert(t('dyn_err_update_start'));
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
                    alert(t('dyn_update_failed'));
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
            alert(t('dyn_choose_bin'));
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
                alert(t('dyn_update_failed'));
                otaProgressContainer.style.display = 'none';
                otaProgressBar.style.width = '0%';
                btnUpdateLocal.disabled = false;
                btnUpdateLocal.innerText = 'Update starten';
            }
        });

        request.send(formData);
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
        
        fetch('/api/state', { cache: 'no-store' })
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
                if(data.ledsPerIC !== undefined && document.getElementById("ledsPerIC")) document.getElementById("ledsPerIC").value = data.ledsPerIC;
                ledType.value = data.type;
                updateEffectVisibility(data.type);
                if(data.abl_en !== undefined) ablEnable.checked = data.abl_en;
                if(data.abl_ma !== undefined) ablMaxMa.value = data.abl_ma;
                
                if(data.matrix_en !== undefined) {
                    matrixEnable.checked = data.matrix_en;
                    matrixWidth.value = data.matrix_w;
                    matrixHeight.value = data.matrix_h;
                    matrixLayout.value = data.matrix_l;
                    matrixConfigGroup.style.display = data.matrix_en ? 'block' : 'none';
                }
                if (data.hub75_shift_driver !== undefined) {
                    const sel = document.getElementById('hub75ShiftDriver');
                    if (sel) sel.value = data.hub75_shift_driver;
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
    
    // --- Onboard status LED ---
    async function fetchStatusLed() {
        try {
            const res = await fetch('/api/statusled');
            if (!res.ok) return;
            const data = await res.json();
            statusLedOn.checked = !!data.on;
            statusLedColor.value = '#' + (data.color >>> 0).toString(16).padStart(6, '0');
            statusLedBri.value = data.bri;
            statusLedOptions.style.opacity = data.on ? '1' : '0.4';
        } catch (e) {
            console.error("Status LED fetch error:", e);
        }
    }

    function sendStatusLed() {
        statusLedOptions.style.opacity = statusLedOn.checked ? '1' : '0.4';
        fetch('/api/statusled', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                on: statusLedOn.checked,
                color: parseInt(statusLedColor.value.slice(1), 16),
                bri: parseInt(statusLedBri.value)
            })
        }).catch(e => console.error("Status LED save error:", e));
    }

    if (statusLedOn) {
        statusLedOn.addEventListener('change', sendStatusLed);
        statusLedColor.addEventListener('input', throttle(sendStatusLed, 200));
        statusLedBri.addEventListener('input', throttle(sendStatusLed, 200));
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
        if (updates.palette !== undefined) state.palette = updates.palette;
        if (updates.intensity !== undefined) state.intensity = updates.intensity;
        if (updates.color2 !== undefined) state.color2 = updates.color2;
        if (updates.color2Enabled !== undefined) state.color2Enabled = updates.color2Enabled;

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
                    if (updates.palette !== undefined) segments[i].palette = updates.palette;
                    if (updates.intensity !== undefined) segments[i].intensity = updates.intensity;
                    if (updates.color2 !== undefined) segments[i].color2 = updates.color2;
                    if (updates.color2Enabled !== undefined) segments[i].color2Enabled = updates.color2Enabled;
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
                if (updates.palette !== undefined) segments[currentSegmentId].palette = updates.palette;
                if (updates.intensity !== undefined) segments[currentSegmentId].intensity = updates.intensity;
                if (updates.color2 !== undefined) segments[currentSegmentId].color2 = updates.color2;
                if (updates.color2Enabled !== undefined) segments[currentSegmentId].color2Enabled = updates.color2Enabled;
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
        if (paletteSelect) paletteSelect.value = state.palette;
        if (intensitySlider) intensitySlider.value = state.intensity;
        if (color2Wheel) color2Wheel.color.hexString = state.color2;
        if (color2EnabledToggle) color2EnabledToggle.checked = state.color2Enabled;
        if (color2WheelWrapper) color2WheelWrapper.style.display = state.color2Enabled ? 'flex' : 'none';

        effectBtns.forEach(btn => {
            if (parseInt(btn.dataset.id) === state.effect) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });

        if (textEffectControls) {
            textEffectControls.style.display = state.effect === EFFECT_TEXT_ID ? 'block' : 'none';
        }
        if (typeof renderTextWidgetEditor === 'function') renderTextWidgetEditor();
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
                ctx.imageSmoothingEnabled = false; // crisp per-pixel sampling instead of a blurred average
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
        const original = btnStreamMatrix.innerText;
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

        streamToBackgroundWidget(pixelArray, w, h).then(() => {
            btnStreamMatrix.innerText = 'Übernommen!';
            setTimeout(() => {
                btnStreamMatrix.innerText = original;
                btnStreamMatrix.disabled = false;
            }, 2000);
        }).catch(() => {
            btnStreamMatrix.innerText = 'Fehler!';
            setTimeout(() => {
                btnStreamMatrix.innerText = original;
                btnStreamMatrix.disabled = false;
            }, 2000);
        });
    });

    // --- Matrix Live Preview ---
    const matrixPreviewCanvas = document.getElementById('matrixPreviewCanvas');
    let matrixPreviewTimer = null;

    function editorDims() {
        return { w: parseInt(matrixWidth.value) || 16, h: parseInt(matrixHeight.value) || 16 };
    }

    function previewCellSize(w, h) {
        return Math.max(2, Math.min(20, Math.floor(280 / Math.max(w, h))));
    }

    function renderMatrixPreview(colors) {
        if (!matrixPreviewCanvas || !Array.isArray(colors)) return;
        const { w, h } = editorDims();
        if (colors.length !== w * h) return;
        const cell = previewCellSize(w, h);
        matrixPreviewCanvas.width = w * cell;
        matrixPreviewCanvas.height = h * cell;
        const ctx = matrixPreviewCanvas.getContext('2d');
        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                ctx.fillStyle = '#' + colors[y * w + x].toString(16).padStart(6, '0');
                ctx.fillRect(x * cell, y * cell, cell, cell);
            }
        }
        if (widgetOverlayCanvas && widgetOverlayCanvas.style.display !== 'none') {
            drawWidgetOverlay(currentWidgets());
        }
    }

    function fetchMatrixPreview() {
        if (!matrixPreviewCanvas) return;
        fetch('/api/matrix_preview')
            .then(res => res.json())
            .then(renderMatrixPreview)
            .catch(() => {});
    }

    function startMatrixPreview() {
        if (matrixPreviewTimer) return;
        fetchMatrixPreview();
        matrixPreviewTimer = setInterval(fetchMatrixPreview, 500);
    }

    function stopMatrixPreview() {
        if (matrixPreviewTimer) {
            clearInterval(matrixPreviewTimer);
            matrixPreviewTimer = null;
        }
    }

    // --- "Uhr / Text" Widget Editor ---
    const textWidgetEditor = document.getElementById('textWidgetEditor');
    const textWidgetList = document.getElementById('textWidgetList');
    const btnAddTextWidget = document.getElementById('btnAddTextWidget');
    const widgetOverlayCanvas = document.getElementById('widgetOverlayCanvas');
    let draggingWidget = null;
    let dragStart = { mx: 0, my: 0, ox: 0, oy: 0 };

    function widgetTypeLabel(type) {
        const keys = ['widget_type_clock', 'widget_type_date', 'widget_type_text', 'widget_type_image', 'widget_type_analog', 'widget_type_weather'];
        const fallback = ['Uhrzeit', 'Datum', 'Text', 'Bild', 'Analoguhr', 'Wetter'];
        const key = keys[type] || keys[0];
        const translated = typeof t === 'function' ? t(key) : key;
        return translated === key ? (fallback[type] || fallback[0]) : translated;
    }

    const WIDGET_FORMAT_KEYS = {
        0: ['widget_fmt_c0', 'widget_fmt_c1', 'widget_fmt_c2'],
        1: ['widget_fmt_d0', 'widget_fmt_d1', 'widget_fmt_d2', 'widget_fmt_d3', 'widget_fmt_d4', 'widget_fmt_d5'],
        4: ['widget_fmt_a0', 'widget_fmt_a1', 'widget_fmt_a2', 'widget_fmt_a3'],
        5: ['widget_fmt_w0', 'widget_fmt_w1', 'widget_fmt_w2']
    };
    const WIDGET_FORMAT_FALLBACK = {
        0: ['14:32', '14:32:05', '02:32PM'],
        1: ['13.11.', '13.11.2026', '13.11.26', '2026-11-13', '11/13/2026', 'FR 13.11.'],
        4: ['Klassisch', 'Minimal', 'Punkte + Sekunde', 'Kreuz'],
        5: ['Symbol + Temperatur', 'Nur Symbol', 'Nur Temperatur']
    };
    function widgetFormatLabel(type, format) {
        const keys = WIDGET_FORMAT_KEYS[type];
        const fallback = WIDGET_FORMAT_FALLBACK[type];
        if (!keys) return '';
        const key = keys[format] || keys[0];
        const translated = typeof t === 'function' ? t(key) : key;
        return translated === key ? (fallback[format] || fallback[0]) : translated;
    }
    function widgetFormatOptionsHtml(w) {
        const keys = WIDGET_FORMAT_KEYS[w.type];
        if (!keys) return '';
        const current = w.format || 0;
        const opts = keys.map((_, idx) =>
            `<option value="${idx}" ${current === idx ? 'selected' : ''}>${widgetFormatLabel(w.type, idx)}</option>`
        ).join('');
        return `<div class="form-group" style="margin-bottom:8px;">
            <select data-field="format" style="padding:6px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">${opts}</select>
        </div>`;
    }
    function widgetFontLabel(key, fallback) {
        const translated = typeof t === 'function' ? t(key) : key;
        return translated === key ? fallback : translated;
    }
    function widgetFontOptionsHtml(w) {
        if (w.type !== 0 && w.type !== 1 && w.type !== 2 && w.type !== 5) return '';
        const current = w.font || 0;
        return `<div class="form-group" style="margin-bottom:8px;">
            <select data-field="font" style="padding:6px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">
                <option value="0" ${current === 0 ? 'selected' : ''}>${widgetFontLabel('widget_font_normal', 'Normal (5x7)')}</option>
                <option value="1" ${current === 1 ? 'selected' : ''}>${widgetFontLabel('widget_font_mini', 'Mini (3x5)')}</option>
            </select>
        </div>`;
    }

    function currentWidgets() {
        const seg = segments[currentSegmentId];
        return (seg && Array.isArray(seg.widgets)) ? seg.widgets : [];
    }

    function widgetScale(w) {
        return Math.max(1, Math.min(8, w.scale || 1));
    }
    // Expected rendered character count for each Uhrzeit/Datum format, used only
    // to size the drag/overlay hit-box - must mirror the snprintf layouts in
    // LEDManagerClass::effectText (LEDManager.cpp).
    const CLOCK_FORMAT_LEN = [5, 8, 7];         // HH:MM / HH:MM:SS / HH:MMAM|PM
    const DATE_FORMAT_LEN = [6, 10, 8, 10, 10, 9]; // DD.MM. / DD.MM.YYYY / DD.MM.YY / ISO / US / Weekday DD.MM.
    // Glyph cell size (width incl. 1px gap, height) per font - must mirror
    // FONT5X7_GLYPH_WIDTH/HEIGHT and FONT3X5_GLYPH_WIDTH/HEIGHT in the firmware.
    function widgetGlyphCell(w) {
        return w.font === 1 ? { w: 4, h: 5 } : { w: 6, h: 7 };
    }
    function widgetPixelWidth(w) {
        if (w.type === 4) return w.w || 16; // Analoguhr: diameter, no scale
        const scale = widgetScale(w);
        if (w.type === 3) return (w.w || 1) * scale;
        const cell = widgetGlyphCell(w);
        if (w.type === 5) {
            let width = 0;
            if (w.format !== 2) width += 8; // 7px icon + 1px gap
            if (w.format !== 1) width += 3 * cell.w - 1; // ~3-char temperature, e.g. "18C"
            return Math.max(width, 1) * scale;
        }
        let len = 5;
        if (w.type === 0) len = CLOCK_FORMAT_LEN[w.format] || CLOCK_FORMAT_LEN[0];
        else if (w.type === 1) len = DATE_FORMAT_LEN[w.format] || DATE_FORMAT_LEN[0];
        else if (w.type === 2) len = (w.text || '').length || 1;
        return (len * cell.w - 1) * scale;
    }
    function widgetPixelHeight(w) {
        if (w.type === 4) return w.h || 16; // Analoguhr: diameter, no scale
        const scale = widgetScale(w);
        if (w.type === 3) return (w.h || 1) * scale;
        if (w.type === 5) return Math.max(7, widgetGlyphCell(w).h) * scale;
        return widgetGlyphCell(w).h * scale;
    }

    function saveWidgets(widgets) {
        return fetch('/api/text_widgets', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                seg: currentSegmentId,
                widgets: widgets.map(w => ({ id: w.id || 0, type: w.type, x: w.x, y: w.y, color: w.color, text: w.text || '', w: w.w || 0, h: w.h || 0, scale: w.scale || 1, format: w.format || 0, font: w.font || 0 }))
            })
        }).then(() => fetch('/api/segments')).then(res => res.json()).then(data => {
            if (Array.isArray(data)) segments = data;
            renderTextWidgetEditor();
        }).catch(() => {});
    }

    function drawWidgetOverlay(widgets) {
        if (!widgetOverlayCanvas) return;
        const { w: mw, h: mh } = editorDims();
        const cell = previewCellSize(mw, mh);
        widgetOverlayCanvas.width = mw * cell;
        widgetOverlayCanvas.height = mh * cell;
        const ctx = widgetOverlayCanvas.getContext('2d');
        ctx.clearRect(0, 0, widgetOverlayCanvas.width, widgetOverlayCanvas.height);
        widgets.forEach((w, idx) => {
            const bw = Math.max(1, widgetPixelWidth(w)) * cell;
            const bh = Math.max(1, widgetPixelHeight(w)) * cell;
            const bx = w.x * cell;
            const by = w.y * cell;
            ctx.strokeStyle = w === draggingWidget ? '#34d399' : 'rgba(52, 211, 153, 0.75)';
            ctx.lineWidth = 2;
            ctx.strokeRect(bx + 1, by + 1, Math.max(bw - 2, 1), Math.max(bh - 2, 1));
            ctx.fillStyle = 'rgba(52, 211, 153, 0.9)';
            ctx.font = '10px sans-serif';
            ctx.fillText(String(idx + 1), bx + 2, by + 9);
        });
    }

    function renderTextWidgetEditor() {
        if (!textWidgetEditor) return;
        const show = state.effect === EFFECT_TEXT_ID;
        textWidgetEditor.style.display = show ? 'block' : 'none';
        if (widgetOverlayCanvas) widgetOverlayCanvas.style.display = show ? 'block' : 'none';
        if (!show) return;

        const widgets = currentWidgets();
        textWidgetList.innerHTML = '';
        widgets.forEach((w) => {
            const card = document.createElement('div');
            card.className = 'card glass';
            card.style.cssText = 'margin-bottom: 0; padding: 12px; background: rgba(0,0,0,0.2); box-shadow: none;';
            const hexColor = '#' + (w.color !== undefined ? w.color : 0xFFFFFF).toString(16).padStart(6, '0');
            const safeText = (w.text || '').replace(/"/g, '&quot;');
            card.innerHTML = `
                <div style="display:flex; justify-content:space-between; align-items:center; gap:8px; margin-bottom:8px;">
                    <select data-field="type" style="flex:1; padding:6px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">
                        <option value="0" ${w.type === 0 ? 'selected' : ''}>${widgetTypeLabel(0)}</option>
                        <option value="1" ${w.type === 1 ? 'selected' : ''}>${widgetTypeLabel(1)}</option>
                        <option value="2" ${w.type === 2 ? 'selected' : ''}>${widgetTypeLabel(2)}</option>
                        <option value="3" ${w.type === 3 ? 'selected' : ''}>${widgetTypeLabel(3)}</option>
                        <option value="4" ${w.type === 4 ? 'selected' : ''}>${widgetTypeLabel(4)}</option>
                        <option value="5" ${w.type === 5 ? 'selected' : ''}>${widgetTypeLabel(5)}</option>
                    </select>
                    <span style="font-size:12px; color: var(--text-muted); white-space:nowrap;" id="widgetPos_${w.id}">X:${w.x} Y:${w.y}</span>
                    <button type="button" data-action="remove" style="background:none; border:none; color:#ef4444; cursor:pointer; font-size:18px; line-height:1; padding:0 4px;">&times;</button>
                </div>
                ${w.type !== 4 ? `<div style="display:flex; align-items:center; gap:8px; margin-bottom:8px;">
                    <label style="font-size:12px; color:var(--text-muted);" data-i18n="widget_size">Größe</label>
                    <input type="number" data-field="scale" min="1" max="8" value="${widgetScale(w)}" style="width:55px; padding:4px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">
                    <span style="font-size:11px; color:var(--text-muted);">x</span>
                </div>` : ''}
                ${widgetFontOptionsHtml(w)}
                ${widgetFormatOptionsHtml(w)}
                ${w.type === 2 ? `<input type="text" data-field="text" maxlength="64" value="${safeText}" placeholder="HELLO" style="width:100%; padding:6px; margin-bottom:8px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">` : ''}
                ${w.type !== 3 ? `<input type="color" data-field="color" value="${hexColor}" style="width:40px; height:28px; padding:2px; border-radius:6px; border:1px solid var(--border-color); background: rgba(0,0,0,0.2);">` : ''}
                ${w.type === 3 ? `<input type="file" data-field="image" accept="image/*" style="display:none;">
                    <div style="display:flex; gap:8px; align-items:center; flex-wrap: wrap;">
                        <input type="number" data-field="imgW" min="1" max="64" value="${w.w || 8}" style="width:55px; padding:4px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">
                        <span style="color:var(--text-muted);">x</span>
                        <input type="number" data-field="imgH" min="1" max="64" value="${w.h || 8}" style="width:55px; padding:4px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">
                        <button type="button" data-action="upload" class="btn btn-secondary" style="flex:1; font-size:12px; padding:6px;" data-i18n="widget_img_select">Bild wählen</button>
                    </div>` : ''}
                ${w.type === 4 ? `<div style="display:flex; align-items:center; gap:8px;">
                        <label style="font-size:12px; color:var(--text-muted);" data-i18n="widget_diameter">Durchmesser</label>
                        <input type="number" data-field="diameter" min="8" max="64" value="${w.w || 16}" style="width:55px; padding:4px; background: rgba(0,0,0,0.2); border:1px solid var(--border-color); border-radius:6px; color:white;">
                    </div>` : ''}
            `;
            textWidgetList.appendChild(card);

            const typeSelect = card.querySelector('[data-field="type"]');
            typeSelect.addEventListener('change', () => {
                w.type = parseInt(typeSelect.value);
                saveWidgets(widgets);
            });
            const scaleInput = card.querySelector('[data-field="scale"]');
            if (scaleInput) {
                scaleInput.addEventListener('change', () => {
                    w.scale = Math.max(1, Math.min(8, parseInt(scaleInput.value) || 1));
                    saveWidgets(widgets);
                });
            }
            const formatSelect = card.querySelector('[data-field="format"]');
            if (formatSelect) {
                formatSelect.addEventListener('change', () => {
                    w.format = parseInt(formatSelect.value) || 0;
                    saveWidgets(widgets);
                });
            }
            const diameterInput = card.querySelector('[data-field="diameter"]');
            if (diameterInput) {
                diameterInput.addEventListener('change', () => {
                    const d = Math.max(8, Math.min(64, parseInt(diameterInput.value) || 16));
                    w.w = d;
                    w.h = d;
                    saveWidgets(widgets);
                });
            }
            const fontSelect = card.querySelector('[data-field="font"]');
            if (fontSelect) {
                fontSelect.addEventListener('change', () => {
                    w.font = parseInt(fontSelect.value) || 0;
                    saveWidgets(widgets);
                });
            }
            const textInput = card.querySelector('[data-field="text"]');
            if (textInput) {
                textInput.addEventListener('change', () => {
                    w.text = textInput.value;
                    saveWidgets(widgets);
                });
            }
            const colorInput = card.querySelector('[data-field="color"]');
            if (colorInput) {
                colorInput.addEventListener('input', () => {
                    w.color = parseInt(colorInput.value.replace('#', ''), 16);
                    drawWidgetOverlay(widgets);
                });
                colorInput.addEventListener('change', () => {
                    w.color = parseInt(colorInput.value.replace('#', ''), 16);
                    saveWidgets(widgets);
                });
            }
            const removeBtn = card.querySelector('[data-action="remove"]');
            removeBtn.addEventListener('click', () => {
                const idx = widgets.indexOf(w);
                if (idx > -1) widgets.splice(idx, 1);
                saveWidgets(widgets);
            });
            const imgWInput = card.querySelector('[data-field="imgW"]');
            const imgHInput = card.querySelector('[data-field="imgH"]');
            const uploadBtn = card.querySelector('[data-action="upload"]');
            const fileInput = card.querySelector('[data-field="image"]');
            if (uploadBtn && fileInput) {
                uploadBtn.addEventListener('click', () => fileInput.click());
                fileInput.addEventListener('change', (e) => {
                    const file = e.target.files[0];
                    if (!file) return;
                    const iw = Math.max(1, Math.min(64, parseInt(imgWInput.value) || 8));
                    const ih = Math.max(1, Math.min(64, parseInt(imgHInput.value) || 8));
                    const reader = new FileReader();
                    reader.onload = (ev) => {
                        const img = new Image();
                        img.onload = () => {
                            const tmp = document.createElement('canvas');
                            tmp.width = iw;
                            tmp.height = ih;
                            const ctx = tmp.getContext('2d');
                            ctx.imageSmoothingEnabled = false;
                            ctx.drawImage(img, 0, 0, iw, ih);
                            const data = ctx.getImageData(0, 0, iw, ih).data;
                            const pixels = [];
                            for (let i = 0; i < data.length; i += 4) {
                                pixels.push((data[i] << 16) | (data[i + 1] << 8) | data[i + 2]);
                            }
                            uploadBtn.disabled = true;
                            fetch('/api/text_widget_image', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({ seg: currentSegmentId, widget: w.id, w: iw, h: ih, pixels })
                            }).then(() => fetch('/api/segments')).then(res => res.json()).then(d => {
                                if (Array.isArray(d)) segments = d;
                                renderTextWidgetEditor();
                            }).catch(() => {}).finally(() => { uploadBtn.disabled = false; });
                        };
                        img.src = ev.target.result;
                    };
                    reader.readAsDataURL(file);
                });
            }
        });

        drawWidgetOverlay(widgets);
    }

    function overlayCellFromEvent(clientX, clientY) {
        const { w: mw, h: mh } = editorDims();
        const rect = widgetOverlayCanvas.getBoundingClientRect();
        const mx = Math.floor((clientX - rect.left) / (rect.width / mw));
        const my = Math.floor((clientY - rect.top) / (rect.height / mh));
        return { mx, my };
    }

    function findWidgetAt(mx, my, widgets) {
        for (let i = widgets.length - 1; i >= 0; i--) {
            const w = widgets[i];
            const ww = Math.max(1, widgetPixelWidth(w));
            const wh = Math.max(1, widgetPixelHeight(w));
            if (mx >= w.x && mx < w.x + ww && my >= w.y && my < w.y + wh) return w;
        }
        return null;
    }

    if (widgetOverlayCanvas) {
        widgetOverlayCanvas.addEventListener('mousedown', (e) => {
            const { mx, my } = overlayCellFromEvent(e.clientX, e.clientY);
            const hit = findWidgetAt(mx, my, currentWidgets());
            if (!hit) return;
            draggingWidget = hit;
            dragStart = { mx, my, ox: hit.x, oy: hit.y };
        });
        window.addEventListener('mousemove', (e) => {
            if (!draggingWidget) return;
            const { mx, my } = overlayCellFromEvent(e.clientX, e.clientY);
            const { w: mw, h: mh } = editorDims();
            draggingWidget.x = Math.max(0, Math.min(mw - 1, dragStart.ox + (mx - dragStart.mx)));
            draggingWidget.y = Math.max(0, Math.min(mh - 1, dragStart.oy + (my - dragStart.my)));
            const posLabel = document.getElementById('widgetPos_' + draggingWidget.id);
            if (posLabel) posLabel.innerText = `X:${draggingWidget.x} Y:${draggingWidget.y}`;
            drawWidgetOverlay(currentWidgets());
        });
        window.addEventListener('mouseup', () => {
            if (!draggingWidget) return;
            const widgets = currentWidgets();
            draggingWidget = null;
            saveWidgets(widgets);
        });
        widgetOverlayCanvas.addEventListener('touchstart', (e) => {
            const t0 = e.touches[0];
            const { mx, my } = overlayCellFromEvent(t0.clientX, t0.clientY);
            const hit = findWidgetAt(mx, my, currentWidgets());
            if (!hit) return;
            draggingWidget = hit;
            dragStart = { mx, my, ox: hit.x, oy: hit.y };
            e.preventDefault();
        }, { passive: false });
        widgetOverlayCanvas.addEventListener('touchmove', (e) => {
            if (!draggingWidget) return;
            const t0 = e.touches[0];
            const { mx, my } = overlayCellFromEvent(t0.clientX, t0.clientY);
            const { w: mw, h: mh } = editorDims();
            draggingWidget.x = Math.max(0, Math.min(mw - 1, dragStart.ox + (mx - dragStart.mx)));
            draggingWidget.y = Math.max(0, Math.min(mh - 1, dragStart.oy + (my - dragStart.my)));
            drawWidgetOverlay(currentWidgets());
            e.preventDefault();
        }, { passive: false });
        widgetOverlayCanvas.addEventListener('touchend', () => {
            if (!draggingWidget) return;
            const widgets = currentWidgets();
            draggingWidget = null;
            saveWidgets(widgets);
        });
    }

    if (btnAddTextWidget) {
        btnAddTextWidget.addEventListener('click', () => {
            const widgets = currentWidgets();
            if (widgets.length >= 12) return;
            widgets.push({ id: 0, type: 0, x: 0, y: 0, color: 0xFFFFFF, text: '', w: 0, h: 0, scale: 1, format: 0, font: 0 });
            saveWidgets(widgets);
        });
    }

    // --- Weather location (used by the "Wetter" widget) ---
    const weatherCityInput = document.getElementById('weatherCityInput');
    const btnSaveWeatherLocation = document.getElementById('btnSaveWeatherLocation');
    const weatherStatus = document.getElementById('weatherStatus');
    const WEATHER_ICON_NAMES = ['weather_icon_sun', 'weather_icon_cloud', 'weather_icon_rain', 'weather_icon_snow', 'weather_icon_thunder'];
    const WEATHER_ICON_FALLBACK = ['Sonne', 'Bewölkt', 'Regen', 'Schnee', 'Gewitter'];

    function weatherIconLabel(icon) {
        const key = WEATHER_ICON_NAMES[icon] || WEATHER_ICON_NAMES[1];
        const translated = typeof t === 'function' ? t(key) : key;
        return translated === key ? (WEATHER_ICON_FALLBACK[icon] || WEATHER_ICON_FALLBACK[1]) : translated;
    }

    function fetchWeatherStatus() {
        if (!weatherStatus) return;
        fetch('/api/weather_status')
            .then(res => res.json())
            .then(data => {
                if (weatherCityInput && document.activeElement !== weatherCityInput) {
                    weatherCityInput.value = data.city || '';
                }
                if (!data.hasLocation) {
                    weatherStatus.innerText = t('weather_status_none') || 'Kein Standort gesetzt.';
                } else if (!data.hasData) {
                    weatherStatus.innerText = (t('weather_status_pending') || 'Standort gesetzt, warte auf erste Wetterdaten...').replace('{city}', data.city);
                } else {
                    const tmpl = t('weather_status_ok') || '{city}: {temp}°C, {icon}';
                    weatherStatus.innerText = tmpl
                        .replace('{city}', data.city)
                        .replace('{temp}', Math.round(data.temperature))
                        .replace('{icon}', weatherIconLabel(data.icon));
                }
            })
            .catch(() => {});
    }

    if (btnSaveWeatherLocation) {
        btnSaveWeatherLocation.addEventListener('click', () => {
            const city = (weatherCityInput.value || '').trim();
            if (!city) return;
            btnSaveWeatherLocation.disabled = true;
            weatherStatus.innerText = t('weather_status_saving') || 'Suche Standort...';
            fetch('/api/weather_location', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ city })
            }).then((res) => {
                if (!res.ok) throw new Error('not found');
                setTimeout(fetchWeatherStatus, 500);
            }).catch(() => {
                weatherStatus.innerText = t('weather_status_notfound') || 'Standort nicht gefunden.';
            }).finally(() => {
                btnSaveWeatherLocation.disabled = false;
            });
        });
    }

    // --- Pixel Art Editor ---
    const editorCanvas = document.getElementById('editorCanvas');
    const editorColor = document.getElementById('editorColor');
    const btnEditorClear = document.getElementById('btnEditorClear');
    const btnEditorLoad = document.getElementById('btnEditorLoad');
    const btnEditorStream = document.getElementById('btnEditorStream');
    let editorPixels = [];
    let editorCellSize = 16;
    let editorPainting = false;

    function initEditorCanvas() {
        if (!editorCanvas) return;
        const { w, h } = editorDims();
        editorCellSize = Math.max(4, Math.min(24, Math.floor(320 / Math.max(w, h))));
        editorCanvas.width = w * editorCellSize;
        editorCanvas.height = h * editorCellSize;
        if (editorPixels.length !== w * h) {
            editorPixels = new Array(w * h).fill('#000000');
        }
        redrawEditor();
    }

    function redrawEditor() {
        if (!editorCanvas) return;
        const { w, h } = editorDims();
        const ctx = editorCanvas.getContext('2d');
        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                ctx.fillStyle = editorPixels[y * w + x] || '#000000';
                ctx.fillRect(x * editorCellSize, y * editorCellSize, editorCellSize, editorCellSize);
            }
        }
    }

    function editorPaintAt(clientX, clientY) {
        if (!editorCanvas) return;
        const { w, h } = editorDims();
        const rect = editorCanvas.getBoundingClientRect();
        const x = Math.floor((clientX - rect.left) / (rect.width / w));
        const y = Math.floor((clientY - rect.top) / (rect.height / h));
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        editorPixels[y * w + x] = editorColor.value;
        const ctx = editorCanvas.getContext('2d');
        ctx.fillStyle = editorColor.value;
        ctx.fillRect(x * editorCellSize, y * editorCellSize, editorCellSize, editorCellSize);
    }

    if (editorCanvas) {
        editorCanvas.addEventListener('mousedown', (e) => {
            editorPainting = true;
            editorPaintAt(e.clientX, e.clientY);
        });
        editorCanvas.addEventListener('mousemove', (e) => {
            if (editorPainting) editorPaintAt(e.clientX, e.clientY);
        });
        window.addEventListener('mouseup', () => { editorPainting = false; });
        editorCanvas.addEventListener('touchstart', (e) => {
            editorPainting = true;
            editorPaintAt(e.touches[0].clientX, e.touches[0].clientY);
            e.preventDefault();
        }, { passive: false });
        editorCanvas.addEventListener('touchmove', (e) => {
            if (editorPainting) editorPaintAt(e.touches[0].clientX, e.touches[0].clientY);
            e.preventDefault();
        }, { passive: false });
        editorCanvas.addEventListener('touchend', () => { editorPainting = false; });
    }

    if (btnEditorClear) {
        btnEditorClear.addEventListener('click', () => {
            const { w, h } = editorDims();
            editorPixels = new Array(w * h).fill('#000000');
            redrawEditor();
        });
    }

    if (btnEditorLoad) {
        btnEditorLoad.addEventListener('click', () => {
            fetch('/api/matrix_preview')
                .then(res => res.json())
                .then(colors => {
                    const { w, h } = editorDims();
                    if (!Array.isArray(colors) || colors.length !== w * h) return;
                    editorPixels = colors.map(c => '#' + c.toString(16).padStart(6, '0'));
                    redrawEditor();
                })
                .catch(() => {});
        });
    }

    if (btnEditorStream) {
        btnEditorStream.addEventListener('click', () => {
            btnEditorStream.disabled = true;
            const original = btnEditorStream.innerText;
            btnEditorStream.innerText = 'Sende...';
            const { w, h } = editorDims();
            const pixelArray = editorPixels.map(hex => parseInt(hex.replace('#', ''), 16));
            streamToBackgroundWidget(pixelArray, w, h).then(() => {
                btnEditorStream.innerText = 'Übernommen!';
                setTimeout(() => { btnEditorStream.innerText = original; btnEditorStream.disabled = false; }, 2000);
            }).catch(() => {
                btnEditorStream.innerText = 'Fehler!';
                setTimeout(() => { btnEditorStream.innerText = original; btnEditorStream.disabled = false; }, 2000);
            });
        });
    }

    // --- Shared "background" image widget helper for the merged Pixel Art
    // Converter/Editor: both feed a full-canvas type=3 widget in the same
    // "Uhr / Text" widget list (identified as the one sitting at (0,0)),
    // instead of the old standalone /api/matrix flow, so uploading or painting
    // a background is just another element next to the clock/date/text ones.
    function getBackgroundWidget() {
        const { w: mw, h: mh } = editorDims();
        // Full-canvas sized image at the origin = "the background", as opposed to
        // a small image widget a user deliberately placed in a corner.
        return currentWidgets().find(w => w.type === 3 && w.x === 0 && w.y === 0 && w.w === mw && w.h === mh) || null;
    }

    function streamToBackgroundWidget(pixelArray, w, h) {
        if (w > 64 || h > 64) {
            return Promise.reject(new Error('Hintergrundbilder sind auf 64x64 Pixel begrenzt - deine Matrix ist größer.'));
        }
        const uploadInto = (widgetId) => fetch('/api/text_widget_image', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ seg: currentSegmentId, widget: widgetId, w, h, pixels: pixelArray })
        }).then((res) => {
            if (!res.ok) throw new Error('Upload failed: ' + res.status);
        }).then(() => fetch('/api/segments')).then(res => res.json()).then(data => {
            if (Array.isArray(data)) segments = data;
            renderTextWidgetEditor();
        });

        const existing = getBackgroundWidget();
        if (existing) return uploadInto(existing.id);

        const widgets = currentWidgets();
        widgets.push({ id: 0, type: 3, x: 0, y: 0, color: 0xFFFFFF, text: '', w, h });
        return saveWidgets(widgets).then(() => {
            const created = getBackgroundWidget();
            if (!created) throw new Error('Background widget was not created');
            return uploadInto(created.id);
        });
    }

});







