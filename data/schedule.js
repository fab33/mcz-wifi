// Page de programmation du mode autonome.
//
// La semaine est éditée localement puis envoyée en entier (POST /api/schedule).
// Format : days[0..6] (lundi en premier) = [{start, end, target}], minutes
// depuis minuit, target en °C ou null pour « arrêt ».
"use strict";

const MAX_SLOTS = 12;
const GROUPS = {
  all: { label: "Tous les jours", days: [0, 1, 2, 3, 4, 5, 6] },
  week: { label: "Semaine (lundi → vendredi)", days: [0, 1, 2, 3, 4] },
  weekend: { label: "Week-end (samedi, dimanche)", days: [5, 6] },
};

let saved = null;      // programmation enregistrée sur le module
let week = null;       // programmation en cours d'édition
let selected = null;   // {day, index} de la plage en cours de modification
let nowInfo = null;    // {day, minute} du module

const clone = (o) => JSON.parse(JSON.stringify(o));

// --- couleurs -----------------------------------------------------------------

// Échelle orangée : plus la consigne est haute, plus la teinte est soutenue
// (15 °C pêche clair -> 24 °C rouge brique).
function slotColor(t) {
  const x = Math.min(1, Math.max(0, (t - 15) / 9));
  return `hsl(${Math.round(34 - 22 * x)} 85% ${Math.round(80 - 42 * x)}%)`;
}
const slotInk = (t) => ((t - 15) / 9 > 0.45 ? "#fff" : "#3b2414");

// --- édition ------------------------------------------------------------------

// Remplace l'intervalle [s, e[ d'une journée par une plage (ou le vide si
// target === undefined), en coupant les plages recouvertes, puis fusionne les
// plages contiguës identiques.
function paint(slots, s, e, target) {
  const out = [];
  for (const x of slots) {
    if (x.end <= s || x.start >= e) { out.push({ ...x }); continue; }
    if (x.start < s) out.push({ ...x, end: s });
    if (x.end > e) out.push({ ...x, start: e });
  }
  if (target !== undefined) out.push({ start: s, end: e, target });
  out.sort((a, b) => a.start - b.start);
  const merged = [];
  for (const x of out) {
    const last = merged[merged.length - 1];
    if (last && last.end === x.start && last.target === x.target) last.end = x.end;
    else merged.push(x);
  }
  return merged;
}

function daysOf(value) {
  return GROUPS[value] ? GROUPS[value].days : [Number(value)];
}

function parseTime(v) {
  const m = /^(\d{1,2}):(\d{2})/.exec(v || "");
  return m ? Number(m[1]) * 60 + Number(m[2]) : NaN;
}

function readRange() {
  const s = parseTime($("#ed-start").value);
  let e = parseTime($("#ed-end").value);
  if (isNaN(s) || isNaN(e)) throw new Error("Heures invalides");
  if (e === 0) e = 1440; // fin à 00:00 = minuit
  if (e <= s) throw new Error("La fin doit être après le début (fin à 00:00 = minuit)");
  return [s, e];
}

// Applique fn aux journées choisies. En modification, la plage d'origine est
// d'abord retirée, pour que changer ses heures ne laisse pas de morceau.
function applyToDays(days, fn) {
  const next = clone(week);
  if (selected) {
    const x = week[selected.day][selected.index];
    next[selected.day] = paint(next[selected.day], x.start, x.end, undefined);
  }
  for (const d of days) {
    next[d] = fn(next[d], d);
    if (next[d].length > MAX_SLOTS) throw new Error(`${DAY_NAMES[d]} : ${MAX_SLOTS} plages au maximum`);
  }
  week = next;
  selected = null;
  render();
}

$("#ed-action").addEventListener("change", () => {
  $("#ed-temp").disabled = $("#ed-action").value === "off";
});

$("#ed-apply").addEventListener("click", () => {
  try {
    const [s, e] = readRange();
    let target = null;
    if ($("#ed-action").value === "temp") {
      target = Math.round(parseFloat($("#ed-temp").value.replace(",", ".")) * 10) / 10;
      if (!(target >= 5 && target <= 30)) throw new Error("Consigne entre 5 et 30 °C");
    }
    const editing = !!selected;
    applyToDays(daysOf($("#ed-days").value), (slots) => paint(slots, s, e, target));
    toast(editing ? "Plage modifiée (pensez à enregistrer)" : "Plage ajoutée (pensez à enregistrer)");
  } catch (err) {
    toast(err.message, true);
  }
});

$("#ed-delete").addEventListener("click", () => {
  if (!selected) return;
  applyToDays([], (slots) => slots);
  toast("Plage supprimée (pensez à enregistrer)");
});

$("#ed-cancel").addEventListener("click", () => {
  selected = null;
  render();
});

$("#ed-erase").addEventListener("click", () => {
  try {
    let s = parseTime($("#ed-start").value), e = parseTime($("#ed-end").value);
    if (s === 0 && e === 0) e = 1440;
    else [s, e] = readRange();
    applyToDays(daysOf($("#ed-days").value), (slots) => paint(slots, s, e, undefined));
  } catch (err) {
    toast(err.message, true);
  }
});

$("#cp-go").addEventListener("click", () => {
  const from = Number($("#cp-from").value);
  const to = daysOf($("#cp-to").value).filter((d) => d !== from);
  if (!to.length) return toast("Choisissez une autre journée", true);
  selected = null; // une copie ne modifie pas la plage sélectionnée
  applyToDays(to, () => clone(week[from]));
  toast(`${DAY_NAMES[from]} copié vers ${to.map((d) => DAY_NAMES[d].toLowerCase()).join(", ")}`);
});

// Touche sur une plage : la charge dans l'éditeur.
function selectSlot(day, index) {
  const x = week[day][index];
  selected = { day, index };
  $("#ed-days").value = String(day);
  $("#ed-start").value = fmtMinute(x.start);
  $("#ed-end").value = fmtMinute(x.end % 1440);
  $("#ed-action").value = x.target === null ? "off" : "temp";
  if (x.target !== null) $("#ed-temp").value = x.target;
  $("#ed-temp").disabled = x.target === null;
  render();
  $("#editor").scrollIntoView({ behavior: "smooth", block: "start" });
}

// Touche sur une zone vide : prépare une plage à cet endroit.
function selectEmpty(day, minute) {
  const slots = week[day];
  let s = 0, e = 1440;
  for (const x of slots) {
    if (x.end <= minute) s = Math.max(s, x.end);
    if (x.start > minute) e = Math.min(e, x.start);
  }
  selected = null;
  $("#ed-days").value = String(day);
  $("#ed-start").value = fmtMinute(s);
  $("#ed-end").value = fmtMinute(e % 1440);
  $("#ed-action").value = "temp";
  $("#ed-temp").disabled = false;
  render();
  $("#editor").scrollIntoView({ behavior: "smooth", block: "start" });
}

// « Vider la semaine » : second appui pour confirmer (pas de boîte de dialogue,
// bloquée par certains navigateurs intégrés). Annulable avant « Enregistrer ».
let clearTimer = null;
$("#clear-all").addEventListener("click", () => {
  const btn = $("#clear-all");
  if (!clearTimer) {
    btn.textContent = "Confirmer : tout effacer ?";
    clearTimer = setTimeout(() => {
      clearTimer = null;
      btn.textContent = "Vider la semaine";
    }, 4000);
    return;
  }
  clearTimeout(clearTimer);
  clearTimer = null;
  btn.textContent = "Vider la semaine";
  selected = null;
  week = [[], [], [], [], [], [], []];
  render();
  toast("Semaine vidée : « Enregistrer » pour valider, « Annuler les modifications » pour revenir");
});

// --- affichage ----------------------------------------------------------------

function render() {
  const hours = [0, 6, 12, 18, 24].map((h) => `<span style="left:${(h / 24) * 100}%">${h}h</span>`).join("");
  let html = `<div class="week-row"><div></div><div class="hours">${hours}</div></div>`;
  for (let d = 0; d < 7; d++) {
    const today = nowInfo && nowInfo.day === d;
    const slots = week[d].map((x, i) => {
      const left = (x.start / 1440) * 100, width = ((x.end - x.start) / 1440) * 100;
      const off = x.target === null;
      const sel = selected && selected.day === d && selected.index === i;
      const text = width >= 9 ? (off ? "Arrêt" : `${x.target.toFixed(1).replace(".", ",")}°`) : "";
      const title = `${fmtMinute(x.start)} – ${fmtMinute(x.end)} : ${fmtTarget(x.target)}`;
      return `<div class="slot${off ? " off" : ""}${sel ? " sel" : ""}" data-i="${i}" title="${title}"
        style="left:${left}%;width:${width}%;${off ? "" : `background:${slotColor(x.target)};color:${slotInk(x.target)}`}">${text}</div>`;
    }).join("");
    const mark = today ? `<div class="now-mark" style="left:${(nowInfo.minute / 1440) * 100}%"></div>` : "";
    html += `<div class="week-row${today ? " today" : ""}"><div class="lbl">${DAY_SHORT[d]}</div>
      <div class="bar" data-d="${d}">${slots}${mark}</div></div>`;
  }
  $("#week").innerHTML = html;

  // éditeur : création ou modification de la plage touchée
  const sel = selected && week[selected.day][selected.index];
  if (selected && !sel) selected = null;
  $("#ed-title").textContent = sel
    ? `Modifier la plage du ${DAY_NAMES[selected.day].toLowerCase()}, ${fmtMinute(sel.start)} – ${fmtMinute(sel.end)}`
    : "Nouvelle plage";
  $("#ed-apply").textContent = sel ? "Modifier" : "Ajouter";
  $("#ed-delete").hidden = !sel;
  $("#ed-cancel").hidden = !sel;
  $("#ed-erase").hidden = !!sel;
  $("#ed-hint").textContent = sel
    ? "La plage est remplacée par les valeurs ci-dessus (jours compris : choisissez « Semaine » pour l'appliquer à toute la semaine)."
    : "« Effacer la période » vide l'intervalle choisi (de 00:00 à 00:00 : toute la journée).";

  const dirty = JSON.stringify(week) !== JSON.stringify(saved);
  $("#save").disabled = !dirty;
  $("#revert").disabled = !dirty;
  $("#dirty").textContent = dirty ? "Modifications non enregistrées" : "";
}

$("#week").addEventListener("click", (e) => {
  const bar = e.target.closest(".bar");
  if (!bar) return;
  const d = Number(bar.dataset.d);
  const slot = e.target.closest(".slot");
  if (slot) return selectSlot(d, Number(slot.dataset.i));
  const r = bar.getBoundingClientRect();
  selectEmpty(d, Math.floor(((e.clientX - r.left) / r.width) * 1440));
});

// --- chargement / enregistrement -------------------------------------------------

function fillSelects() {
  const groups = Object.entries(GROUPS).map(([k, g]) => `<option value="${k}">${g.label}</option>`).join("");
  const days = DAY_NAMES.map((n, i) => `<option value="${i}">${n}</option>`).join("");
  $("#ed-days").innerHTML = groups + days;
  $("#cp-from").innerHTML = days;
  $("#cp-to").innerHTML = groups + days;
  $("#cp-to").value = "1";
}

async function load() {
  try {
    const [sched, st] = await Promise.all([api("/api/schedule"), api("/api/status")]);
    saved = sched.days;
    week = clone(saved);
    if (st.auto && st.auto.time_valid) nowInfo = { day: st.auto.day, minute: st.auto.minute };
    render();
  } catch (e) {
    toast(e.message, true);
  }
}

$("#save").addEventListener("click", async () => {
  try {
    const r = await api("/api/schedule", { days: week });
    saved = r.days;
    week = clone(saved);
    render();
    toast("Programmation enregistrée");
  } catch (e) {
    toast(e.message, true);
  }
});

$("#revert").addEventListener("click", () => {
  week = clone(saved);
  selected = null;
  render();
});

window.addEventListener("beforeunload", (e) => {
  if (week && JSON.stringify(week) !== JSON.stringify(saved)) e.preventDefault();
});

fillSelects();
load();
