// Page de commande du poêle.
"use strict";

let current = null;
let autoState = null;
let busy = false;

const PHASES = {
  off: "Arrêt (en attente)",
  startup: "Allumage",
  heat: "Chauffe",
  hold: "Maintien",
};

function setActive(groupId, value) {
  document.querySelectorAll(`#${groupId} button`).forEach((b) => {
    b.classList.toggle("active", b.dataset.v === String(value));
  });
}

function renderStove(s) {
  current = s;
  $("#state").textContent = s.on ? "En marche" : "Arrêté";
  $("#toggle").classList.toggle("on", s.on);
  setActive("power", s.power);
  setActive("fan", s.fan);
  const tx = s.tx_count === 0 ? "Aucune commande envoyée depuis le démarrage"
    : `Dernier envoi ${formatDuration(s.last_tx_age_s)}${s.last_tx_ok ? "" : " (échec radio)"}`;
  $("#last-tx").textContent = tx;
}

function renderAuto(a, temp) {
  autoState = a;
  $("#auto-toggle").classList.toggle("on", a.enabled);
  let sub;
  if (!a.time_valid && a.enabled && a.degraded) {
    sub = `${dot("warn")}heure inconnue : régulation sur la dernière consigne, sans nouvel allumage`;
  } else if (!a.time_valid) sub = `${dot("warn")}heure inconnue (synchronisation NTP en attente)`;
  else if (a.enabled && !a.sensor) sub = `${dot("warn")}pas de sonde de température : le poêle ne sera pas allumé`;
  else if (a.enabled) sub = `${dot("ok")}le module pilote le poêle`;
  else sub = "désactivé : le poêle est piloté à la main";
  $("#auto-sub").innerHTML = sub;

  const rows = [];
  if (a.time_valid || a.degraded) {
    rows.push(["Heure du module", a.time_valid ? `${DAY_NAMES[a.day]} ${fmtMinute(a.minute)}` : `${dot("warn")}inconnue`]);
    rows.push([a.time_valid ? "Consigne actuelle" : "Dernière consigne", fmtTarget(a.target)]);
    if (temp && temp.celsius !== undefined) rows.push(["Température", fmtTarget(temp.celsius)]);
    if (a.enabled && a.phase) {
      let ph = PHASES[a.phase] || a.phase;
      if (a.power) ph += ` · puissance ${a.power}`;
      if (a.phase_duration_s) {
        const left = Math.max(0, Math.ceil((a.phase_duration_s - a.phase_elapsed_s) / 60));
        ph += ` · encore ${left} min`;
      }
      rows.push(["Régulation", ph]);
    }
    if (!a.time_valid) {
      rows.push(["Prochain changement", "programmation suspendue jusqu'au retour de l'heure"]);
    } else if (a.next) {
      const when = a.next.day === a.day ? fmtMinute(a.next.minute)
        : `${DAY_NAMES[a.next.day].toLowerCase()} ${fmtMinute(a.next.minute)}`;
      rows.push(["Prochain changement", `${when} → ${fmtTarget(a.next.target)}`]);
    } else {
      rows.push(["Prochain changement", "aucun (programmation vide ?)"]);
    }
  }
  $("#auto-status").innerHTML = rows.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("");
}

function renderDevice(d) {
  const w = d.wifi, m = d.mqtt, r = d.radio;
  const rows = [
    ["WiFi", w.connected ? `${dot("ok")}${w.ssid} (${w.rssi} dBm)` : `${dot("err")}déconnecté`],
    ["Adresse", w.connected ? `${w.ip} · ${d.hostname}.local` : "—"],
    ["MQTT", !m.enabled ? `${dot("")}désactivé`
      : m.connected ? `${dot("ok")}connecté` : `${dot("warn")}${m.error || "déconnecté"}`],
    ["Radio", r.present ? `${dot("ok")}RFM69 (v0x${r.version.toString(16)})` : `${dot("err")}module absent`],
  ];
  const t = d.temperature;
  if (t && t.present) {
    rows.push(["Température", t.celsius !== undefined
      ? `${t.celsius.toFixed(1).replace(".", ",")} °C` : `${dot("warn")}mesure en cours…`]);
  }
  rows.push(
    ["Version", d.version],
  );
  $("#status").innerHTML = rows.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("");
}

async function refresh() {
  if (busy) return;
  try {
    const st = await api("/api/status");
    renderStove(st.stove);
    renderDevice(st.device);
    if (st.auto) renderAuto(st.auto, st.device.temperature);
  } catch (e) {
    $("#state").textContent = "Module injoignable";
  }
}

async function send(cmd) {
  if (autoState && autoState.enabled &&
      !confirm("Une commande manuelle désactive le mode autonome. Continuer ?")) return;
  busy = true;
  document.querySelectorAll("button").forEach((b) => (b.disabled = true));
  try {
    renderStove(await api("/api/stove", cmd));
    toast("Commande envoyée");
  } catch (e) {
    toast(e.message, true);
  } finally {
    busy = false;
    document.querySelectorAll("button").forEach((b) => (b.disabled = false));
    setTimeout(refresh, 1200); // récupère le résultat de l'émission radio
  }
}

$("#toggle").addEventListener("click", () => current && send({ on: !current.on }));

$("#power").addEventListener("click", (e) => {
  const v = e.target.dataset.v;
  if (v) send({ power: Number(v) });
});

$("#fan").addEventListener("click", (e) => {
  const v = e.target.dataset.v;
  if (v) send({ fan: v === "auto" ? "auto" : Number(v) });
});

$("#auto-toggle").addEventListener("click", async () => {
  if (!autoState) return;
  const enable = !autoState.enabled;
  try {
    await api("/api/auto", { enabled: enable });
    toast(enable ? "Mode autonome activé" : "Mode autonome désactivé");
  } catch (e) {
    toast(e.message, true);
  }
  refresh();
});

$("#resend").addEventListener("click", async () => {
  try {
    await api("/api/stove/resend", {});
    toast("Commande renvoyée");
    setTimeout(refresh, 1200);
  } catch (e) {
    toast(e.message, true);
  }
});

refresh();
setInterval(refresh, 5000);
