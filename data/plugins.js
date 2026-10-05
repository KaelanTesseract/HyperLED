/*
 * HyperLED - Open Source LED Controller
 *
 * Copyright (c) 2026 Dennis Guse
 *
 * Licensed under the EUPL, Version 1.2 or – as soon they will be approved by
 * the European Commission - subsequent versions of the EUPL (the "Licence");
 * You may not use this work except in compliance with the Licence.
 * You may obtain a copy of the Licence at:
 *
 * https://joinup.ec.europa.eu/software/page/eupl
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 */

// The plugin pages: the list under Einstellungen > Plugins, the dialogs for installing, setting up
// and watching a plugin, and the "controlled by" badges on the light page.
//
// Everything a plugin delivers - its name, description, labels, values, the answer of its source, its
// messages - is untrusted text. It is only ever put into the page with textContent (through h()),
// never as HTML.
(function () {
    'use strict';

    // app.js hands over its helpers (window.HyperUI) in its own DOMContentLoaded handler, which was
    // registered first and so runs first: this file starts from the same event, not before it.
    function start() {
        const UI = window.HyperUI || {};
        const toast = (message, kind) => (UI.showToast ? UI.showToast(message, kind) : console.log(message));
        const ask = (options) => (UI.askConfirm ? UI.askConfirm(options) : Promise.resolve(window.confirm(options.body || options.title || '')));
        const tr = (key, vars) => (typeof t === 'function' ? t(key, vars) : key);
        const MAX_FILE = 16384;

        // --- DOM helper ---------------------------------------------------------------------------
        // h('div', { class: 'x', text: 'plain text', onclick: fn }, [children]). `text` is textContent.
        function h(tag, attrs, children) {
            const el = document.createElement(tag);
            const a = attrs || {};
            Object.keys(a).forEach((name) => {
                const value = a[name];
                if (value === undefined || value === null || value === false) return;
                if (name === 'text') el.textContent = value;
                else if (name === 'class') el.className = value;
                else if (name.indexOf('on') === 0 && typeof value === 'function') el.addEventListener(name.slice(2), value);
                else if (name === 'value' || name === 'checked' || name === 'disabled' || name === 'hidden') el[name] = value;
                else el.setAttribute(name, value === true ? '' : value);
            });
            (children || []).forEach((child) => {
                if (child === null || child === undefined || child === false) return;
                el.appendChild(typeof child === 'string' ? document.createTextNode(child) : child);
            });
            return el;
        }

        // A text a plugin gives per language ({de, en, ru}, or a plain string): the one for the language
        // of the page, falling back to German and then to whatever there is.
        function pick(text, fallback) {
            if (!text) return fallback || '';
            if (typeof text === 'string') return text;
            const lang = typeof currentLang === 'string' ? currentLang : 'de';
            return text[lang] || text.de || text.en || text.ru || fallback || '';
        }

        async function api(method, path, body, raw) {
            const options = { method, headers: {} };
            if (body !== undefined) {
                options.headers['Content-Type'] = 'application/json';
                options.body = raw ? body : JSON.stringify(body);
            }
            const res = await fetch(path, options);
            let data = null;
            try { data = await res.json(); } catch (e) { data = null; }
            if (!res.ok) {
                const err = new Error((data && data.error) || tr('plugin_err_generic'));
                err.status = res.status;
                throw err;
            }
            return data;
        }

        // --- Dialogs ------------------------------------------------------------------------------
        let openDialogRecord = null;

        function openDialog(title, options) {
            const o = options || {};
            if (openDialogRecord) openDialogRecord.close();
            const opener = document.activeElement;
            const titleId = 'pluginDialogTitle';
            const titleEl = h('h3', { id: titleId, text: title });
            const body = h('div', { class: 'plugin-dialog-body' });
            const footer = h('div', { class: 'confirm-actions plugin-dialog-actions' });
            const card = h('div', { class: 'confirm-card plugin-dialog', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': titleId }, [titleEl, body, footer]);
            const root = h('div', { class: 'modal plugin-modal' }, [card]);
            document.body.appendChild(root);
            setTimeout(() => root.classList.add('show'), 0);  // (not requestAnimationFrame: it stands still in a hidden tab)

            const record = { root, body, footer, closed: false };
            record.setTitle = (text) => { titleEl.textContent = text; };
            record.close = () => {
                if (record.closed) return;
                record.closed = true;
                document.removeEventListener('keydown', onKey, true);
                root.classList.remove('show');
                setTimeout(() => root.remove(), 300);
                if (openDialogRecord === record) openDialogRecord = null;
                if (typeof o.onClose === 'function') o.onClose();
                if (opener && typeof opener.focus === 'function' && document.contains(opener)) opener.focus({ preventScroll: true });
            };

            const confirmOpen = () => {
                const c = document.getElementById('confirmModal');
                return !!(c && c.classList.contains('show'));
            };
            function onKey(e) {
                if (confirmOpen()) return;  // the question on top has the keyboard
                if (e.key === 'Escape') {
                    e.stopPropagation();
                    record.close();
                    return;
                }
                if (e.key !== 'Tab') return;
                // Keep the focus inside the dialog.
                const focusable = Array.from(card.querySelectorAll('button, input, select, textarea, a[href]'))
                    .filter((el) => !el.disabled && el.offsetParent !== null);
                if (!focusable.length) return;
                const first = focusable[0];
                const last = focusable[focusable.length - 1];
                if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
                else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
            }
            document.addEventListener('keydown', onKey, true);
            root.addEventListener('click', (e) => { if (e.target === root) record.close(); });
            openDialogRecord = record;
            return record;
        }

        function button(label, cls, onclick) {
            return h('button', { type: 'button', class: 'btn ' + cls, text: label, onclick });
        }

        function errorBox(message) {
            return h('div', { class: 'plugin-note plugin-note-error', role: 'alert', text: message });
        }

        // --- The list -----------------------------------------------------------------------------
        let plugins = [];
        let maxPlugins = 8;
        let lastListSignature = '';
        const listEl = document.getElementById('pluginList');
        const tabEl = document.getElementById('tab-plugins');

        function stateLabel(p) {
            return tr('plugin_state_' + p.state);
        }

        function scriptLine(p) {
            if (!p.script) return null;
            const s = p.script;
            if (s.mode === 'script' && typeof s.frame_ms === 'number') return tr('plugin_script_running', { ms: s.frame_ms.toFixed(1) });
            if (s.mode === 'script') return tr('plugin_script_starting');
            if (p.state === 'running' || p.state === 'no_connection') {
                return s.note ? tr('plugin_script_rules', { note: s.note }) : tr('plugin_script_rules_plain');
            }
            return tr('plugin_script_has');
        }

        async function setEnabled(id, enabled) {
            await api('POST', '/api/plugins/enable', { id, enabled });
        }

        function pluginCard(p) {
            const card = h('div', { class: 'device-card plugin-card', 'data-plugin': p.id });
            const known = p.state !== 'invalid';
            const name = known ? (p.name || p.id) : p.id;

            const title = h('div', { class: 'plugin-title' }, [
                h('strong', { class: 'plugin-name', text: name }),
                known && p.version ? h('span', { class: 'badge', text: 'v' + p.version }) : null,
            ]);
            const head = h('div', { class: 'plugin-head' }, [title]);
            if (known) {
                const input = h('input', { type: 'checkbox', class: 'toggle-chip-input', checked: !!p.enabled, 'aria-label': tr('plugin_switch_aria', { name }) });
                const toggle = h('label', { class: 'toggle-chip' }, [input, h('span', { class: 'toggle-chip-mark', 'aria-hidden': 'true', text: '✓' }), h('span', { text: tr('plugin_on') })]);
                input.addEventListener('change', async () => {
                    input.disabled = true;
                    try {
                        await setEnabled(p.id, input.checked);
                    } catch (e) {
                        input.checked = !input.checked;
                        toast(e.message, 'error');
                    }
                    input.disabled = false;
                    load();
                });
                head.appendChild(toggle);
            }
            card.appendChild(head);

            if (known) {
                const by = [];
                if (p.author) by.push(tr('plugin_by', { author: p.author }));
                if (p.license) by.push(tr('plugin_license', { license: p.license }));
                if (by.length) card.appendChild(h('div', { class: 'plugin-meta', text: by.join(' · ') }));
                if (p.description) card.appendChild(h('p', { class: 'plugin-desc', text: p.description }));
            }

            const status = h('div', { class: 'plugin-status' }, [h('span', { class: 'plugin-state plugin-state-' + p.state, text: stateLabel(p) })]);
            if (p.reason) status.appendChild(h('span', { class: 'plugin-reason', text: p.reason }));
            card.appendChild(status);
            if (p.warning) card.appendChild(h('div', { class: 'plugin-note plugin-note-warn', text: p.warning }));
            const script = scriptLine(p);
            if (script) card.appendChild(h('div', { class: 'plugin-script', text: script }));

            const actions = h('div', { class: 'plugin-actions' });
            if (known) {
                actions.appendChild(button(tr('plugin_btn_settings'), 'btn-secondary btn-chip', () => openSettings(p.id)));
                actions.appendChild(button(tr('plugin_btn_values'), 'btn-secondary btn-chip', () => openValues(p.id)));
            }
            actions.appendChild(button(tr('plugin_btn_remove'), 'btn-secondary btn-chip plugin-remove', async () => {
                const ok = await ask({
                    title: tr('plugin_remove_title'),
                    body: tr('plugin_remove_body', { name }),
                    ok: tr('plugin_remove_ok'),
                    destructive: true,
                });
                if (!ok) return;
                try {
                    await api('POST', '/api/plugins/remove', { id: p.id });
                    toast(tr('plugin_removed', { name }), 'success');
                } catch (e) {
                    toast(e.message, 'error');
                }
                load();
            }));
            card.appendChild(actions);
            return card;
        }

        function renderList() {
            if (!listEl) return;
            listEl.replaceChildren();
            if (!plugins.length) {
                listEl.appendChild(h('div', { class: 'plugin-empty' }, [
                    h('strong', { text: tr('plugin_empty_title') }),
                    h('p', { text: tr('plugin_empty_desc') }),
                ]));
                return;
            }
            plugins.forEach((p) => listEl.appendChild(pluginCard(p)));
            listEl.appendChild(h('div', { class: 'plugin-count', text: tr('plugin_count', { n: plugins.length, max: maxPlugins }) }));
        }

        async function load(force) {
            if (!listEl) return;
            try {
                const data = await api('GET', '/api/plugins');
                maxPlugins = data.max || maxPlugins;
                const signature = JSON.stringify(data.plugins) + (typeof currentLang === 'string' ? currentLang : '');
                plugins = data.plugins || [];
                if (force || signature !== lastListSignature) {
                    lastListSignature = signature;
                    renderList();
                }
            } catch (e) {
                if (!listEl.children.length) listEl.appendChild(errorBox(e.message));
            }
        }

        // --- Installing ---------------------------------------------------------------------------
        function openInstall() {
            const dlg = openDialog(tr('plugin_install_title'));
            showSources(dlg);
        }

        function showSources(dlg, message) {
            dlg.setTitle(tr('plugin_install_title'));
            const file = h('input', { type: 'file', accept: '.json,application/json', id: 'pluginFile', class: 'plugin-file' });
            const url = h('input', { type: 'url', id: 'pluginUrl', class: 'field', placeholder: tr('plugin_install_url_placeholder'), autocomplete: 'off', inputmode: 'url' });
            const loadBtn = button(tr('plugin_install_url_btn'), 'btn-secondary', () => fetchFromUrl(dlg, url, loadBtn));
            const error = h('div', { class: 'plugin-error-slot' });
            if (message) error.appendChild(errorBox(message));

            file.addEventListener('change', () => {
                const chosen = file.files && file.files[0];
                if (!chosen) return;
                if (chosen.size > MAX_FILE) {
                    error.replaceChildren(errorBox(tr('plugin_file_too_big')));
                    return;
                }
                const reader = new FileReader();
                reader.onload = () => preview(dlg, String(reader.result));
                reader.onerror = () => error.replaceChildren(errorBox(tr('plugin_err_generic')));
                reader.readAsText(chosen);
            });
            url.addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); loadBtn.click(); } });

            dlg.body.replaceChildren(
                h('p', { class: 'plugin-dialog-lead', text: tr('plugin_install_lead') }),
                h('div', { class: 'form-group' }, [h('label', { for: 'pluginFile', text: tr('plugin_install_file') }), file]),
                h('div', { class: 'form-group' }, [
                    h('label', { for: 'pluginUrl', text: tr('plugin_install_url_label') }),
                    h('div', { class: 'plugin-url-row' }, [url, loadBtn]),
                ]),
                error
            );
            dlg.footer.replaceChildren(button(tr('plugin_close'), 'btn-secondary', () => dlg.close()));
            setTimeout(() => file.focus({ preventScroll: true }), 30);
        }

        async function fetchFromUrl(dlg, input, loadBtn) {
            const address = input.value.trim();
            const slot = dlg.body.querySelector('.plugin-error-slot');
            slot.replaceChildren();
            if (!address) {
                slot.appendChild(errorBox(tr('plugin_url_empty')));
                return;
            }
            loadBtn.disabled = true;
            input.disabled = true;
            slot.appendChild(h('div', { class: 'plugin-note', role: 'status', text: tr('plugin_loading') }));
            try {
                await api('POST', '/api/plugins/fetch', { url: address });
                for (let i = 0; i < 60; i++) {
                    await new Promise((r) => setTimeout(r, 500));
                    if (dlg.closed) return;
                    const result = await api('GET', '/api/plugins/fetch_result');
                    if (result.state === 'done') {
                        preview(dlg, result.text);
                        return;
                    }
                    if (result.state === 'error') throw new Error(result.error);
                    if (result.state === 'idle') break;
                }
                throw new Error(tr('plugin_url_timeout'));
            } catch (e) {
                if (dlg.closed) return;
                slot.replaceChildren(errorBox(e.message));
                loadBtn.disabled = false;
                input.disabled = false;
            }
        }

        async function preview(dlg, text) {
            let pv;
            try {
                pv = await api('POST', '/api/plugins/preview', text, true);
            } catch (e) {
                showSources(dlg, e.message);
                return;
            }
            dlg.setTitle(tr('plugin_preview_title'));
            const rows = [];
            rows.push(h('div', { class: 'plugin-preview-head' }, [
                h('strong', { class: 'plugin-name', text: pv.name }),
                h('span', { class: 'badge', text: 'v' + pv.version }),
            ]));
            const by = [];
            if (pv.author) by.push(tr('plugin_by', { author: pv.author }));
            if (pv.license) by.push(tr('plugin_license', { license: pv.license }));
            if (by.length) rows.push(h('div', { class: 'plugin-meta', text: by.join(' · ') }));
            if (pv.description) rows.push(h('p', { class: 'plugin-desc', text: pv.description }));

            // What the person is agreeing to: where the plugin looks, and what it may do.
            rows.push(h('div', { class: 'plugin-preview-block' }, [
                h('div', { class: 'plugin-preview-label', text: tr('plugin_preview_source') }),
                h('code', { class: 'plugin-address', text: pv.source_url }),
                h('div', { class: 'plugin-hint', text: tr('plugin_preview_source_hint') }),
            ]));
            const can = [];
            if (pv.segment_setting) can.push(tr('plugin_preview_segment'));
            if (pv.wants_power) can.push(tr('plugin_preview_power'));
            if (pv.has_script) can.push(tr('plugin_preview_script'));
            if (can.length) rows.push(h('ul', { class: 'plugin-can' }, can.map((line) => h('li', { text: line }))));
            if (pv.replaces) rows.push(h('div', { class: 'plugin-note', text: tr('plugin_preview_replaces', { version: pv.installed_version || '?' }) }));
            if (!pv.compatible) rows.push(h('div', { class: 'plugin-note plugin-note-warn', text: tr('plugin_preview_incompat', { note: pv.compat_note }) }));
            if (pv.limit_reached) rows.push(errorBox(tr('plugin_preview_full', { max: maxPlugins })));
            const error = h('div', { class: 'plugin-error-slot' });
            rows.push(error);
            dlg.body.replaceChildren(...rows);

            const install = button(tr('plugin_install_btn'), 'btn-primary', async () => {
                install.disabled = true;
                error.replaceChildren();
                try {
                    const result = await api('POST', '/api/plugins/install', text, true);
                    toast(tr('plugin_installed', { name: pv.name }), 'success');
                    dlg.close();
                    await load(true);
                    openSettings(result.id);
                } catch (e) {
                    error.replaceChildren(errorBox(e.message));
                    install.disabled = false;
                }
            });
            install.disabled = !!pv.limit_reached;
            dlg.footer.replaceChildren(button(tr('plugin_cancel'), 'btn-secondary', () => dlg.close()), install);
            setTimeout(() => install.focus({ preventScroll: true }), 30);
        }

        // --- Settings -----------------------------------------------------------------------------
        // One field per setting, built from the plugin's definition. Every kind gives back its value as
        // text (a switch as true/false), the way the device keeps it.
        function buildField(def, current, segments) {
            const label = pick(def.label, def.key);
            const hint = pick(def.hint, '');
            const id = 'plugin_set_' + def.key;
            const wrap = h('div', { class: 'form-group plugin-field', 'data-key': def.key });
            const errorEl = h('div', { class: 'plugin-field-error', role: 'alert', hidden: true });
            let input;
            let read;
            let initial = current;

            const labelEl = h('label', { for: id }, [label, def.optional || def.type === 'switch' ? null : h('span', { class: 'plugin-required', 'aria-hidden': 'true', text: ' *' })]);

            if (def.type === 'switch') {
                input = h('input', { type: 'checkbox', id, class: 'toggle-chip-input', checked: current === 'true' });
                const chip = h('label', { class: 'toggle-chip' }, [input, h('span', { class: 'toggle-chip-mark', 'aria-hidden': 'true', text: '✓' }), h('span', { text: label })]);
                wrap.appendChild(chip);
                read = () => (input.checked ? 'true' : 'false');
            } else if (def.type === 'list' || def.type === 'effect' || def.type === 'segment') {
                input = h('select', { id });
                const options = [];
                if (def.type === 'list') {
                    (def.options || []).forEach((o) => options.push({ value: o.value, label: pick(o.label, o.value) }));
                } else if (def.type === 'effect') {
                    (def.effects || []).forEach((name) => options.push({ value: name, label: name }));
                } else {
                    segments.forEach((s, index) => options.push({ value: String(index), label: (index + 1) + ': ' + (UI.translateSegmentName ? UI.translateSegmentName(s.name) : s.name) }));
                }
                if (current === '' || !options.some((o) => o.value === current)) {
                    input.appendChild(h('option', { value: '', text: current && def.type !== 'segment' ? current : tr('plugin_pick') }));
                }
                options.forEach((o) => input.appendChild(h('option', { value: o.value, text: o.label })));
                input.value = current;
                wrap.append(labelEl, input);
                read = () => input.value;
            } else if (def.type === 'color') {
                const text = h('input', { type: 'text', id, value: current, maxlength: '7', autocomplete: 'off', class: 'plugin-color-text' });
                const picker = h('input', { type: 'color', class: 'plugin-color-picker', 'aria-label': label, value: /^#[0-9a-fA-F]{6}$/.test(current) ? current : '#00ff00' });
                picker.addEventListener('input', () => { text.value = picker.value; });
                text.addEventListener('input', () => { if (/^#[0-9a-fA-F]{6}$/.test(text.value)) picker.value = text.value; });
                input = text;
                wrap.append(labelEl, h('div', { class: 'plugin-color-row' }, [picker, text]));
                read = () => text.value.trim();
            } else if (def.type === 'number') {
                input = h('input', { type: 'number', id, value: current, step: 'any', inputmode: 'decimal' });
                if (typeof def.min === 'number') input.min = def.min;
                if (typeof def.max === 'number') input.max = def.max;
                wrap.append(labelEl, input);
                read = () => input.value.trim();
            } else {
                const isPassword = def.type === 'password';
                input = h('input', { type: isPassword ? 'password' : 'text', id, value: isPassword ? '' : current, maxlength: '120', autocomplete: isPassword ? 'new-password' : 'off', spellcheck: 'false' });
                if (isPassword && current) input.placeholder = tr('plugin_password_kept');
                wrap.append(labelEl, input);
                read = () => input.value;
                if (isPassword) initial = '';
            }
            if (hint) {
                const hintEl = h('div', { class: 'plugin-hint', id: id + '_hint', text: hint });
                wrap.appendChild(hintEl);
                input.setAttribute('aria-describedby', id + '_hint');
            }
            wrap.appendChild(errorEl);
            const showError = (message) => {
                errorEl.textContent = message || '';
                errorEl.hidden = !message;
                if (message) input.setAttribute('aria-invalid', 'true');
                else input.removeAttribute('aria-invalid');
            };
            return { key: def.key, type: def.type, element: wrap, input, read, initial, showError };
        }

        async function openSettings(id) {
            const dlg = openDialog(tr('plugin_settings_loading'));
            dlg.body.appendChild(h('p', { class: 'plugin-hint', text: tr('plugin_loading') }));
            dlg.footer.replaceChildren(button(tr('plugin_close'), 'btn-secondary', () => dlg.close()));
            let def, list, state;
            try {
                [def, list, state] = await Promise.all([
                    api('GET', '/api/plugins/definition?id=' + encodeURIComponent(id)),
                    api('GET', '/api/plugins'),
                    api('GET', '/api/state'),
                ]);
            } catch (e) {
                dlg.body.replaceChildren(errorBox(e.message));
                return;
            }
            const p = (list.plugins || []).find((x) => x.id === id);
            if (!p || !def.valid) {
                dlg.body.replaceChildren(errorBox(p ? p.reason : tr('plugin_err_generic')));
                return;
            }
            dlg.setTitle(tr('plugin_settings_title', { name: p.name || id }));
            const segments = state.seg || [];
            const fields = (def.settings || []).map((s) => {
                // The definition has the effects a plugin may use only once, at the top level.
                const entry = Object.assign({}, s, { effects: def.effects });
                return buildField(entry, (p.settings && p.settings[s.key] !== undefined) ? String(p.settings[s.key]) : '', segments);
            });

            const top = h('div', { class: 'plugin-error-slot' });
            const body = [];

            // Switched on or off, from the page that is about it.
            const enableInput = h('input', { type: 'checkbox', class: 'toggle-chip-input', checked: !!p.enabled });
            const enableChip = h('label', { class: 'toggle-chip' }, [enableInput, h('span', { class: 'toggle-chip-mark', 'aria-hidden': 'true', text: '✓' }), h('span', { text: tr('plugin_enabled_label') })]);
            const stateBadge = h('span', { class: 'plugin-state plugin-state-' + p.state, text: stateLabel(p) });
            const reasonEl = h('div', { class: 'plugin-reason', text: p.reason || '', hidden: !p.reason });
            body.push(h('div', { class: 'plugin-enable-row' }, [enableChip, stateBadge]));
            body.push(reasonEl);
            body.push(top);

            fields.forEach((f) => body.push(f.element));

            // Options that need a deliberate yes.
            const options = h('div', { class: 'plugin-options' });
            if (p.state === 'incompatible' || p.force) {
                const forceInput = h('input', { type: 'checkbox', class: 'toggle-chip-input', checked: !!p.force });
                forceInput.addEventListener('change', async () => {
                    if (forceInput.checked) {
                        const ok = await ask({ title: tr('plugin_force_title'), body: tr('plugin_force_body'), ok: tr('plugin_force_ok'), destructive: true });
                        if (!ok) { forceInput.checked = false; return; }
                    }
                    try {
                        await api('POST', '/api/plugins/options', { id, force: forceInput.checked });
                        refreshHeader();
                        load();
                    } catch (e) {
                        forceInput.checked = !forceInput.checked;
                        top.replaceChildren(errorBox(e.message));
                    }
                });
                options.appendChild(h('label', { class: 'toggle-chip' }, [forceInput, h('span', { class: 'toggle-chip-mark', 'aria-hidden': 'true', text: '✓' }), h('span', { text: tr('plugin_opt_force') })]));
            }
            if (def.wants_power) {
                const powerInput = h('input', { type: 'checkbox', class: 'toggle-chip-input', checked: !!p.allow_power });
                powerInput.addEventListener('change', async () => {
                    try {
                        await api('POST', '/api/plugins/options', { id, allow_power: powerInput.checked });
                        refreshHeader();
                        load();
                    } catch (e) {
                        powerInput.checked = !powerInput.checked;
                        top.replaceChildren(errorBox(e.message));
                    }
                });
                options.appendChild(h('label', { class: 'toggle-chip' }, [powerInput, h('span', { class: 'toggle-chip-mark', 'aria-hidden': 'true', text: '✓' }), h('span', { text: tr('plugin_opt_power') })]));
                options.appendChild(h('div', { class: 'plugin-hint', text: tr('plugin_opt_power_hint') }));
            }
            if (options.children.length) body.push(options);
            dlg.body.replaceChildren(...body);

            // Only what changed goes to the device; a password only when something was typed.
            function changes() {
                const values = {};
                fields.forEach((f) => {
                    const now = f.read();
                    if (f.type === 'password') {
                        if (now !== '') values[f.key] = now;
                    } else if (now !== f.initial) {
                        values[f.key] = f.type === 'switch' ? now === 'true' : now;
                    }
                });
                return values;
            }

            function showFieldError(message) {
                // The device names the setting in its message: 'Einstellung 'port': ...'.
                const match = /'([a-z0-9_]+)'/.exec(message);
                const field = match ? fields.find((f) => f.key === match[1]) : null;
                fields.forEach((f) => f.showError(''));
                top.replaceChildren();
                if (field) {
                    field.showError(message);
                    field.input.focus({ preventScroll: false });
                } else {
                    top.appendChild(errorBox(message));
                }
            }

            async function save() {
                const values = changes();
                if (!Object.keys(values).length) return true;
                try {
                    await api('POST', '/api/plugins/settings', { id, values });
                } catch (e) {
                    showFieldError(e.message);
                    return false;
                }
                fields.forEach((f) => {
                    f.showError('');
                    if (f.type !== 'password') {
                        f.initial = f.read();
                    } else {
                        if (values[f.key]) f.input.placeholder = tr('plugin_password_kept');
                        f.input.value = '';
                    }
                });
                top.replaceChildren();
                return true;
            }

            // The state badge follows what the device says now (switched on, waiting, running ...).
            async function refreshHeader() {
                try {
                    const data = await api('GET', '/api/plugins');
                    const now = (data.plugins || []).find((x) => x.id === id);
                    if (!now) return;
                    stateBadge.className = 'plugin-state plugin-state-' + now.state;
                    stateBadge.textContent = stateLabel(now);
                    reasonEl.textContent = now.reason || '';
                    reasonEl.hidden = !now.reason;
                } catch (e) { /* the next refresh will do */ }
            }

            enableInput.addEventListener('change', async () => {
                const wanted = enableInput.checked;
                enableInput.disabled = true;
                try {
                    if (!(await save())) { enableInput.checked = !wanted; }
                    else {
                        try { await setEnabled(id, wanted); } catch (e) { enableInput.checked = !wanted; showFieldError(e.message); }
                    }
                } finally {
                    enableInput.disabled = false;
                }
                refreshHeader();
                load();
            });

            const saveBtn = button(tr('plugin_save'), 'btn-primary', async () => {
                saveBtn.disabled = true;
                const ok = await save();
                saveBtn.disabled = false;
                if (ok) {
                    top.replaceChildren();
                    fields.forEach((f) => f.showError(''));
                    toast(tr('plugin_saved'), 'success');
                    refreshHeader();
                    load();
                }
            });
            dlg.footer.replaceChildren(button(tr('plugin_close'), 'btn-secondary', () => dlg.close()), saveBtn);
        }

        // --- Live values --------------------------------------------------------------------------
        // A number comes over the air as a 32-bit float: 60 may arrive as 60.00000238. Two decimals are plenty.
        function formatValue(value) {
            if (typeof value === 'number') return String(Math.round(value * 100) / 100);
            return String(value);
        }

        function openValues(id) {
            let timer = null;
            const dlg = openDialog(tr('plugin_values_title'), { onClose: () => clearInterval(timer) });
            const content = h('div', { class: 'plugin-values' });
            dlg.body.appendChild(content);
            dlg.footer.replaceChildren(button(tr('plugin_close'), 'btn-secondary', () => dlg.close()));

            async function refresh() {
                let v;
                try {
                    v = await api('GET', '/api/plugins/values?id=' + encodeURIComponent(id));
                } catch (e) {
                    content.replaceChildren(errorBox(e.message));
                    return;
                }
                const nodes = [];
                const state = { name: v.state, label: tr('plugin_state_' + v.state) };
                nodes.push(h('div', { class: 'plugin-enable-row' }, [h('span', { class: 'plugin-state plugin-state-' + state.name, text: state.label })]));
                if (v.reason) nodes.push(h('div', { class: 'plugin-reason', text: v.reason }));

                const entries = Object.keys(v.values || {});
                nodes.push(h('div', { class: 'plugin-preview-label', text: tr('plugin_values_values') }));
                if (entries.length) {
                    nodes.push(h('table', { class: 'plugin-table' }, entries.map((name) => {
                        const value = v.values[name];
                        return h('tr', {}, [
                            h('th', { scope: 'row', text: name }),
                            h('td', { text: value === null ? tr('plugin_values_unknown') : formatValue(value) }),
                        ]);
                    })));
                } else {
                    nodes.push(h('div', { class: 'plugin-hint', text: tr('plugin_values_none') }));
                }

                // Who is drawing, and why.
                let who;
                if (v.source === 'script') who = tr('plugin_values_source_script');
                else if (v.source === 'rules' && v.rule) who = tr('plugin_values_source_rules', { n: v.rule.index, when: v.rule.when || '' });
                else if (v.source === 'on_error') who = tr('plugin_values_source_on_error');
                else who = tr('plugin_values_source_none');
                nodes.push(h('div', { class: 'plugin-preview-label', text: tr('plugin_values_source') }));
                nodes.push(h('div', { class: 'plugin-source-line', text: who }));
                if (v.script && v.script.note) nodes.push(h('div', { class: 'plugin-note plugin-note-warn', text: v.script.note }));
                if (v.script && v.script.message) nodes.push(h('div', { class: 'plugin-hint', text: v.script.message }));

                if (v.raw) {
                    nodes.push(h('div', { class: 'plugin-preview-label', text: tr('plugin_values_raw') }));
                    nodes.push(h('pre', { class: 'plugin-raw', text: v.raw }));
                }
                content.replaceChildren(...nodes);
            }
            refresh();
            timer = setInterval(refresh, 2000);
            // The title carries the plugin's name once it is known.
            const known = plugins.find((p) => p.id === id);
            dlg.setTitle(tr('plugin_values_title_named', { name: known && known.name ? known.name : id }));
        }

        // --- "Controlled by" on the light page ----------------------------------------------------
        const badgesEl = document.getElementById('pluginBadges');
        let lastBadgeSignature = '';

        async function refreshBadges() {
            if (!badgesEl || document.hidden) return;
            const area = badgesEl.closest('.area');
            if (area && !area.classList.contains('active')) return;
            let state;
            try {
                state = await api('GET', '/api/state');
            } catch (e) {
                return;
            }
            const controlled = [];
            (state.seg || []).forEach((s, index) => {
                if (s.plugin) controlled.push({ index, segment: s.name, id: s.plugin.id, name: s.plugin.name });
            });
            const signature = JSON.stringify(controlled) + (typeof currentLang === 'string' ? currentLang : '');
            if (signature === lastBadgeSignature) return;
            lastBadgeSignature = signature;
            badgesEl.replaceChildren();
            badgesEl.hidden = controlled.length === 0;
            if (!controlled.length) return;
            controlled.forEach((c) => {
                const segName = UI.translateSegmentName ? UI.translateSegmentName(c.segment) : c.segment;
                const pause = button(tr('plugin_pause'), 'btn-secondary btn-chip', async () => {
                    pause.disabled = true;
                    try {
                        await setEnabled(c.id, false);
                        toast(tr('plugin_paused', { name: c.name }), 'success');
                    } catch (e) {
                        toast(e.message, 'error');
                    }
                    lastBadgeSignature = '';
                    refreshBadges();
                    load();
                });
                badgesEl.appendChild(h('div', { class: 'plugin-badge' }, [
                    h('span', { class: 'plugin-badge-text', text: tr('plugin_controlled_by', { name: c.name, segment: segName }) }),
                    pause,
                ]));
            });
            badgesEl.appendChild(h('div', { class: 'plugin-hint', text: tr('plugin_light_hint') }));
        }

        // --- Wiring -------------------------------------------------------------------------------
        const addBtn = document.getElementById('btnPluginAdd');
        if (addBtn) addBtn.addEventListener('click', openInstall);
        const tabBtn = document.querySelector('.tab-btn[data-tab="tab-plugins"]');
        if (tabBtn) tabBtn.addEventListener('click', () => load(true));

        // Keep the list and the badges current while they are on screen - and only then.
        setInterval(() => {
            if (document.hidden || openDialogRecord) return;
            if (tabEl && tabEl.offsetParent !== null) load();
        }, 5000);
        setInterval(refreshBadges, 5000);
        refreshBadges();
        document.addEventListener('languageChanged', () => {
            lastBadgeSignature = '';
            refreshBadges();
            load(true);
        });
        document.querySelectorAll('.nav-btn').forEach((b) => b.addEventListener('click', () => setTimeout(refreshBadges, 200)));
        if (tabEl && tabEl.offsetParent !== null) load(true);

        // --- Plugin values for the placeholders of text elements --------------------------------------
        // {<plugin-id>.<value>} in a Text or Lauftext element shows a value of a plugin; the controller fills
        // it in. These two let the element editor offer the values and work out what the controller will show.
        let placeholderValues = {};  // "id.value" -> text, only of plugins that are running

        // The value as text the way the controller makes it (Value::toText, then control characters
        // become spaces and the text is cut to 64 bytes - a source is not trusted).
        function valueText(v) {
            let text;
            if (typeof v === 'number') text = (v === Math.floor(v) && Math.abs(v) < 1e9) ? String(v) : v.toFixed(2);
            else if (typeof v === 'boolean') text = v ? 'true' : 'false';
            else text = String(v);
            return text.replace(/[\u0000-\u001f\u007f]/g, ' ').slice(0, 64);
        }

        async function pluginValues() {
            let list = [];
            try {
                const data = await api('GET', '/api/plugins');
                list = (data && data.plugins) || [];
            } catch (e) {
                return [];
            }
            const fresh = {};
            const out = [];
            list.forEach((p) => {
                if (!p || !p.enabled || p.state !== 'running' || !p.data) return;
                Object.keys(p.data).forEach((name) => {
                    const v = p.data[name];
                    if (v === null || v === undefined) return;
                    const key = p.id + '.' + name;
                    fresh[key] = valueText(v);
                    out.push({ key, plugin: p.name || p.id, name, current: fresh[key] });
                });
            });
            placeholderValues = fresh;
            out.sort((a, b) => (a.plugin + a.name).localeCompare(b.plugin + b.name));
            return out;
        }

        // The text as the controller shows it: every {id.value} filled in, "--" where the value is not
        // known. What was filled in is not searched again; the result is cut to 64 only if something was.
        function expandPlaceholders(text) {
            const source = String(text || '');
            let filled = false;
            const result = source.replace(/\{([a-z0-9_-]{1,32})\.([a-z0-9_]{1,24})\}/g, (match, id, name) => {
                filled = true;
                const key = id + '.' + name;
                return Object.prototype.hasOwnProperty.call(placeholderValues, key) ? placeholderValues[key] : '--';
            });
            return filled ? result.slice(0, 64) : source;
        }

        if (window.HyperUI) {
            window.HyperUI.pluginValues = pluginValues;
            window.HyperUI.expandPlaceholders = expandPlaceholders;
        }

        window.HyperPlugins = { load, openSettings, openValues, openInstall };

    }

    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
    else start();
}());
