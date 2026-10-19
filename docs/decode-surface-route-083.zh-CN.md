# Decode Stack 0.8.3-test2：原生 Surface 路径独立回退候选

本版只处理升级后的 Exo/CCodec 报错回归。新增 `codec2_surface` 分项，默认关闭，
依赖 `codec2`，重启生效。它选择已有兼容路由，并非新的解码器、队列根修复或性能升级。
当前实机状态与接受范围见 [结构化记录](validation/decode-surface-route-083.json)。

## 故障对照与范围

- 0.8.2 的升级核心实际 Exo 长暂停后，输出线程等待 BufferQueue 预取，输入线程
  重试送包，发生 queue watchdog／codec error；旧 LSPosed 客户端也可复现。
- 仅回退到包内 1.14.3_[2] 仍在用户快进后发生送包重试耗尽、错误 `0xe`，不能算修复。
- 整包停用后，本轮用户快进未复现 codec error；保留 MPP／RGA、关闭 Codec2／显示
  升级后也未复现。两次仍有快进回到片头的现象，不把它归为已修复或此次报错根因。

尚需用本候选隔离升级核心与 Codec2 框架／显示适配的影响。不能根据以上有界对照
认定所有片源或全部错误都已根治。

## 路由与回退语义

既有 `CompatRouter` 选择规则为：显式 P010、启用升级的 Surface、或已批准的八位
复制路径进入源代码构建的 quality 核心，其余进入原厂核心。此前模块把
`debug.rkcodec2.surface_upgrade` 直接绑定到整个 `codec2` 开关，无法只回退普通 Surface。
本版保留该旧属性随 `codec2` 开启，另以 `debug.rkcodec2.native_surface_upgrade` 绑定独立
`codec2_surface` 及当前启动记录；兼容服务同时检查这两个标志，只影响非显式 P010 Surface。

关闭本项并重启：非显式 P010 的 Surface 播放恢复原厂解码器，`codec2_sdk_next`、
八位缓冲兼容、显式 P010／HW copy-back、P010 pool、HDR、MPP／RGA 等选择保留。
HEVC 原生 Surface 和 AV1 原生／AFBC 分项会因依赖缺失抑制生效，不谎报为已启用。
显式 P010 仍按既有规则进入升级核心；它的完整精度与 HDR 不应通过降低位深替代。

重新开启本项并重启会恢复升级前本项对应的 quality Surface 路径，也可能恢复已知报错，
因此当前默认关闭且不是稳定版。整包停用并重启仍恢复原固件链路。旧配置首次升级
补入 `codec2_surface=0`，其他已有选择不改写；保存配置不会切换正在播放的解码实例。

本版修改模块控制及兼容服务的原生 Surface 选择条件，并记录实际选中的路由。其余
39 个组件 ELF、全部解码核心和客户端不变；固件／ABI 校验保留。test1 曾错误复用旧
P010 标志，已拒绝作为兼容交付；test2 用独立标志修正该耦合，需重新实机验收。
Frame Guard 按用户要求持续关闭，不自动恢复；不截图／录屏，用户操作播放器。

## 上游核查

本轮实际读取了以下具体提交，并核对现有补丁范围：

- [4a270d77986a](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/4a270d77986a9c487cdcbf5a4c89941377920b0b)：
  引入标准 work 逐帧关联，本地固定 1.14.16 基线已经包含；不能再次计为新增修复。
- [e577f69d04d7](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/e577f69d04d7435a8e4a1088337ff2ed92a3a7e1)：
  本地已选择性采用空输入及真实 EOS pending 处理，其余初始化重构未直接采用。
- [2dc3fb88fa2e](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/2dc3fb88fa2ea0c71810ccdd0dec5fc1e10cb967)：
  卡住 work 的非标准流程回退仍未采用；需区分合法重排序、真实未完成输入和 Surface
  背压，不能用提前归还 work 掩盖此次超时。
- [c5b8620973c7](https://github.com/rockchip-linux/mpp/commit/c5b8620973c7e22224fcc78ad5016740979c2e0a)：
  reset 后重放参数集的 MPP 修复已经包含，不重复回移。
- API32 [C2BqBuffer.cpp](https://android.googlesource.com/platform/frameworks/av/+/android-12.1.0_r1/media/codec2/vndk/platform/C2BqBuffer.cpp)：
  底层忙碌池返回 C2_BLOCKING，已有等待；本地核心封装继续重试。尚无新的背压补丁部署。

本候选没有声称采用上述未回移的上游修正。私有日志、媒体、设备地址、原始固件及
签名密钥不随模块分发；已测 0.8.2 ZIP 原字节保留。
