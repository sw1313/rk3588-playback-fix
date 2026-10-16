"use strict";
(() => {
  // KernelSU's documented exec callback contract, implemented without remote dependencies.
  const bridge = "sh /data/adb/modules/rk3588_decode_stack/webui.sh";
  const definitions = [
    ["rga", "硬件与解码", "RGA", "图像转换、缩放与缓冲区处理。", []],
    ["mpp", "硬件与解码", "MPP / VPU", "硬件视频解码与 VPU 兼容库，32 / 64 位作为一组切换。", []],
    ["av1_grain", "硬件与解码", "AV1 胶片颗粒修正", "恢复片源携带的颗粒参数，不是额外添加滤镜。", ["mpp"]],
    ["codec2", "Codec2 与软件解码", "Codec2 兼容层", "保留原系统接口的解码组件与平台适配。", ["mpp", "rga"]],
    ["codec2_backend", "Codec2 与软件解码", "Codec2 后端版本", "单选一个后端，修改后重启生效。", ["codec2"]],
    ["codec2_buffer", "Codec2 与软件解码", "八位缓冲区兼容修正", "AVC / HEVC 停止与排空路径修正；关闭会恢复旧路径。", ["codec2_sdk"]],
    ["av1_native", "十位与 AV1", "AV1 原生输出", "按实际消费者选择显示 / P010 路径；关闭后恢复完整精度复制。", ["codec2_sdk_next"]],
    ["av1_afbc", "十位与 AV1", "AV1 无损 AFBC", "符合条件的显示 Surface 与 64 对齐宽度启用无损压缩；其他路径保留原选择。", ["av1_native"]],
    ["software", "Codec2 与软件解码", "系统软件解码器", "libvpx / libgav1 与完整精度 P010 输出适配。", ["codec2"]],
    ["p010_fast", "十位与 AV1", "十位 SIMD 转换", "关闭后使用标量转换，保留相同十位样本。", ["codec2"]],
    ["hdr_static", "HDR 元数据", "HDR10 静态元数据", "包含既有 HDR / SDR 状态切换适配。", ["codec2"]],
    ["hdr10plus", "HDR 元数据", "Codec2 HDR10+ 元数据桥", "传递标准逐帧元数据，不代表 HDMI HDR10+ 或杜比输出。", ["codec2"]],
    ["libyuv", "像素转换", "libyuv", "系统像素转换与缩放库。", []]
  ];
  const parents = { av1_grain: "mpp", codec2: "mpp", codec2_backend: "codec2", codec2_buffer: "codec2_backend", av1_native: "codec2_backend", av1_afbc: "av1_native", software: "codec2", p010_fast: "codec2", hdr_static: "codec2", hdr10plus: "codec2" };
  const names = Object.fromEntries(definitions.map(([key, , title]) => [key, title]));
  names.codec2_sdk = "后端版本：1.14.3 SIMD 或 1.14.16";
  names.codec2_sdk_next = "后端版本：1.14.16";
  const backendNames = { legacy: "旧后端（0.4.0 路径）", simd: "1.14.3 SIMD", latest: "1.14.16" };
  function backend(components, field) {
    const sdk = components.codec2_sdk[field], latest = components.codec2_sdk_next[field];
    if (sdk === null || latest === null) return "unknown";
    return sdk === 0 ? "legacy" : latest === 1 ? "latest" : "simd";
  }
  const rows = new Map();
  let state = null;
  let busy = false;
  let counter = 0;
  const $ = id => document.getElementById(id);
  const bitLabel = value => value === 1 ? "开启" : value === 0 ? "关闭" : "未知";
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
      if (!window.ksu || typeof window.ksu.exec !== "function") { reject(new Error("请在 KSU WebUI / 独立版 KsuWebUI 中打开此面板，并授予 root 权限。")); return; }
      const callback = "rkDecode_" + Date.now() + "_" + counter++;
      const cleanup = () => { clearTimeout(timer); delete window[callback]; };
      const timer = setTimeout(() => { cleanup(); reject(new Error("读取超时。请刷新确认实际保存状态。")); }, 15000);
      window[callback] = (errno, stdout) => {
        cleanup();
        if (Number(errno) !== 0) { reject(new Error("模块操作失败（" + errno + "），请刷新检查配置和 root 权限。")); return; }
        try { resolve(JSON.parse(stdout)); } catch { reject(new Error("模块返回了无效状态，请检查安装是否完整。")); }
      };
      try { window.ksu.exec(command, "{}", callback); } catch { cleanup(); reject(new Error("无法调用 root 接口，请确认 WebUI 已获授权。")); }
    });
  }
  function render(next) {
    if (next.schema !== 1 || !next.components || definitions.some(([k]) => k !== "codec2_backend" && !next.components[k]) || !next.components.codec2_sdk || !next.components.codec2_sdk_next) throw new Error("模块状态版本不匹配，请重新安装完整 ZIP。");
    state = next;
    $("version").textContent = "模块 " + next.version + " · 当前启动 " + (next.activeKnown ? next.activeEnabled ? next.activeVersion : "原厂组件" : "尚未确认");
    $("whole").setAttribute("aria-checked", String(next.enabled));
    $("whole-state").textContent = "当前：" + (next.activeKnown ? next.activeEnabled ? "启用" : "停用" : "未知") + " · 重启后：" + (next.enabled && !next.removing ? "启用" : "停用");
    for (const [key, , , , deps] of definitions) {
      if (key === "codec2_backend") {
        const row = rows.get(key);
        row.section.classList.toggle("inactive", next.components.codec2.effective === 0);
        row.toggle.value = backend(next.components, "requested");
        row.current.textContent = "当前：" + (next.components.codec2.active === null ? "未知" : next.components.codec2.active === 0 ? "未启用" : backendNames[backend(next.components, "active")]);
        row.effective.textContent = "重启后：" + (next.components.codec2.effective === null ? "未知" : next.components.codec2.effective === 0 ? "未启用" : backendNames[backend(next.components, "effective")]);
        row.reason.textContent = next.components.codec2.effective === 0 ? "版本选择已保留；Codec2 兼容层或整包关闭时暂不生效。" : "";
        row.reason.hidden = !row.reason.textContent;
        continue;
      }
      const info = next.components[key], row = rows.get(key);
      row.section.classList.toggle("inactive", info.effective === 0);
      row.toggle.setAttribute("aria-checked", String(info.requested === 1));
      row.current.textContent = "当前：" + bitLabel(info.active);
      row.effective.textContent = "重启后：" + bitLabel(info.effective);
      let reason = "";
      if (info.requested === 1 && info.effective === 0) {
        const missing = deps.filter(k => next.components[k].effective !== 1).map(k => names[k]);
        reason = !next.enabled || next.removing ? "选择已保留；整包停用时暂不生效。" : "选择已保留；需要先启用：" + missing.join("、") + "。";
      }
      row.reason.textContent = reason;
      row.reason.hidden = !reason;
    }
    if (next.removing) notice("模块已标记卸载，重启后恢复原厂组件。当前禁止修改选择。", "pending");
    else if (!next.valid) notice("配置或组件载荷不完整，已禁止修改分项。仍可关闭整包并重启回退，或重新安装完整 ZIP。", "error");
    else if (next.health === "failed") notice("本次启动检查失败，模块已请求回退。请检查整包状态并重启。", "error");
    else if (next.pendingReboot) notice((next.staged ? "模块更新已暂存。" : "") + (!next.activeKnown ? "尚无本次启动记录。" : "") + "待重启：已保存的选择将在重启后应用。依赖未启用的分项会保留选择。", "pending");
    else notice("配置已应用，无需重启。依赖未满足的分项仍保留原选择。", "ready");
    const health = { passed: "通过", checking: "检查中", failed: "失败，等待回退", unknown: "无本次记录" };
    $("health").textContent = "启动检查：" + (health[next.health] || "未知") + "。此状态不等于所有视频均已通过播放测试。";
  }
  async function update(key, value) {
    if (busy) return;
    if (key && key !== "all" && !names[key]) return;
    if (key === "codec2_backend" && !backendNames[value]) return;
    busy = true; controls();
    try {
      const command = key === "codec2_backend" ? bridge + " backend " + value : key ? bridge + " set " + key + " " + (value ? "on" : "off") : bridge + " status";
      const before = state;
      const result = await exec(command);
      render(result);
      const linked = before && key ? definitions.filter(([k]) => k !== key && k !== "codec2_backend" && before.components[k].requested === 0 && result.components[k].requested === 1).map(([, , title]) => title) : [];
      if (before && key !== "codec2_backend" && backend(before.components, "requested") !== backend(result.components, "requested")) linked.push("后端版本 " + backendNames[backend(result.components, "requested")]);
      $("changes").textContent = key ? "已保存。" + (linked.length ? "已联动开启：" + linked.join("、") + "。" : "") + "重启后应用。" : "";
    } catch (error) {
      state = null; notice(error.message, "error");
    } finally { busy = false; controls(); }
  }
  let group = "";
  for (const [key, category, title, description, deps] of definitions) {
    if (!parents[key] && category !== group) { $("components").append(element("h2", category, "group-title")); group = category; }
    const section = element("section", "", "component"), content = element("div");
    content.append(element("h3", title), element("p", description, "detail"));
    content.append(element("p", "依赖：" + (deps.length ? deps.map(k => names[k]).join("、") : "无其他分项依赖"), "depends"));
    const badges = element("div", "", "states"), current = element("span", "当前：未知", "badge"), effective = element("span", "重启后：未知", "badge");
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
    rows.set(key, { toggle, current, effective, reason, section, children });
  }
  $("whole").addEventListener("click", () => update("all", !state.enabled));
  $("refresh").addEventListener("click", () => update());
  document.addEventListener("visibilitychange", () => { if (!document.hidden) update(); });
  update();
})();
