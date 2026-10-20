# Root 环境与固件兼容边界

本记录核对 v0.8.7 实际安装包：Decode Stack 0.8.6、Mali Global 0.4.3。没有改写设备 root，也没有把其他 root 的源码检查当成实机验收。

## 普通功能不按 root 品牌或版本号拦截

两个普通安装入口均未要求 `magisk -V = 30700`，也没有要求只能使用 Magisk。安装器仍需要兼容的模块安装接口、root 脚本环境、属性工具和系统文件挂载；KernelSU／APatch 环境是否提供这些能力需按其具体配置核对。部分版本还依赖额外的模块挂载组件，不能仅看到管理器显示“已启用”就算已挂载。

| 功能 | 当前 root 限制 | 当前验收范围 |
| --- | --- | --- |
| Decode 常规解码、缓冲、HDR、系统显示策略 | 不固定 root 品牌或版本；需兼容模块接口、工具、挂载及原固件依赖 | 已有 Magisk 实机记录；KernelSU／APatch 未实机验收 |
| Mali g18 全局用户态 | 不固定 root 品牌或版本；需实际挂载，原 GPU／图形依赖匹配 | 已有 Magisk 实机记录；其他 root 不因名字被普通安装入口拒绝 |
| Mali 配套内核、CSF、DVFS | 还必须匹配真实运行内核及驱动 ABI；不能只看 `5.10.66` 字符串 | 限现有已验收的原厂内核契约 |
| 图形服务前首次绑定 | 当前后端明确依赖已验证的 Magisk 30700 早期启动流程和 root 载荷 | 尚不能宣称其他 Magisk 版本、KernelSU 或 APatch 已支持 |
| QMS 启动配套 | 当前已验收路径依赖配套首次绑定载体和显示初始化顺序 | 不能因普通模块安装成功就视为其他 root 下也已启用 |

## 为什么不能直接删除首次绑定的 root 检查

这项功能会在 ramdisk 中调用 `${MAGISKTMP}/magisk --post-fs-data`，使用 Magisk SELinux 执行域，并依赖该阶段重复调用的行为。它还检查原 init、root 载荷和图形服务启动契约。只删掉数字 30700，或让非 Magisk 环境返回“检查通过”，都不会自动产生正确的早期启动、挂载和恢复流程。

后续其他 Magisk 版本需要核对其启动实现并建立对应适配；KernelSU／APatch 需要各自的早期挂载／服务屏障适配与回退验收。没有这些证据前，只限制依赖该后端的功能，不应新增整个模块的 root 品牌白名单，也不应默认转到旧的运行时驱动交接来冒充首次绑定。

## 同设备、同固件还需区分什么

同一固件镜像刷到不同盒子后，原厂 `/system`、`/vendor` 和 APEX 文件通常相同，因此继续验证这些文件的哈希不会绑定某一台盒子。校验中没有设备序列号、MAC 或 Android ID 白名单。私有编译产物、原 boot 备份与候选哈希按各自设备生成，不要求与作者设备一致。

但 root 工具可能改变内核、ramdisk、启动服务或挂载方式。KernelSU 可能使用不同内核，也可能通过 LKM 等方式提供；APatch 也有自己的内核修改及启动机制。不能按品牌直接断定一定不兼容，也不能仅凭外层固件相同就断定内核 ABI 相同。真实变化触及现有内核契约时仍应拒绝该内核功能；固定地址／指令布局相关检查不能取消。

更换 root 前先关闭首次绑定并重启，确认已回原 boot，再按相应工具的流程更换。启用后被外部工具改写的 boot，当前整镜像回退不会覆盖；不能使用另一人的 boot 或备份来绕过。

## 本次核对位置

- 普通安装入口：`magisk-decode/customize.sh`、`magisk-mali-global/customize.sh`。
- 固件及已知模块配套：`magisk-mali-global/platform-check.sh`、两包的 `contracts/`。
- 首次绑定 root 约束：`magisk-mali-global/boot-stage.sh`、`cold-state.sh`、`tools/cold-boot/prepare.py`。
- 设备本地 ART 产物：Decode 包中的 `components/display_policy/compile-artifacts.sh`；产物及其哈希现场生成，没有把作者设备的 OAT 哈希作为跨设备准入值。

这是一份兼容边界核对，不是 KernelSU／APatch 支持发布。现有正式安装包保持原样。
