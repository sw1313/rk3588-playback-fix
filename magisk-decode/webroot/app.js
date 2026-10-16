"use strict";
(() => {
  // KernelSU's documented exec callback contract, implemented without remote dependencies.
  const catalogs = window.DecodeI18n;
  const languageKey = "rk3588.decode.language";
  let language = (navigator.language || "en").toLowerCase().startsWith("zh") ? "zh" : "en";
  try { const saved = localStorage.getItem(languageKey); if (saved === "zh" || saved === "en") language = saved; } catch {}
  const t = (key, args = {}) => (catalogs[language][key] || catalogs.en[key] || key).replace(/\{(\w+)\}/g, (match, name) => args[name] ?? match);
  function uiError(key, args = {}) { const error = new Error(key); error.translation = "error." + key; error.args = args; return error; }
  const bridge = "sh /data/adb/modules/rk3588_decode_stack/webui.sh";
  const definitions = [
    ["rga", "group.hardware", t("name.rga"), t("desc.rga"), []],
    ["mpp", "group.hardware", t("name.mpp"), t("desc.mpp"), []],
    ["av1_grain", "group.hardware", t("name.av1_grain"), t("desc.av1_grain"), ["mpp"]],
    ["codec2", "group.hardware", t("name.codec2"), t("desc.codec2"), ["mpp", "rga"]],
    ["codec2_backend", "group.hardware", t("name.codec2_backend"), t("desc.codec2_backend"), ["codec2"]],
    ["codec2_buffer", "group.hardware", t("name.codec2_buffer"), t("desc.codec2_buffer"), ["codec2_sdk"]],
    ["av1_native", "group.hardware", t("name.av1_native"), t("desc.av1_native"), ["codec2_sdk_next"]],
    ["av1_afbc", "group.hardware", t("name.av1_afbc"), t("desc.av1_afbc"), ["av1_native"]],
    ["software", "group.hardware", t("name.software"), t("desc.software"), ["codec2"]],
    ["p010_fast", "group.hardware", t("name.p010_fast"), t("desc.p010_fast"), ["codec2"]],
    ["hdr_static", "group.hardware", t("name.hdr_static"), t("desc.hdr_static"), ["codec2"]],
    ["hdr10plus", "group.hardware", t("name.hdr10plus"), t("desc.hdr10plus"), ["codec2"]],
    ["libyuv", "group.pixels", t("name.libyuv"), t("desc.libyuv"), []]
  ];
  const parents = { av1_grain: "mpp", codec2: "mpp", codec2_backend: "codec2", codec2_buffer: "codec2_backend", av1_native: "codec2_backend", av1_afbc: "av1_native", software: "codec2", p010_fast: "codec2", hdr_static: "codec2", hdr10plus: "codec2" };
  const names = Object.fromEntries(definitions.map(([key, , title]) => [key, title]));
  names.codec2_sdk = t("name.codec2_sdk");
  names.codec2_sdk_next = t("name.codec2_sdk_next");
  const backendNames = { legacy: t("backend.legacy"), simd: t("backend.simd"), latest: t("backend.latest") };
  function backend(components, field) {
    const sdk = components.codec2_sdk[field], latest = components.codec2_sdk_next[field];
    if (sdk === null || latest === null) return "unknown";
    return sdk === 0 ? "legacy" : latest === 1 ? "latest" : "simd";
  }
  const rows = new Map();
  let state = null;
  let lastError = null;
  let lastChange = null;
  let busy = false;
  let counter = 0;
  const $ = id => document.getElementById(id);
  const bitLabel = value => t(value === 1 ? "on" : value === 0 ? "off" : "unknown");
  const element = (tag, text, cls) => { const node = document.createElement(tag); if (text) node.textContent = text; if (cls) node.className = cls; return node; };
  function notice(text, kind) { $("notice").textContent = text; $("notice").className = "notice " + kind; }
  function controls() {
    const blocked = busy || !state || !state.valid || state.removing;
    $("whole").disabled = busy || !state || state.removing;
    for (const row of rows.values()) row.toggle.disabled = blocked;
    $("refresh").disabled = busy;
  }
  function exec(command) {
    return new Promise((resolve, reject) => {
      if (!window.ksu || typeof window.ksu.exec !== "function") { reject(uiError("host")); return; }
      const callback = "rkDecode_" + Date.now() + "_" + counter++;
      const cleanup = () => { clearTimeout(timer); delete window[callback]; };
      const timer = setTimeout(() => { cleanup(); reject(uiError("timeout")); }, 15000);
      window[callback] = (errno, stdout) => {
        cleanup();
        if (Number(errno) !== 0) { reject(uiError("operation", { errno })); return; }
        try { resolve(JSON.parse(stdout)); } catch { reject(uiError("json")); }
      };
      try { window.ksu.exec(command, "{}", callback); } catch { cleanup(); reject(uiError("permission")); }
    });
  }
  function render(next) {
    if (next.schema !== 1 || !next.components || definitions.some(([k]) => k !== "codec2_backend" && !next.components[k]) || !next.components.codec2_sdk || !next.components.codec2_sdk_next) throw uiError("schema");
    state = next;
    $("version").textContent = t("version", { version: next.version, active: next.activeKnown ? next.activeEnabled ? next.activeVersion : t("originals") : t("unconfirmed") });
    $("whole").setAttribute("aria-checked", String(next.enabled));
    $("whole-state").textContent = t("wholeState", { current: t(next.activeKnown ? next.activeEnabled ? "enabled" : "disabled" : "unknown"), next: t(next.enabled && !next.removing ? "enabled" : "disabled") });
    for (const [key, , , , deps] of definitions) {
      if (key === "codec2_backend") {
        const row = rows.get(key);
        row.section.classList.toggle("inactive", next.components.codec2.effective === 0);
        row.toggle.value = backend(next.components, "requested");
        row.current.textContent = t("current", { value: next.components.codec2.active === null ? t("unknown") : next.components.codec2.active === 0 ? t("inactive") : backendNames[backend(next.components, "active")] });
        row.effective.textContent = t("next", { value: next.components.codec2.effective === null ? t("unknown") : next.components.codec2.effective === 0 ? t("inactive") : backendNames[backend(next.components, "effective")] });
        row.reason.textContent = next.components.codec2.effective === 0 ? t("backendRetained") : "";
        row.reason.hidden = !row.reason.textContent;
        continue;
      }
      const info = next.components[key], row = rows.get(key);
      row.section.classList.toggle("inactive", info.effective === 0);
      row.toggle.setAttribute("aria-checked", String(info.requested === 1));
      row.current.textContent = t("current", { value: bitLabel(info.active) });
      row.effective.textContent = t("next", { value: bitLabel(info.effective) });
      let reason = "";
      if (info.requested === 1 && info.effective === 0) {
        const missing = deps.filter(k => next.components[k].effective !== 1).map(k => names[k]);
        reason = !next.enabled || next.removing ? t("wholeRetained") : t("dependenciesRetained", { items: missing.join(t("separator")) });
      }
      row.reason.textContent = reason;
      row.reason.hidden = !reason;
    }
    if (next.removing) notice(t("removing"), "pending");
    else if (!next.valid) notice(t("invalid"), "error");
    else if (next.health === "failed") notice(t("failed"), "error");
    else if (next.pendingReboot) notice((next.staged ? t("staged") : "") + (!next.activeKnown ? t("noReceipt") : "") + t("pending"), "pending");
    else notice(t("ready"), "ready");
    $("health").textContent = t("health", { value: t("health." + (["passed", "checking", "failed"].includes(next.health) ? next.health : "unknown")) });
  }
  async function update(key, value) {
    if (busy) return;
    if (key && key !== "all" && !names[key]) return;
    if (key === "codec2_backend" && !backendNames[value]) return;
    busy = true; controls();
    try {
      const command = key === "codec2_backend" ? bridge + " backend " + value : key ? bridge + " set " + key + " " + (value ? "on" : "off") : bridge + " status";
      lastError = null;
      const before = state;
      const result = await exec(command);
      render(result);
      const linked = before && key ? definitions.filter(([k]) => k !== key && k !== "codec2_backend" && before.components[k].requested === 0 && result.components[k].requested === 1).map(([k]) => k) : [];
      const changedBackend = before && key !== "codec2_backend" && backend(before.components, "requested") !== backend(result.components, "requested") ? backend(result.components, "requested") : null;
      lastChange = key ? { linked, backend: changedBackend } : null;
      paintChanges();
    } catch (error) {
      state = null; lastChange = null;
      lastError = { key: error.translation || "error.generic", args: error.args || {} };
      applyLanguage();
    } finally { busy = false; controls(); }
  }
  let group = "";
  for (const [key, category, title, description, deps] of definitions) {
    if (!parents[key] && category !== group) { const heading = element("h2", category, "group-title"); heading.dataset.i18n = key === "libyuv" ? "group.pixels" : "group.hardware"; $("components").append(heading); group = category; }
    const section = element("section", "", "component"), content = element("div");
    const heading = element("h3"), descriptionNode = element("p", "", "detail"), dependency = element("p", "", "depends");
    content.append(heading, descriptionNode, dependency);
    const badges = element("div", "", "states"), current = element("span", t("current", { value: t("unknown") }), "badge"), effective = element("span", t("next", { value: t("unknown") }), "badge");
    badges.append(current, effective); content.append(badges);
    const reason = element("p", "", "blocked"); reason.hidden = true; content.append(reason);
    const toggle = element(key === "codec2_backend" ? "select" : "button"); toggle.id = "toggle-" + key; toggle.disabled = true;
    toggle.setAttribute("aria-label", title);
    if (key === "codec2_backend") {
      section.classList.add("backend");
      for (const [value, title] of Object.entries(backendNames)) { const option = element("option", title); option.value = value; toggle.append(option); }
      toggle.addEventListener("change", () => update(key, toggle.value));
    } else {
      toggle.type = "button"; toggle.setAttribute("role", "switch"); toggle.setAttribute("aria-checked", "false"); toggle.append(element("span"));
      toggle.addEventListener("click", () => update(key, state.components[key].requested !== 1));
    }
    section.append(content, toggle);
    const wrapper = element("div", "", "tree-node"), children = element("div", "", "children");
    wrapper.dataset.component = key; wrapper.append(section, children);
    (parents[key] ? rows.get(parents[key]).children : $("components")).append(wrapper);
    rows.set(key, { toggle, current, effective, reason, section, children, heading, description: descriptionNode, dependency, deps });
  }
  function paintChanges() {
    const items = lastChange ? lastChange.linked.map(k => names[k]) : [];
    if (lastChange && lastChange.backend) items.push(t("backendChanged", { value: backendNames[lastChange.backend] }));
    $("changes").textContent = lastChange ? t("saved") + (items.length ? t("linked", { items: items.join(t("separator")) }) : "") + t("applyReboot") : "";
  }
  function applyLanguage() {
    document.documentElement.lang = language === "zh" ? "zh-CN" : "en";
    document.title = t("pageTitle");
    for (const node of document.querySelectorAll("[data-i18n]")) node.textContent = t(node.dataset.i18n);
    for (const node of document.querySelectorAll("[data-i18n-aria]")) node.setAttribute("aria-label", t(node.dataset.i18nAria));
    $("language").textContent = language === "zh" ? "English" : "中文";
    $("language").setAttribute("aria-label", t("languageAction"));
    $("whole").setAttribute("aria-label", t("wholeLabel"));
    for (const key of Object.keys(names)) names[key] = t("name." + key);
    for (const key of Object.keys(backendNames)) backendNames[key] = t("backend." + key);
    for (const [key, row] of rows) {
      row.heading.textContent = names[key]; row.description.textContent = t("desc." + key);
      row.dependency.textContent = t("dependency", { items: row.deps.length ? row.deps.map(k => names[k]).join(t("separator")) : t("noDependencies") });
      row.toggle.setAttribute("aria-label", names[key]);
      if (key === "codec2_backend") for (const option of row.toggle.options) option.textContent = backendNames[option.value];
      if (!state) { row.current.textContent = t("current", { value: t("unknown") }); row.effective.textContent = t("next", { value: t("unknown") }); row.reason.hidden = true; }
    }
    if (state) render(state);
    else {
      $("version").textContent = t(lastError ? "unavailable" : "loading");
      $("whole-state").textContent = t("wholeState", { current: t("unknown"), next: t("unknown") });
      $("health").textContent = t("healthLoading");
      notice(lastError ? t(lastError.key, lastError.args) : t("connecting"), lastError ? "error" : "");
    }
    paintChanges(); controls();
  }
  $("language").addEventListener("click", () => {
    language = language === "zh" ? "en" : "zh";
    try { localStorage.setItem(languageKey, language); } catch {}
    applyLanguage();
  });
  applyLanguage();
  $("whole").addEventListener("click", () => update("all", !state.enabled));
  $("refresh").addEventListener("click", () => update());
  document.addEventListener("visibilitychange", () => { if (!document.hidden) update(); });
  update();
})();
