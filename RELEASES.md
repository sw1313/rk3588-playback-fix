# 项目整套发布规则 / Complete project releases

每个公开 Release 必须附齐四个组件：Decode Stack、Native Mali Global、Yamby/mpv LSPosed 插件、Frame Guard。
项目版本使用 `v主版本.次版本.修订号`，随整套项目更新递增；组件包内保留各自真实版本，未变化的包原样携带。
Release 标题统一为 `v版本号 — RK3588 Playback Fix（完整组件）`。

GitHub Latest 必须指向最近更新的公开整套项目版本。系统组件仍为实验性质，固件检查、验证限制和已知问题写入说明；
项目发布标记不表示系统组件获得通用兼容性或新增画质／功能承诺。

当前公开整套历史为 v0.3.2 → v0.4.0 → v0.5.0 → v0.5.1，对应原发布时间递增。
首次发布时间和历史源码引用不伪造、不重写。v0.3.2 的历史 Tag `v0.3.2-system` 为避免覆盖原引用而保留。
旧独立组件 Release 作为草稿归档，原附件、Release ID 和所有历史 Git Tag 均保留。

All public releases contain all four installers. Project versions identify complete snapshots; component versions remain independent.
Latest identifies the newest complete project release. Experimental firmware-specific system components keep their validation limits.
Historical standalone releases are archived as drafts, with assets and original source tags retained.

当前安装包、校验值和完整版本时间表见 [DOWNLOADS.md](DOWNLOADS.md)。
