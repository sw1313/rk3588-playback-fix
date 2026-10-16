# Decode Stack 0.6.4：KSU WebUI 开关面板

在现有 Magisk 盒子上打开独立版 **KsuWebUI**，选择 **RK3588 Decode Stack**，
按需授予 root 权限。无需更换 root 方案。Magisk 自身的模块页不内嵌此面板；
原有总开关、Action 按钮和 root 命令行仍然可用，Action 仍直接切换整包启停。

面板按基础组件、Codec2 与软件解码、十位与 AV1、HDR 元数据分组，包含十四个分项。
开关表示已保存选择；每个分项同时显示“当前”和“重启后”，以及直接依赖。
即使依赖未满足，仍能编辑并保存该分项的选择；依赖恢复后按保存值生效。
暂存升级、整包停用和卸载标记也会显示，卸载标记存在时禁止修改配置。

| 分项 | 直接依赖 |
| --- | --- |
| MPP / VPU、RGA、libyuv | 无其他分项依赖 |
| Codec2 | MPP、RGA |
| SDK Codec2 | Codec2 |
| Codec2 1.14.16、八位缓冲区修正 | SDK Codec2 |
| 系统软件解码器、十位 SIMD、HDR10 静态、HDR10+ 元数据桥 | Codec2 |
| AV1 原生输出 | Codec2 1.14.16 |
| AV1 胶片颗粒修正 | MPP |
| AV1 AFBC | AV1 原生输出 |

依赖判断沿用 `common.sh` 的 `resolve`，WebUI 不自行决定实际启用逻辑。
`webui.sh` 只接受固定状态查询及白名单分项的 on/off，写入调用原来的
`mode-control.sh`，包括升级暂存目录与现有目录间的同步逻辑。不使用 eval、
不提供任意命令输入，不展示原始设备日志、地址或应用路径。

所有操作均需重启，不热替换解码库。恢复原有效选择后，若无更新暂存等其他原因，
“待重启”会消失。依赖暂时抑制的已保存选择不冒充正在运行的功能。
只有 `post-fs-data.sh` 在实际启动装配成功后写入启动记录，安装器不会写入。
旧版没有记录时显示“尚未确认／待重启”；停用后新一轮启动不会沿用旧记录。
启动健康检查状态与播放兼容性分别显示，不把启动通过当作所有视频通过。

整包关闭仍经原逻辑生成 disable 标记，重启恢复原厂库；分项选择不丢失。
分项配置异常时禁止分项修改，但总开关仍可关闭整包回退。
自动健康检查及一次性回退机制保持不动，未修改内核、boot、解码二进制、
Yamby 或 LSPosed，也未降低源位深、分辨率、码率或帧率。

页面 JS、CSS 随 ZIP 离线携带，无远程脚本、字体、统计服务或联网依赖；
使用语义按钮、switch 状态、键盘焦点和窄屏布局。普通浏览器缺少 root 桥时
明确报错并禁用写入，不伪装为保存成功。

## 验证

测试入口：[Android shell 隔离测试](../tools/tests/test_decode_webui.py)、
[浏览器交互测试](../tools/tests/test_decode_webui.cjs)。结构化结果见
[0.6.4 验证记录](validation/decode-0.6.4.json)。

Android shell 测试在固定的 `/data/local/tmp/rk-webui-test064` 临时目录中运行，
仅重定向模块目录字面量；不会把假载荷装到系统。覆盖十四分项开关、依赖抑制、
选择恢复、整包与 Action 回退、旧启动记录、非法参数、更新暂存及同步、卸载拦截。
浏览器测试使用模拟 KSU 回调覆盖界面状态、实际发出的命令、失败处理、键盘和窄屏；
模拟回调结果不等同于真实 Android WebView 的实测，后者在验证记录中单列。

KSU API 兼容不等于整个系统模块已在所有 KernelSU 挂载方案上验证。
此版本沿用匹配固件的 Magisk 安装和解码路径；本次没有重做 0.6.3 全部格式精度矩阵，
历史播放、逐像素验证与已知边界仍见 [0.6.3 报告](decode-av1-063.zh-CN.md)。

实现依据：[KernelSU 官方 WebUI 文档](https://kernelsu.org/guide/module-webui.html)、
[官方 JS 桥](https://github.com/tiann/KernelSU/blob/main/js/index.js)、
[独立版 KsuWebUI](https://github.com/5ec1cff/KsuWebUIStandalone)。
