// Page de commande du poêle.
"use strict";

let current = null;
let busy = false;

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

function renderDevice(d) {
  const w = d.wifi, m = d.mqtt, r = d.radio;
  const rows = [
    ["WiFi", w.connected ? `${dot("ok")}${w.ssid} (${w.rssi} dBm)` : `${dot("err")}déconnecté`],
    ["Adresse", w.connected ? `${w.ip} · ${d.hostname}.local` : "—"],
    ["MQTT", !m.enabled ? `${dot("")}désactivé`
      : m.connected ? `${dot("ok")}connecté` : `${dot("warn")}${m.error || "déconnecté"}`],
    ["Radio", r.present ? `${dot("ok")}RFM69 (v0x${r.version.toString(16)})` : `${dot("err")}module absent`],
    ["Version", d.version],
  ];
  $("#status").innerHTML = rows.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("");
}

async function refresh() {
  if (busy) return;
  try {
    const st = await api("/api/status");
    renderStove(st.stove);
    renderDevice(st.device);
  } catch (e) {
    $("#state").textContent = "Module injoignable";
  }
}

async function send(cmd) {
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
