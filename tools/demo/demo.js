// Démo de l'interface web sans module : remplace fetch() par un faux module
// qui simule le poêle, la pièce et le mode autonome (même logique que
// lib/Thermostat). Chargé avant common.js par tools/make_demo.py.
//
// L'état est gardé dans le navigateur (localStorage) pour passer d'une page à
// l'autre. Un bandeau permet d'accélérer le temps simulé.
"use strict";
(() => {
  const KEY = "mcz-demo-v1";
  const P = (s) => JSON.parse(JSON.stringify(s));

  const DEFAULT_SCHEDULE = (() => {
    const wd = [{ start: 360, end: 510, target: 20 }, { start: 1020, end: 1350, target: 21 }];
    const we = [{ start: 480, end: 1380, target: 20.5 }];
    return [wd, wd, wd, wd, wd, we, we].map(P);
  })();

  function fresh() {
    return {
      simTime: Date.now(),       // horloge simulée (ms)
      lastReal: Date.now(),
      speed: 60,
      uptime: 0,                 // secondes simulées
      room: 18.6,                // température de la pièce
      stove: { on: false, power: 3, fan: "auto" },
      txCount: 0, lastTx: -1,
      auto: { enabled: true, phase: "off", phaseStart: 0, holdRef: 0 },
      timeLost: false,          // simule un redémarrage sans accès au serveur de temps
      lastTarget: undefined,    // dernière consigne appliquée avec l'heure (mode dégradé)
      schedule: P(DEFAULT_SCHEDULE),
      settings: {
        wifi: { ssid: "MaBox", hostname: "poele-mcz", has_password: true },
        mqtt: { enabled: true, host: "192.168.1.10", port: 1883, user: "mqtt", base_topic: "mcz/mcz-a1b2c3",
          discovery_prefix: "homeassistant", has_password: true },
        radio: { frequency_hz: 868950000, high_power: true, power_dbm: 13, chip_us: 414, repeats: 5 },
        stove: { id: 0xa1b2c3, fan2: 7, resend_s: 300 },
        sensor: { offset: 0 },
        time: { tz: "CET-1CEST,M3.5.0,M10.5.0/3", ntp: "pool.ntp.org" },
        auto: { hysteresis: 1, startup_power: 5, startup_min: 20, heat_power: 3, hold_power: 1, hold_min: 15, beep: true },
        has_web_password: false,
      },
    };
  }

  let S;
  try { S = JSON.parse(localStorage.getItem(KEY)); } catch (e) { S = null; }
  if (!S || !S.settings || !S.settings.auto) S = fresh();
  const save = () => { try { localStorage.setItem(KEY, JSON.stringify(S)); } catch (e) { /* navigation privée */ } };

  // --- programmation et régulation (copie de lib/Thermostat) ---------------------

  function local() {
    const d = new Date(S.simTime);
    return { day: (d.getDay() + 6) % 7, minute: d.getHours() * 60 + d.getMinutes(), date: d };
  }
  function targetAt(day, minute) {
    for (const x of S.schedule[day]) if (minute >= x.start && minute < x.end) return x.target;
    return null;
  }
  function nextChange(day, minute) {
    const cur = targetAt(day, minute);
    for (let k = 0; k <= 7; k++) {
      const dd = (day + k) % 7;
      const marks = [0];
      for (const x of S.schedule[dd]) { marks.push(x.start); if (x.end < 1440) marks.push(x.end); }
      marks.sort((a, b) => a - b);
      for (const m of marks) {
        if (k === 0 && m <= minute) continue;
        const t = targetAt(dd, m);
        if (t !== cur) return { day: dd, minute: m, target: t };
      }
    }
    return null;
  }
  const params = () => S.settings.auto;
  function powerOf(ph) {
    const p = params();
    return { startup: p.startup_power, heat: p.heat_power, hold: p.hold_power }[ph] || 0;
  }
  function enter(ph, temp) {
    S.auto.phase = ph;
    S.auto.phaseStart = S.uptime;
    if (ph === "hold") S.auto.holdRef = temp;
  }
  function regulate() {
    const { day, minute } = local();
    let target;
    if (!S.timeLost) {
      target = targetAt(day, minute);
      S.lastTarget = target;
    } else if (S.lastTarget !== undefined) {
      target = S.lastTarget;  // mode dégradé : pas de nouvel allumage
    } else {
      return;
    }
    const p = params();
    const a = S.auto;
    const temp = Math.round(S.room * 10) / 10;
    const el = S.uptime - a.phaseStart;
    if (target === null) { if (a.phase !== "off") enter("off"); }
    else if (a.phase === "off") { if (!S.timeLost && temp <= target - p.hysteresis + 1e-9) enter("startup"); }
    else if (a.phase === "startup") { if (el >= p.startup_min * 60) enter(temp >= target ? "hold" : "heat", temp); }
    else if (a.phase === "heat") { if (temp >= target) enter("hold", temp); }
    else if (a.phase === "hold" && el >= p.hold_min * 60) enter(temp < a.holdRef - 0.1 - 1e-9 ? "heat" : "off");

    const on = a.phase !== "off";
    const st = S.stove;
    const next = { on, power: on ? powerOf(a.phase) : st.power, fan: on ? "auto" : st.fan };
    if (next.on !== st.on || next.power !== st.power || next.fan !== st.fan) {
      S.stove = next;
      S.txCount++;
      S.lastTx = S.uptime;
    }
  }

  // Avance la simulation jusqu'à maintenant (pas de 10 s simulées).
  function advance() {
    const real = Date.now();
    let dt = Math.min((real - S.lastReal) / 1000 * S.speed, 2 * 86400);
    S.lastReal = real;
    while (dt > 0) {
      const h = Math.min(10, dt);
      dt -= h;
      S.simTime += h * 1000;
      S.uptime += h;
      // pièce : pertes vers 8 °C (constante de temps 2 h) + apport du poêle
      const heat = S.stove.on ? S.stove.power * 0.0007 : 0;
      S.room += h * (heat - (S.room - 8) / 7200);
      if (S.auto.enabled) regulate();
      // renvoi périodique silencieux (stove.resend_s), comme Stove::loop()
      const rs = S.settings.stove.resend_s;
      if (rs > 0 && S.txCount > 0 && S.uptime - S.lastTx >= rs) { S.txCount++; S.lastTx = S.uptime; }
    }
    save();
  }

  // --- API simulée ---------------------------------------------------------------

  function status() {
    const { day, minute, date } = local();
    const pad = (n) => String(n).padStart(2, "0");
    const a = S.auto;
    const degraded = S.timeLost && S.lastTarget !== undefined;
    const auto = S.timeLost ? { enabled: a.enabled, time_valid: false, time: "", sensor: true, degraded } : {
      enabled: a.enabled, time_valid: true, sensor: true, degraded: false, day, minute,
      time: `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(minute % 60)}:00`,
      target: targetAt(day, minute),
    };
    if (degraded) auto.target = S.lastTarget;
    const n = !S.timeLost && nextChange(day, minute);
    if (n) auto.next = n;
    if (S.timeLost && !degraded) return finish(auto);
    if (a.enabled) {
      auto.phase = a.phase;
      if (a.phase !== "off") auto.power = powerOf(a.phase);
      auto.phase_elapsed_s = Math.round(S.uptime - a.phaseStart);
      const p = params();
      auto.phase_duration_s = a.phase === "startup" ? p.startup_min * 60 : a.phase === "hold" ? p.hold_min * 60 : 0;
      if (a.phase === "hold") auto.hold_ref = a.holdRef;
    }
    return finish(auto);
  }

  function finish(auto) {
    const temp = Math.round((S.room + S.settings.sensor.offset) * 10) / 10;
    return {
      stove: { ...S.stove, tx_count: S.txCount, last_tx_ok: true,
        last_tx_age_s: S.lastTx < 0 ? -1 : Math.round(S.uptime - S.lastTx) },
      auto,
      device: {
        version: "1.2.0-démo", uptime_s: Math.round(S.uptime), free_heap: 31000, hostname: S.settings.wifi.hostname,
        wifi: { connected: true, ssid: S.settings.wifi.ssid, ip: "192.168.1.42", rssi: -58, ap: false, ap_name: "MCZ-a1b2c3" },
        mqtt: { enabled: S.settings.mqtt.enabled, connected: S.settings.mqtt.enabled, error: "" },
        radio: { present: true, version: 0x24 },
        temperature: { present: true, celsius: temp, age_s: 12 },
      },
    };
  }

  function manual(transmit = true) {
    if (S.auto.enabled) S.auto.enabled = false;
    if (!transmit) return;  // poêle arrêté : réglage seulement mémorisé
    S.txCount++;
    S.lastTx = S.uptime;
  }

  function validWeek(days) {
    if (!Array.isArray(days) || days.length !== 7) return "\"days\" doit contenir 7 journées";
    for (let d = 0; d < 7; d++) {
      const s = [...days[d]].sort((a, b) => a.start - b.start);
      if (s.length > 12) return "trop de plages";
      for (let i = 0; i < s.length; i++) {
        const x = s[i];
        if (!(x.start >= 0 && x.end <= 1440 && x.start < x.end) || (i && s[i - 1].end > x.start) ||
            (x.target !== null && !(x.target >= 5 && x.target <= 30))) return `plage invalide (${DAY_NAMES[d].toLowerCase()})`;
      }
      days[d] = s;
    }
    return null;
  }

  function merge(dst, src) {
    for (const [k, v] of Object.entries(src)) {
      if (v && typeof v === "object" && !Array.isArray(v)) merge(dst[k] = dst[k] || {}, v);
      else if (!/password/.test(k)) dst[k] = v;
    }
  }

  function route(method, path, body) {
    advance();
    switch (`${method} ${path}`) {
      case "GET /api/status": return [200, status()];
      case "POST /api/stove": {
        const n = { ...S.stove };
        if ("on" in body) n.on = !!body.on;
        if ("power" in body) n.power = body.power;
        if ("fan" in body) n.fan = body.fan;
        manual(n.on || "on" in body);
        S.stove = n;
        return [200, status().stove];
      }
      case "POST /api/stove/resend": S.txCount++; S.lastTx = S.uptime; return [200, { ok: true }];
      case "POST /api/auto":
        if (body.enabled && !S.auto.enabled) {
          enter(S.stove.on ? "heat" : "off");
          S.txCount++; S.lastTx = S.uptime;  // émission de synchronisation
        }
        S.auto.enabled = !!body.enabled;
        regulate();
        return [200, status().auto];
      case "GET /api/schedule": return [200, { days: S.schedule }];
      case "POST /api/schedule": {
        const err = validWeek(body.days);
        if (err) return [400, { error: err }];
        S.schedule = body.days;
        return [200, { days: S.schedule }];
      }
      case "GET /api/settings": return [200, S.settings];
      case "POST /api/settings": merge(S.settings, body); return [200, { ok: true, wifi_changed: false, reboot: false }];
      case "GET /api/wifi/scan":
        return [200, { scanning: false, networks: [
          { ssid: "MaBox", rssi: -52, secure: true }, { ssid: "Voisin", rssi: -80, secure: true }] }];
      case "POST /api/reboot": return [200, { ok: true }];
      case "POST /api/factory-reset": S = fresh(); return [200, { ok: true }];
    }
    return [404, { error: "Introuvable (démo)" }];
  }

  // Certains lecteurs (aperçus intégrés) bloquent confirm() : la démo affiche
  // le message et continue.
  window.confirm = (msg) => {
    setTimeout(() => typeof toast === "function" && toast(msg), 1500);
    return true;
  };

  const realFetch = window.fetch.bind(window);
  window.fetch = async (url, opts = {}) => {
    const path = new URL(url, location.href).pathname.replace(/^.*(\/api\/)/, "$1");
    if (!path.startsWith("/api/")) return realFetch(url, opts);
    const body = opts.body ? JSON.parse(opts.body) : {};
    const [code, data] = route((opts.method || "GET").toUpperCase(), path, body);
    save();
    await new Promise((r) => setTimeout(r, 120));
    return new Response(JSON.stringify(data), { status: code, headers: { "Content-Type": "application/json" } });
  };

  // --- bandeau de démo -----------------------------------------------------------

  document.addEventListener("DOMContentLoaded", () => {
    const st = document.createElement("style");
    st.textContent = `
      .demo-bar { background: var(--accent-soft); border-bottom: 1px solid var(--line); font-size: .85rem; }
      .demo-bar div { max-width: 560px; margin: 0 auto; padding: 8px 16px; display: flex; flex-wrap: wrap;
        gap: 6px 10px; align-items: center; }
      .demo-bar b { color: var(--accent); }
      .demo-bar select, .demo-bar button { width: auto; padding: 4px 8px; font-size: .85rem; border-radius: 7px; }
      .demo-bar button { border: 1px solid var(--line); background: var(--card); color: var(--text); cursor: pointer; }
      .demo-bar span { color: var(--muted); }`;
    document.head.appendChild(st);
    const bar = document.createElement("div");
    bar.className = "demo-bar";
    bar.innerHTML = `<div><b>Démo</b><span>aucun poêle n'est piloté ; pièce et poêle simulés.</span>
      <label style="display:flex;align-items:center;gap:6px;margin:0;color:inherit">Temps
        <select id="demo-speed">
          <option value="1">réel</option><option value="60">×60 (1 min = 1 h)</option>
          <option value="600">×600</option></select></label>
      <button id="demo-time"></button>
      <button id="demo-reset">Réinitialiser</button></div>`;
    document.body.prepend(bar);
    const sel = bar.querySelector("#demo-speed");
    sel.value = String(S.speed);
    sel.addEventListener("change", () => { advance(); S.speed = Number(sel.value); save(); });
    const tb = bar.querySelector("#demo-time");
    const label = () => (tb.textContent = S.timeLost ? "Rétablir l'heure (NTP)" : "Redémarrage sans Internet");
    label();
    tb.addEventListener("click", () => {
      advance();
      S.timeLost = !S.timeLost;
      if (S.timeLost) enter(S.stove.on ? "heat" : "off");  // redémarrage : régulation réinitialisée
      save();
      location.reload();
    });
    bar.querySelector("#demo-reset").addEventListener("click", () => {
      S = fresh(); save(); location.reload();
    });
  });
})();
