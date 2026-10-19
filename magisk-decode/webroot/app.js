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
    ["codec2_surface", "group.hardware", t("name.codec2_surface"), t("desc.codec2_surface"), ["codec2"]],
    ["codec2_native_backpressure", "group.hardware", t("name.codec2_native_backpressure"), t("desc.codec2_native_backpressure"), ["codec2_sdk_next", "codec2_surface", "codec2_lifecycle"]],
    ["codec2_backend", "group.hardware", t("name.codec2_backend"), t("desc.codec2_backend"), ["codec2"]],
    ["codec2_buffer", "group.hardware", t("name.codec2_buffer"), t("desc.codec2_buffer"), ["codec2_sdk"]],
    ["hevc_eos_pending", "group.hardware", t("name.hevc_eos_pending"), t("desc.hevc_eos_pending"), ["codec2_sdk_next"]],
    ["hevc_native_surface", "group.hardware", t("name.hevc_native_surface"), t("desc.hevc_native_surface"), ["codec2_sdk_next", "hdr_static", "codec2_surface"]],
    ["codec2_delay_notify", "group.hardware", t("name.codec2_delay_notify"), t("desc.codec2_delay_notify"), ["codec2_sdk_next"]],
    ["codec2_lifecycle", "group.hardware", t("name.codec2_lifecycle"), t("desc.codec2_lifecycle"), ["codec2_sdk_next"]],
    ["av1_native", "group.hardware", t("name.av1_native"), t("desc.av1_native"), ["codec2_sdk_next", "codec2_surface"]],
    ["av1_afbc", "group.hardware", t("name.av1_afbc"), t("desc.av1_afbc"), ["av1_native"]],
    ["av1_texture_afbc", "group.hardware", t("name.av1_texture_afbc"), t("desc.av1_texture_afbc"), ["av1_afbc"]],
    ["software", "group.hardware", t("name.software"), t("desc.software"), ["codec2"]],
    ["p010_fast", "group.hardware", t("name.p010_fast"), t("desc.p010_fast"), ["codec2"]],
    ["p010_copy_pool", "group.hardware", t("name.p010_copy_pool"), t("desc.p010_copy_pool"), ["codec2_sdk_next", "codec2_buffer"]],
    ["p010_pairs", "group.hardware", t("name.p010_pairs"), t("desc.p010_pairs"), ["codec2_sdk_next", "p010_fast"]],
    ["hdr_static", "group.hardware", t("name.hdr_static"), t("desc.hdr_static"), ["codec2"]],
    ["gralloc_hdr_lease", "group.hardware", t("name.gralloc_hdr_lease"), t("desc.gralloc_hdr_lease"), ["hdr_static", "codec2_sdk_next"]],
    ["hwui_rgb10", "group.hardware", t("name.hwui_rgb10"), t("desc.hwui_rgb10"), ["hdr_static"]],
    ["hwc_client_hdr10", "group.hardware", t("name.hwc_client_hdr10"), t("desc.hwc_client_hdr10"), ["hdr_static", "hwui_rgb10"]],
    ["hwc_mode_cache", "group.hardware", t("name.hwc_mode_cache"), t("desc.hwc_mode_cache"), ["hdr_static"]],
    ["display_policy", "group.hardware", t("name.display_policy"), t("desc.display_policy"), []],
    ["display_session", "group.hardware", t("name.display_session"), t("desc.display_session"), ["hwc_client_hdr10"]],
    ["display_qms", "group.hardware", t("name.display_qms"), t("desc.display_qms"), ["hdr_static"]],
    ["hdmi_audio_guard", "group.hardware", t("name.hdmi_audio_guard"), t("desc.hdmi_audio_guard"), []],
    ["hwc_scaled_overlay", "group.hardware", t("name.hwc_scaled_overlay"), t("desc.hwc_scaled_overlay"), ["hdr_static"]],
    ["hwc_metadata_reuse", "group.hardware", t("name.hwc_metadata_reuse"), t("desc.hwc_metadata_reuse"), ["hdr_static"]],
    ["hdr10plus", "group.hardware", t("name.hdr10plus"), t("desc.hdr10plus"), ["codec2"]],
    ["libyuv", "group.pixels", t("name.libyuv"), t("desc.libyuv"), []]
  ];
  const bundles = {"core": ["mpp", "rga", "codec2", "libyuv", "software"], "native": ["codec2_surface", "codec2_native_backpressure", "codec2_lifecycle", "codec2_delay_notify", "hevc_eos_pending", "codec2_buffer"], "av1": ["av1_native", "av1_grain", "av1_afbc", "av1_texture_afbc"], "hdr": ["hdr_static", "hdr10plus", "hwui_rgb10", "hwc_client_hdr10", "gralloc_hdr_lease", "hevc_native_surface", "p010_fast", "p010_pairs", "p010_copy_pool"], "display": ["hwc_mode_cache", "hwc_metadata_reuse", "hwc_scaled_overlay", "hdmi_audio_guard", "display_policy", "display_session"]};
  const bundleRows = new Map();
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
    for (const row of bundleRows.values()) row.toggle.disabled = blocked;
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
    for (const [id, keys] of Object.entries(bundles)) {
      const row = bundleRows.get(id);
      const count = field => keys.some(k => next.components[k][field] === null) ? t("unknown") : keys.filter(k => next.components[k][field] === 1).length;
      const selected = count("requested");
      row.toggle.setAttribute("aria-checked", selected === keys.length ? "true" : selected ? "mixed" : "false");
      row.status.textContent = t("bundleState", { selected, total: keys.length,
        current: next.activeKnown ? count("active") : t("unknown"), next: count("effective") });
    }
    if (next.removing) notice(t("removing"), "pending");
    else if (!next.valid) notice(t("invalid"), "error");
    else if (next.health === "failed") notice(t("failed"), "error");
    else if (next.pendingReboot) notice((next.staged ? t("staged") : "") + (!next.activeKnown ? t("noReceipt") : "") + t("pending"), "pending");
    else notice(t("ready"), "ready");
    $("health").textContent = t("health", { value: t("health." + (["passed", "checking", "failed"].includes(next.health) ? next.health : "unknown")) });
  }
  async function update(key, value, bundle = false) {
    if (busy) return;
    if (bundle ? !bundles[key] : key && key !== "all" && !names[key]) return;
    if (key === "codec2_backend" && !backendNames[value]) return;
    busy = true; controls();
    try {
      const command = bundle ? bridge + " bundle " + key + " " + (value ? "on" : "off") : key === "codec2_backend" ? bridge + " backend " + value : key ? bridge + " set " + key + " " + (value ? "on" : "off") : bridge + " status";
      lastError = null;
      const before = state;
      const result = await exec(command);
      render(result);
      const selectedKeys = bundle ? bundles[key] : [key];
      const linked = before && key ? definitions.filter(([k]) => !selectedKeys.includes(k) && k !== "codec2_backend" && before.components[k].requested === 0 && result.components[k].requested === 1).map(([k]) => k) : [];
      const changedBackend = before && key !== "codec2_backend" && backend(before.components, "requested") !== backend(result.components, "requested") ? backend(result.components, "requested") : null;
      lastChange = key ? { linked, backend: changedBackend } : null;
      paintChanges();
    } catch (error) {
      state = null; lastChange = null;
      lastError = { key: error.translation || "error.generic", args: error.args || {} };
      applyLanguage();
    } finally { busy = false; controls(); }
  }
  const containers = new Map();
  for (const id of [...Object.keys(bundles), "qms"]) {
    const card = element("section", "", "bundle"); card.dataset.bundle = id;
    const title = element("h2"), description = element("p"), scope = element("p", "", "detail");
    title.dataset.i18n = "bundle." + id;
    description.dataset.i18n = "bundleDesc." + id;
    scope.dataset.i18n = "bundleScope." + id;
    const intro = element("div"); intro.append(title, description, scope);
    const header = element("div", "", "bundle-header"); header.append(intro); card.append(header);
    if (id === "qms") containers.set(id, card);
    else {
      const toggle = element("button"); toggle.type = "button"; toggle.id = "bundle-" + id;
      // A mixed group is a tri-state checkbox; activation selects the whole group.
      toggle.setAttribute("role", "checkbox"); toggle.setAttribute("aria-checked", "false");
      toggle.disabled = true; toggle.append(element("span")); header.append(toggle);
      toggle.addEventListener("click", () => update(id, !bundles[id].every(k => state.components[k].requested === 1), true));
      const status = element("p", "", "bundle-status"); card.append(status);
      const detail = element("details"), summary = element("summary"); summary.dataset.i18n = "details";
      detail.append(summary); card.append(detail); containers.set(id, detail);
      bundleRows.set(id, { toggle, status });
    }
    $("components").append(card);
  }
  for (const [key, , title, , deps] of definitions) {
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
    const group = key === "display_qms" ? "qms" : key === "codec2_backend" ? "native" : Object.keys(bundles).find(id => bundles[id].includes(key));
    containers.get(group).append(wrapper);
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
    for (const [id, row] of bundleRows) {
      row.toggle.setAttribute("aria-label", t("bundle." + id));
      if (!state) row.status.textContent = t("unknown");
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
