# v0.8.8 正式版源码与构建 · Mali Global 0.4.4

本次正式版按真实启动接口和本机布局判断 root 兼容性，不按 root 版本或官方二进制哈希白名单放行。固件／ABI、原 init 与个人原 boot／候选完整性和回退保护继续保留。普通模块入口不按 root 品牌拒绝；KernelSU／APatch 专用首次绑定后端尚未实现。当前 Magisk 30.7 完成实机安装重启，其他变体仅有离线回归。正式版与已验收接口策略的运行文件逐字节一致，仅版本、文档及发布来源元数据更新。

精确复现公开安装包时，先检出发布标签，使用哈希固定的公开 Mali 0.4.2 基包及与已验收 keeper 相同的 NDK 29.0.14206865：

```text
git checkout --detach v0.8.8
python tools/cold-boot/generate_contract.py --check
python tools/build_mali_boot_policy.py --base <public-0.4.2.zip> --ndk <ndk-29.0.14206865> --work <fresh-dir> --qms-barrier --source-ref v0.8.8
```

构建器核对重新编译的 keeper 是否等于实机验收过的二进制。正式化时 36 个运行文件与已验收接口策略逐字节一致，只修改版本、文档、静态界面缓存标记、来源与校验清单。旧测试版本名仅保留在历史证据中。回归输入中的官方 APK 哈希不被运行时或准备工具作为准入条件读取。[正式包对照与验收](docs/validation/release-0.8.8.json) · [root 兼容边界](docs/root-compatibility.zh-CN.md)。

Decode 0.8.6、客户端 0.3.7 和可选 PanVK Test8.1 保留原字节与原源码标签。Frame Guard 不随本版提供。root APK、固件／链接输入、私有 boot／profile、日志与密钥均不进入公开附件。每台设备仍须独立准备首次绑定并保留回退检查；本次不新增游戏、HDR 或 HDMI 短黑收益声明。

以下为历史版本构建说明；重建旧版请检出相应标签。

仓库 main 的说明会继续更新；精确复现已发布附件请使用其 `OPEN_SOURCE.json` 指向的标签。历史安装包与校验值不随文档更新改写。

<details>
<summary>历史构建与研发记录（请检出对应标签再使用）</summary>

# Release v0.8.7 · Mali Global 0.4.3 boot validation

本版以固件组件检查代替个人整份 boot 哈希：`magisk-mali-global/contracts/boot-firmware.json` 固定内核、DTB 和关键启动契约，保留本机 root 与个人原件／候选完整性校验。允许差异及拒绝边界见 [启动文档](magisk-mali-global/boot/README.md)，真实安装、首次绑定、整包回退及重新启用见 [验收记录](docs/validation/mali-boot-compat-043.json)。未知内核、DTB、root 方案和冲突启动钩子仍拒绝。

Build the formal Mali package from the hash-pinned public 0.4.2 ZIP:

```text
python tools/cold-boot/generate_contract.py --check
python tools/build_mali_boot_policy.py --base <public-0.4.2.zip> --ndk <ndk> --work <fresh-dir> --qms-barrier --source-ref v0.8.7
```

The builder reuses unchanged GPU/KMD/CSF/interface bytes and rebuilds only the boot keeper/policy. It verifies the formal keeper equals the actual tested binary. The full `tools/build_mali_global.py` also supports `--qms-barrier`. Per-device private preparation uses `tools/cold-boot/prepare.py`; its boot/profile must match the chosen keeper. Existing private backup stores are not replaced automatically. Foreign boot changes after activation still block full-image rollback.

Mali 0.4.3 is new; Decode 0.8.6 and client 0.3.7 are carried byte-for-byte from v0.8.6, optional PanVK Test8.1 from v0.6.9. Their original provenance stays intact. Frame Guard is omitted from this release and stays historical. No private firmware, boot images, logs, keys or closed Mali/CSF blobs are included. This release does not add new game/HDR/physical-blackout acceptance.

以下为历史版本的构建和验证记录，当前状态以上方说明及对应验收记录为准。

# Release v0.8.6 / 正式版源码与构建

Release components: Decode Stack 0.8.6, Mali Global 0.4.2, LSPosed 0.3.7. The accepted system refresh policy lives in `magisk-hdr/src/refresh_session.cpp`; `tools/build_refresh_session.py` builds its exact-firmware loader chain and `tools/build_decode_stack.py --refresh-session <component>` integrates it. `docs/validation/decode-refresh-session-086.json` records the tested binary SHA and bounded physical feedback.

Use the existing component builders with hash-pinned private firmware as link/patch inputs. Public installers contain source-built shims, patches and contracts; original SF/HWUI/HWC, services.jar/OAT, boot images, closed Mali/CSF blobs, device logs and signing keys are excluded. Installers generate the firmware variants locally. QMS and private first-binding preparation are independently gated; a release package is not a universal boot adaptation.

The release retains the accepted executable code and regenerates version, bilingual UI, provenance and integrity manifests. FFmpeg build-directory strings are redacted only in readonly configuration data; executable bytes and ELF offsets remain unchanged, and the payload/source contracts are regenerated. `tools/verify_open_source.py` verifies license documents and nested checksums. The source archive includes the local GPL kernel changes and FFmpeg patch/source obligations; external BSP/link inputs remain separate. New source-rate combinations, all TVs and complete HDR calibration are not newly certified.

以下保留历史构建步骤和阶段记录。旧版本“候选／未发布”仅指当时状态，不覆盖上方正式版与当前验收记录。

---

# 当前工作区：Decode Stack 0.7.9 / LSPosed 0.3.6 候选

HWUI 映射及 HWC 模式表查找已集成，完整 HDR 仍锁定不生效；
范围及实机边界见[0.7.9 补充记录](docs/decode-hwui-rgb10-079.zh-CN.md)。
使用 `tools/build_hdr.py` 和现有 Decode Stack 组合器重建，仅以精确哈希固件作为私有
链接输入，不把原 HWUI/HWC 副本放进公开包。LSPosed 可选客户端使用 FFmpeg
`bf4d5037b4`、本地 P010 补丁和静态 dav1d 1.5.3，构建／打包范围及未完成的应用验收
见[说明](lsposed/patches/README.md)。这些是未发布的工作区候选，下方旧发布链接不包含它们。

# Working-tree Mali Global 0.4.2-test1

Selective Rockchip CSF fence metadata initialization backport (`194a7d5a7567395d37e11ffa9014589b83ba8aad`), independently selected at first binding. Full frozen source, original upstream patch, read-only KMD parameter, private boot builder and module WebUI are included. Rebuild with the same Linux 5.10.66 headers / pinned CRC inputs using the existing commands below; prepare a new private boot for the rebuilt KMD and keeper hashes. Installer excludes proprietary Mali libraries, CSF, private boot and device data. See [scope and validation](docs/mali-csf-fence-042.zh-CN.md).

---

# 当前工作区：Decode Stack 0.7.4-test1

本地实验快照选择性回移 Rockchip 输出缓冲配置通知及其配套标志修正，保留原缓冲数量、像素和 fence。来源及新版本实机边界见[专项报告](docs/decode-output-delay-074.zh-CN.md)。安装包验收输入保留原字节，私有日志、媒体和诊断签名材料不进入源码包。

---

# 当前工作区：Decode Stack 0.7.3-test1

本地实验快照新增独立 Gralloc HDR 传递复用项，保持固件／ABI 合约和重启回退。范围与新版本实机验证见[专项报告](docs/decode-gralloc-transfer-073.zh-CN.md)。没有整套替换 Gralloc，也不将本地补丁标成新的厂商 SDK。

---

# 当前工作区：Mali Global 0.4.1-test2 修复

本地未发布快照包含 Zygisk 启动修复、固定上游来源及双 ABI 注入／回退实机结果。已实测安装包保留原字节；范围和限制见[修复验收](docs/mali-zygisk-repair-041.zh-CN.md)。私有 boot 和设备配置不进入源码包。下方旧版本构建说明保留作历史记录。

---

# v0.6.9 optional 应用内 PanVK driver / 可选 应用内驱动

Checkout `v0.6.9` for `panvk/` and `tools/build_panvk.py`. The new driver is
RC3 Test8.1, a documentation/metadata packaging revision of tested Test8.
Its complete original adapter source, strict address-width binary patcher,
one-line Mesa source fix and compiled-engine offline tests are included.
It is **not a complete Mesa rebuild**. Public upstream pins, toolchain,
requirements and exact reproduction commands: [panvk/README.md](panvk/README.md).

四个原安装包沿用原字节和原源码标签：Mali 0.3.0 对应 v0.6.8，其余三包对应 v0.6.7。
重建历史安装包请使用其各自标签，勿把文档变化冒充运行组件升级。新增 PanVK 的运行库
与本地 Test8 一致，构建器校验哈希；无原版应用 APK、私人固件输入、游戏配置或日志。

---

# 未发布候选：Mali 0.4.0-cold1 / Decode Stack 0.7.2-cold2

首次绑定模式默认关闭，使用 `tools/cold-boot/prepare.py` 私下生成匹配当前 root 的
boot。开启此项会写入 boot；原生加载／恢复程序由现有 Mali 构建器编译，显示协调
归入 Decode Stack。镜像和 root 配置不进入源码或安装包，旧版本测试不作为新模式
验收。准备、回退及物理恢复边界见 `magisk-mali-global/boot/README.md`。

# v0.6.8 Mali kernel / CSF integration（历史）

Mali Global is now 0.3.0. Decode Stack 0.6.7, plugin 0.3.5 and Frame Guard 0.1.1
are carried with unchanged bytes and v0.6.7 source metadata. Use v0.6.8 for the new
Mali sources; the older sections below document the unchanged component inputs.
本次新增 Mali 内核／CSF 与中英文 WebUI；其余三包沿用原版本、字节和源码标签。

## Rebuilding the GPL kernel option

`mali-kernel/` contains the complete frozen driver subtree and compatibility changes,
all helper source, exact object list, linker export namespace CRCs, image-derived ABI
contract, matching configuration and generated headers. Original kernel/boot images
are not inputs to this release build. Obtain pristine Linux 5.10.66 source headers
from kernel.org and build its host `scripts/mod/modpost` (GNU host build tools/MSYS2
on Windows; normal host compiler on Linux). The frozen configuration/generated
headers are used by the standalone builder. Do not configure against arbitrary
installed distribution headers or replace the pinned import versions.

```sh
python tools/build_mali_kernel.py --ndk /inputs/ndk --kernel-source /inputs/linux-5.10.66 --modpost /inputs/kernel-host/scripts/mod/modpost --work /work/kernel
python tools/build_mali_kernel_runtime.py --ndk /inputs/ndk --work /work/kernel-runtime
```

On Windows `--host-tools /inputs/msys/usr/bin` supplies modpost's host runtime.
The build checks 394 KMD and 34 handover-helper import versions against the original
ABI contract and maps compiler paths. The module loader additionally verifies the
live original code/layout before installing any handover callbacks. Build success
alone does not establish compatibility with a different firmware.

Then add `--kernel-artifacts /work/kernel --kernel-runtime /work/kernel-runtime`
to the existing `tools/build_mali_global.py` command below. Both native helpers and
GPL modules are compiled from the published files. Proprietary userspace libraries
and CSF are downloaded at installation using the pinned URL/size/SHA256 contracts.
Do not copy those blobs into source archives or public installer ZIPs.

原内核版本仍为 5.10.66；新版可加载模块在内存接管 GPU，不编译或刷写 boot。构建需要
上述 Linux 头、NDK 和主机 modpost 工具，不需要私人内核镜像。运行前仍严格校验当前固件。

---

# 源码与构建说明

本次版本 **v0.6.7**：Decode Stack 0.6.7、Mali Global 0.2.1、LSPosed 插件
0.3.5、Frame Guard 0.1.1。仅补充许可、第三方说明和版本信息，沿用已验证运行代码。
各包的 OPEN_SOURCE.json 标明精确组件版本与源码标签。

仓库：https://github.com/sw1313/rk3588-playback-fix

```sh
git clone https://github.com/sw1313/rk3588-playback-fix.git
cd rk3588-playback-fix
git checkout --detach v0.6.7
```

Release 额外提供项目源码 ZIP 与 SHA256。源码包包含已跟踪源码、补丁、构建脚本、
许可及说明，排除 .git、编译产物、安装包和私有输入。它不是完整 Android BSP：
上游源码需按 [英文说明的精确版本表](SOURCE_CODE.md#public-source-pins-and-modifications)
下载，系统组件还需自行合法取得匹配固件的链接输入。因此不能仅解压源码包就完整
重建所有系统库，也不声称跨工具链逐字节可重现。当前文档版复用旧版二进制，验证
记录明确区分包内字节一致性检查与历史设备播放测试。

## 工具与四包构建

- 公共工具：Python 3.10+、Git、CMake、Ninja、JDK 17；系统库现有构建使用
  NDK 29.0.14206865、API32。libvpx 还需要 POSIX shell、GNU Make。
- 插件：SDK 35、AGP 8.7.3、Gradle wrapper 8.11.1。运行
  `./gradlew -p lsposed :app:assembleDebug :app:testDebugUnitTest`；Windows 用
  `gradlew.bat`。APK 在 `lsposed/app/build/outputs/apk/debug/app-debug.apk`。
  许可自动放入 assets/open-source；Xposed API 仅编译时使用，JUnit 仅用于测试。
  签名私钥不公开，自建包证书不同，不能直接覆盖已安装的发布包。
- Frame Guard：`python tools/package_magisk.py`，无需私有固件输入，版本从
  magisk/module.prop 读取，自动携带开源说明。
- Mali Global：`tools/build_mali_global.py` 编译双架构开源接口、探针、下载器。
  AIDL 用 Build Tools 34.0.0，D8 用 35.0.0；原系统 Binder/C++ 库只作为链接输入。
  闭源 Mali 驱动安装时独立下载、校验，不放入公开包，也不宣称其开源。
- Decode Stack：依次构建 MPP、RGA、Codec2 及两版 SDK 后端、libyuv、软件解码、HDR
  适配组件，再通过 tools/build_decode_stack.py 合并干净的安装前包目录。
  [英文完整命令](SOURCE_CODE.md#decode-stack)给出了所有构建器参数和 libvpx 配置。
  不从设备上已安装的模块目录打包，因为那里可能存在本地生成的原厂文件。

## 系统输入与限制

Decode Stack 0.7.6-test1 在固定新版 Codec2 核心的输出配置通知补丁之后应用
`hevc-native-surface.patch`，为原生 HEVC 显示消费者保留晚发现十位后的 NV15
路径。这是本地 API32 保护条件修正，未更换整个 SDK、强制改变 AFBC 或修改
像素。独立默认关闭分项依赖新版核心与 HDR 适配；专项新验收与回退见
`docs/decode-hevc-native-076.zh-CN.md` 及对应 JSON。新增测试源码可重建，影片、
原始样本／日志、密钥和测试 APK 仍为私有输入，不进入公开交付。

Decode Stack 0.7.5-test1 的 HDR 构建器增加 `plane_probe.c`，安装时由
`overlay-scale.sh` 对完整哈希匹配的原 HWC 私有生成四字节差异的缩放资格变体。
原伴随库与生成变体均不得进入公开源码／安装包。上游提交、选择性范围、许可、
新版本验收和未通过的延迟改善目标见 `docs/decode-hwc-overlay-075.zh-CN.md`、
对应 validation JSON 与 `LICENSES/hwc/`。保留已测 ZIP 原字节，实机结果和法律
补充单独附带；不以文档更新冒充重新验收过的整套新版 HWC 或 VOP2。

Codec2 的 prepared-root 需要 codec2-a12、AOSP Codec2/hardware 头、匹配的
platform-private 头与 vendor32 链接库；具体文件列表在构建器内。它是开发者自行
准备的目录，不是仓库附带的完整 SDK。VNDK 头固定在 UPSTREAM.json 的提交。
RGA 使用 vendor32/vendor64；软件解码需要原系统 APEX 链接库；HDR 需要对应 DRM
UAPI 头与平台链接库。原厂库和 XML 只作本地输入或设备上校验后生成，不上传。
未知固件必须通过独立兼容验证，不能绕过哈希保护直接安装。

MPP/Codec2/libyuv 等来源、精确提交、补丁顺序和构建器见 SOURCE_CODE.md；
各组件 UPSTREAM.json/SOURCE.json 提供机器可读来源。保留上游原始版权、许可、
专利授权和作者名单，不把全部代码重新标成项目作者所有。

## 检查与发布

发布前检查四包开源说明、上游许可、嵌套 SHA256 清单、签名和源码/压缩包隐私。
`python tools/verify_open_source.py` 核查许可覆盖。运行库变化时还需设备解码、
十位精度、HDR、生命周期和回退测试；文档版不虚构新的播放结果。
四个安装包、源码包、清单、验证记录和校验值一并发布，旧包不覆盖。
不得上传个人路径、设备地址和标识、原始日志、账号、私钥、影片或原厂固件库。


0.7.7 对 Lineage-Rockchip e577f69d04d7435a8e4a1088337ff2ed92a3a7e1 剩余 EOS 清理选择性回移，保留 HEVC 限定、默认关闭、重启回退和锁外批量回调。范围及新实机结果见 docs/decode-hevc-eos-077.zh-CN.md；诊断可重建源码在 tools/decode-lifecycle/eos_pending_probe.cpp 和 build_eos_pending_probe.py，不含影片或私有链接输入。


0.7.8 在固定 Codec2 基线上新增本地 MIT NV15→P010 NEON 成对解包，规范头文件和独立边界/精度测试在 tools/pixel-conversion，构建器核对补丁内新头文件。没有新的直接上游回移或整库/SDK 更新；实际 CPU 收益、重启回退及显式 P010 HDMI HDR 既有限制见 docs/decode-p010-pairs-078.zh-CN.md。


Decode Stack 0.8.2-test1 的内部 HDMI 事件组件源码为 `magisk-hdmi-event/src/`（MIT）。使用 `tools/build_hdmi_event.py --ndk ... --runtime <精确私有原库> --hardware-legacy <精确私有原库> --work <新目录>` 构建，将干净包通过 `--hdmi-event` 合入统一模块。原库不打包，只包含源码、新构建辅助库和本地生成配方；不是新版内核／Composer 升级。


Decode Stack 0.8.4 私有候选新增 `magisk-codec2-sdk-next/patches/native-backpressure.patch`，在既有固定 SDK 上配套原生 Surface 输出预取与 MPP 输入背压流控。参考 AOSP V4L2 b/184020290 机制及固定 MPP mpp_put_packet 实现，没有直接套用 V4L2 缓冲常数、提高 watchdog 阈值或伪造 work。test1 实际用户失败，test2 当前用户场景有界通过；test3 收紧复制／tunnel 排除条件，验收单列。源码、独立异步帧数／PTS 检查及当前边界见 docs/decode-native-backpressure-084.zh-CN.md 与 tools/decode-lifecycle/native_backpressure_probe.cpp。不是新 SDK 整库升级或可发布声明。

正在开发的Decode Stack QMS内核核心位于 `decode-kernel/qms/`，按GPL-2.0-only提供，选择性引入Rockchip d0e8f558时序数据和5a89a1cb的VTEM／多帧流程，范围及两项本地数据修正见该目录UPSTREAM.json。新增请求会话／UAPI、MIT HWC前端和启动屏障协调候选；普通构建关闭候选编译门禁。真实硬件绑定、内核端点、显示loader及可安装QMS升级尚未交付；私有Image／ELF／契约及日志不公开。该GPL源码不适用原创适配器的默认MIT授权。

UAPI头文件使用GPL-2.0-only WITH Linux-syscall-note，MIT前端只引用该系统调用头，不引入GPL实现。增量检查入口为`tools/qms/test_session.py`、`tools/build_hdr.py --qms-candidate --qms-frontend-test`及`tools/tests/test_qms_cold_stage.py`；私有输入见各工具help。这些检查不操作真实显示或验收boot；Decode打包器拒绝只有前端而没有真实后端的候选组件。

</details>
