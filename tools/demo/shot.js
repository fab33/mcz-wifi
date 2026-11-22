// Captures pour la documentation (tools/make_screenshots.py) : état fixe
// (lundi 18:40, chauffe), sans bandeau de démo.
(async () => {
  const p = new URLSearchParams(location.search);
  if (location.hash !== "#ready") {
    localStorage.removeItem("mcz-demo-v1");
    await api("/api/status");
    const S = JSON.parse(localStorage.getItem("mcz-demo-v1"));
    S.simTime = new Date(2026, 10, 23, 18, 40).getTime();
    S.uptime = 100000; S.lastReal = Date.now(); S.speed = 1; S.room = 19.9;
    S.stove = { on: true, power: 3, fan: "auto" }; S.txCount = 3; S.lastTx = S.uptime - 70;
    S.auto = { enabled: true, phase: "heat", phaseStart: S.uptime - 1500, holdRef: 0 };
    localStorage.setItem("mcz-demo-v1", JSON.stringify(S));
    location.hash = "#ready"; location.reload(); return;
  }
  const st = document.createElement("style");
  st.textContent = ".demo-bar{display:none!important} *{transition:none!important} html{scroll-behavior:auto!important}";
  document.head.appendChild(st);
  await new Promise((r) => setTimeout(r, 800));
  if (p.get("slot")) {
    const [d, i] = p.get("slot").split(",");
    document.querySelector(`.bar[data-d="${d}"] .slot[data-i="${i}"]`).click();
    await new Promise((r) => setTimeout(r, 300));
    window.scrollTo(0, 0);
  }
  if (p.get("only")) {  // ne garder qu'une carte
    const keep = document.querySelector(p.get("only")).closest(".card");
    document.querySelectorAll(".card, .actions:not(.card .actions)").forEach((c) => { if (c !== keep) c.hidden = true; });
  }
})();
