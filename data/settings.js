// Page de réglages.
"use strict";

const getPath = (obj, path) => path.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);

function setPath(obj, path, value) {
  const keys = path.split(".");
  const last = keys.pop();
  let o = obj;
  for (const k of keys) o = o[k] = o[k] || {};
  o[last] = value;
}

function fields() {
  return document.querySelectorAll("[data-key]");
}

function fillForm(cfg) {
  fields().forEach((el) => {
    const v = getPath(cfg, el.dataset.key);
    if (el.dataset.secret) {
      el.value = "";
      el.placeholder = getPath(cfg, el.dataset.secret) ? "•••••• (inchangé)" : "";
    } else if (el.type === "checkbox") {
      el.checked = !!v;
    } else if (el.dataset.type === "mhz") {
      el.value = (v / 1e6).toFixed(3);
    } else if (el.dataset.type === "id") {
      el.value = `${v} (0x${v.toString(16).toUpperCase()})`;
    } else {
      el.value = v ?? "";
    }
  });
}

function readForm() {
  const out = {};
  for (const el of fields()) {
    const key = el.dataset.key;
    let v = el.type === "checkbox" ? el.checked : el.value.trim();
    if (el.dataset.secret && v === "") continue; // inchangé
    switch (el.dataset.type) {
      case "int": v = parseInt(v, 10); break;
      case "float": v = parseFloat(v.replace(",", ".")); break;
      case "bool": v = v === "true"; break;
      case "mhz": v = Math.round(parseFloat(v) * 1e6); break;
      case "id": {
        const m = v.match(/^\s*(0x[0-9a-f]+|\d+)/i);
        v = m ? Number(m[1]) : NaN;
        if (!(v >= 0 && v <= 0xffffff)) throw new Error("Identifiant invalide (24 bits)");
        break;
      }
    }
    if (typeof v === "number" && isNaN(v)) throw new Error(`Valeur invalide : ${el.labels[0]?.textContent || key}`);
    setPath(out, key, v);
  }
  if ($("#clear-web-password").checked) out.clear_web_password = true;
  return out;
}

// --- Bande radio : préréglages 433 / 868 MHz -------------------------------
const BANDS = [
  { min: 433.05, max: 434.79, maxDbm: 10, text: "Bande 433,05–434,79 MHz : 10 dBm (10 mW) maximum autorisé." },
  { min: 863, max: 870, maxDbm: 14, text: "Bande 868,7–869,2 MHz : 14 dBm (25 mW) maximum autorisé." },
];

function currentBand() {
  const mhz = parseFloat($("#radio-freq").value);
  return BANDS.find((b) => mhz >= b.min && mhz <= b.max);
}

function syncBand() {
  const hz = Math.round(parseFloat($("#radio-freq").value) * 1e6);
  const sel = $("#radio-band");
  sel.value = [...sel.options].some((o) => o.value === String(hz)) ? String(hz) : "";
  const band = currentBand();
  $("#radio-limit").textContent = band
    ? band.text
    : "Fréquence hors des bandes 433,05–434,79 MHz et 863–870 MHz : elle sera ramenée dans la bande la plus proche.";
}

$("#radio-band").addEventListener("change", (e) => {
  if (!e.target.value) return $("#radio-freq").focus();
  $("#radio-freq").value = (Number(e.target.value) / 1e6).toFixed(3);
  const band = currentBand();
  const power = $("#radio-power");
  if (band && parseInt(power.value, 10) > band.maxDbm) power.value = band.maxDbm;
  syncBand();
});
$("#radio-freq").addEventListener("input", syncBand);

async function load() {
  try {
    fillForm(await api("/api/settings"));
    syncBand();
  } catch (e) {
    toast(e.message, true);
  }
}

$("#form").addEventListener("submit", async (e) => {
  e.preventDefault();
  let body;
  try {
    body = readForm();
  } catch (err) {
    return toast(err.message, true);
  }
  try {
    const r = await api("/api/settings", body);
    if (r.reboot) {
      toast("Enregistré, redémarrage du module…");
    } else if (r.wifi_changed) {
      const host = body.wifi.hostname || "poele-mcz";
      toast(`Connexion au réseau « ${body.wifi.ssid} »… Le module sera joignable sur http://${host}.local`);
    } else {
      toast("Réglages enregistrés");
      load();
    }
  } catch (err) {
    toast(err.message, true);
  }
});

// Recherche des réseaux WiFi (scan asynchrone côté module)
$("#scan").addEventListener("click", async () => {
  const btn = $("#scan");
  btn.disabled = true;
  btn.textContent = "Recherche…";
  try {
    let r;
    for (let i = 0; i < 20; i++) {
      r = await api("/api/wifi/scan");
      if (!r.scanning) break;
      await new Promise((res) => setTimeout(res, 1000));
    }
    const list = (r.networks || []).sort((a, b) => b.rssi - a.rssi);
    $("#networks").innerHTML = list.length
      ? list.map((n) => `<li data-ssid="${encodeURIComponent(n.ssid)}"><span>${n.secure ? "🔒 " : ""}${
        n.ssid.replace(/</g, "&lt;")}</span><span>${n.rssi} dBm</span></li>`).join("")
      : "<li>Aucun réseau trouvé</li>";
  } catch (e) {
    toast(e.message, true);
  } finally {
    btn.disabled = false;
    btn.textContent = "Rechercher les réseaux";
  }
});

$("#networks").addEventListener("click", (e) => {
  const li = e.target.closest("li[data-ssid]");
  if (!li) return;
  $("#wifi-ssid").value = decodeURIComponent(li.dataset.ssid);
  $("#wifi-password").focus();
});

$("#reboot").addEventListener("click", async () => {
  if (!confirm("Redémarrer le module ?")) return;
  await api("/api/reboot", {}).catch(() => {});
  toast("Redémarrage…");
});

$("#factory").addEventListener("click", async () => {
  if (!confirm("Effacer tous les réglages (WiFi compris) ? Le module repassera en point d'accès.")) return;
  await api("/api/factory-reset", {}).catch(() => {});
  toast("Réglages effacés, redémarrage…");
});

addPasswordToggles();
load();
