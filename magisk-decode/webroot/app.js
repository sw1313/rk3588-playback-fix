"use strict";
(() => {
  // KernelSU's documented exec callback contract, implemented without remote dependencies.
  const bridge = "sh /data/adb/modules/rk3588_decode_stack/webui.sh";
  const definitions = [
    ["mpp", "基础组件", "MPP / VPU", "硬件视频解码与 VPU 兼容库，32 / 64 位作为一组切换。", []],
    ["rga", "基础组件", "RGA", "图像转换、缩放与缓冲区处理。", []],
    ["libyuv", "基础组件", "libyuv", "系统像素转换与缩放库。", []],
    ["codec2", "Codec2 与软件解码", "Codec2 兼容层", "保留原系统接口的解码组件与平台适配。", ["mpp", "rga"]],
    ["codec2_sdk", "Codec2 与软件解码", "SDK Codec2 后端", "关闭后回退到旧后端。", ["codec2"]],
    ["codec2_sdk_next", "Codec2 与软件解码", "Codec2 1.14.16", "关闭后回退到独立保留的 1.14.3 SIMD 后端。", ["codec2_sdk"]],
    ["codec2_buffer", "Codec2 与软件解码", "八位缓冲区兼容修正", "AVC / HEVC 停止与排空路径修正；关闭会恢复旧路径。", ["codec2_sdk"]],
    ["software", "Codec2 与软件解码", "系统软件解码器", "libvpx / libgav1 与完整精度 P010 输出适配。", ["codec2"]],
    ["p010_fast", "十位与 AV1", "十位 SIMD 转换", "关闭后使用标量转换，保留相同十位样本。", ["codec2"]],
    ["av1_native", "十位与 AV1", "AV1 原生输出", "按实际消费者选择显示 / P010 路径；关闭后恢复完整精度复制。", ["codec2_sdk_next"]],
    ["av1_grain", "十位与 AV1", "AV1 胶片颗粒修正", "恢复片源携带的颗粒参数，不是额外添加滤镜。", ["mpp"]],
    ["av1_afbc", "十位与 AV1", "AV1 无损 AFBC", "符合条件的显示 Surface 与 64 对齐宽度启用无损压缩；其他路径保留原选择。", ["av1_native"]],
    ["hdr_static", "HDR 元数据", "HDR10 静态元数据", "包含既有 HDR / SDR 状态切换适配。", ["codec2"]],
    ["hdr10plus", "HDR 元数据", "Codec2 HDR10+ 元数据桥", "传递标准逐帧元数据，不代表 HDMI HDR10+ 或杜比输出。", ["codec2"]]
  ];
  const names = Object.fromEntries(definitions.map(([key, , title]) => [key, title]));
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
    if (next.schema !== 1 || !next.components || definitions.some(([k]) => !next.components[k])) throw new Error("模块状态版本不匹配，请重新安装完整 ZIP。");
    state = next;
    $("version").textContent = "模块 " + next.version + " · 当前启动 " + (next.activeKnown ? next.activeEnabled ? next.activeVersion : "原厂组件" : "尚未确认");
    $("whole").setAttribute("aria-checked", String(next.enabled));
    $("whole-state").textContent = "当前：" + (next.activeKnown ? next.activeEnabled ? "启用" : "停用" : "未知") + " · 重启后：" + (next.enabled && !next.removing ? "启用" : "停用");
    for (const [key, , , , deps] of definitions) {
      const info = next.components[key], row = rows.get(key);
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
    busy = true; controls();
    try {
      const command = key ? bridge + " set " + key + " " + (value ? "on" : "off") : bridge + " status";
      render(await exec(command));
    } catch (error) {
      state = null; notice(error.message, "error");
    } finally { busy = false; controls(); }
  }
  let group = "";
  for (const [key, category, title, description, deps] of definitions) {
    if (category !== group) { $("components").append(element("h2", category, "group-title")); group = category; }
    const section = element("section", "", "component"), content = element("div");
    content.append(element("h3", title), element("p", description, "detail"));
    content.append(element("p", "依赖：" + (deps.length ? deps.map(k => names[k]).join("、") : "无其他分项依赖"), "depends"));
    const badges = element("div", "", "states"), current = element("span", "当前：未知", "badge"), effective = element("span", "重启后：未知", "badge");
    badges.append(current, effective); content.append(badges);
    const reason = element("p", "", "blocked"); reason.hidden = true; content.append(reason);
    const toggle = element("button"); toggle.type = "button"; toggle.id = "toggle-" + key; toggle.disabled = true;
    toggle.setAttribute("role", "switch"); toggle.setAttribute("aria-label", title); toggle.setAttribute("aria-checked", "false"); toggle.append(element("span"));
    toggle.addEventListener("click", () => update(key, state.components[key].requested !== 1));
    section.append(content, toggle); $("components").append(section); rows.set(key, { toggle, current, effective, reason });
  }
  $("whole").addEventListener("click", () => update("all", !state.enabled));
  $("refresh").addEventListener("click", () => update());
  document.addEventListener("visibilitychange", () => { if (!document.hidden) update(); });
  update();
})();
