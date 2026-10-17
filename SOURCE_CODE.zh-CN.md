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

# v0.6.8 Mali kernel / CSF integration

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
