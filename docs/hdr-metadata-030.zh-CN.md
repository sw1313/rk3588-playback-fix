# 0.3.0 HDR 元数据实现与验证范围

适用范围：已验证哈希的 RK3588、Android 12/API32、原 5.10.66 内核。
只改变统一解码 Magisk 模块。Yamby 原生组件、LSPosed 0.3.3、GPU、刷新率修正和
SELinux 状态均保持原样。自动测试结果见 `magisk-decode/VALIDATION.json`；
用户已确认 0.3.0 下 HEVC 十位播放正常；用户暂无 HDR10 视频，
因此 HDR10 暂仅有独立测试片的自动输出验证，不记为用户实播通过。

## HDR10 静态信息如何传递

原厂的 HDR/PQ 输出可以切到十位 BT.2020，但 HWC 没有完整传出母版和亮度数值。
本版通过以下用户态链路补齐已验证的直接 Surface 路径：

`MPP → Rockchip Codec2 → 标准 gralloc SMPTE2086/CTA861.3 → 原 HWC 适配层 → HDR_OUTPUT_METADATA → HDMI 静态 InfoFrame`

Codec2 将每帧静态信息写入兼容缓冲的标准字段；没有信息时写入显式零结构，避免缓冲
复用带入前一段视频的数值。不分配像素缓冲，不做颜色转换、降位深或额外图像复制。

HWC 适配层调用设备上原版 HWC；安装时检查其精确哈希，再克隆到模块私有路径。
发布包不包含原厂二进制。适配层仅在原 HWC 选择 PQ 时补入静态数值，保留原 EOTF、
图层、格式、fence 和合成策略。它复制原 DRM atomic request，把静态 blob 加进同一次
原子事务；若新增事务被拒绝，重试未改动的原请求。不会直接写 HDMI 输出寄存器。

适配层改变本进程三个原 HWC 的导入指针：atomic add、commit、free；数据页写权限
临时开启后立即恢复，不修改可执行指令或内核。TEST_ONLY 请求的切换状态保留到正式
提交，释放请求时清理；清空 blob 的 SDR/禁用请求也会识别。仅对已验证单一输出路径
启用；多个活动 connector 时透传原 HWC，避免把主显示元数据写到另一显示器。

测试包括 1000 nit / 4000 nit 两组不同母版、MaxCLL/MaxFALL、返回原视频、没有母版
的 PQ 视频和退出播放。检查 DRM blob 与完整 HDMI InfoFrame 的字段及校验和，
并核对 MediaCodec 的 `hdr-static-info`。原内核 debug/status 的静态数值读取偏移一字节，
因此以完整寄存器包解码为准。寄存器验证不是外置 HDMI 分析仪的线上测量。

原显示栈在不销毁 Surface 的 HLG↔PQ 切换中会保留先前 EOTF；已关闭适配层、恢复
原 HWC 对照复现。0.3.0 保持这一原策略，不把它宣称为已修复。后续的受限直接硬解修正见
[0.3.1 切换修正](hdr-transition-031.zh-CN.md)。新建 Surface 的 HLG、
PQ 与 SDR 输出另行测试。应用若将视频渲染到自己的纹理／输出缓冲，仍需要自己传递
正确 dataspace 和 HDR 元数据；本版没有修改 mpv/Yamby 的渲染组件。

## 标准 Codec2 HDR10+ 桥

HEVC MPP 为 HDR10+ 保存的是去掉六字节 T.35 前缀后的数据。本版按实际结构
`format, size, data` 读取，恢复 `b5 00 3c 00 01 04` 前缀，并校验 ST2094-40 载荷结构
和长度上限。无效、超长、截断或不支持的载荷会被拒绝；不会把其他类型伪装成 HDR10+。

每帧通过 `C2StreamHdr10PlusInfo::output` 的 C2Buffer info/configUpdate 进入标准
MediaCodec/BufferQueue 路径。没有动态信息的帧也明确提供空值，避免沿用上一帧。
本机 mapper 无法可靠清空可变长 SMPTE2094_40 缓冲字段，所以不向那个独立字段写入。

基础合成流的三个源 SEI 在 MediaCodec 恰好三个输出帧上逐字节一致，其他九帧为空。
另有暂停、seek/flush、EOS 和启停对照，以及 20,000 次有界随机输入／边界验证。
这证明标准解码元数据桥；原 HWC 没有相应逐帧输出接口，内核缺少新版动态 HDMI 属性，
因此 **不代表电视收到 HDR10+ HDMI 动态信令**，也不伪造系统支持标志。

## 开关与回退

在 root shell 使用，改完重启：

```sh
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr_static off
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr10plus off
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr_static on
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr10plus on
```

两个开关相互独立，均依赖有效 Codec2；Codec2 依赖 MPP 和 RGA。依赖停用时只暂时
抑制相关功能，不抹掉保存的选择。总开关/Action 或卸载后重启恢复原系统文件。
本机 HWC 可能早于 Magic Mount 启动；启用静态适配时，启动检查必要时会重载一次
composer、SurfaceFlinger 及框架消费者。关闭静态适配后不做该重载。安装/启动失败
保护会停用整包并重启一次，禁止恢复循环；不能代替对所有视频的播放测试。

## 画质与范围

14 项常用编码与 7 项软件解码播放均通过暂停、跳转和 EOS。14 张硬解套件画面与
已验证 0.2.0 完全一致；4,147,200 个可见 P010 样本逐点相同，最大误差 0，
其中 1,884,604 个样本的低两位非零。整包关闭重启恢复 31 个原始文件，
重新开启冷启动核验 19 个挂载文件。

全部画质检查以像素为依据，元数据桥本身不改变像素。P010 的可见十位样本保留低两位；
不将十位内容强制转八位，不缩分辨率、不压码率、不改变色调映射或强开增强。
软件 HDR 即便解码缓冲为 P010，原 HWC 的客户端合成仍可能使用八位 AB24；这是原有
显示链路限制，不能用解码精度测试代替端到端十位输出。优先使用可用硬解直接路径。
未知媒体、DRM/安全视频、多屏、摄像头、长时间和热稳定性没有全部验证。
