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
