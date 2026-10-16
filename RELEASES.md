# 项目整套发布规则 / Complete project releases

每个公开 Release 必须附齐四个组件：Decode Stack、Native Mali Global、Yamby/mpv LSPosed 插件、Frame Guard。
项目版本使用 `v主版本.次版本.修订号`，随整套项目更新递增；组件包内保留各自真实版本，未变化的包原样携带。
Release 标题统一为 `v版本号 — RK3588 Playback Fix（完整组件）`。

GitHub Latest 必须指向最近更新的公开整套项目版本。系统组件仍为实验性质，固件检查、验证限制和已知问题写入说明；
项目发布标记不表示系统组件获得通用兼容性或新增画质／功能承诺。

当前公开整套历史为 v0.3.2 → v0.4.0 → v0.5.0 → v0.5.1 → v0.6.3 → v0.6.4 → v0.6.5 → v0.6.6，对应原发布时间递增。
首次发布时间和历史源码引用不伪造、不重写。v0.3.2 的历史 Tag `v0.3.2-system` 为避免覆盖原引用而保留。
旧独立组件 Release 已从发布列表移除，不保留会出现在仓库拥有者列表中的归档草稿。
原说明及全部 20 个附件集中保存于 v0.3.2 整套版本的[历史备份 ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.2-system/historical-standalone-components.zip)，所有历史 Git Tag 保留。

All public releases contain all four installers. Project versions identify complete snapshots; component versions remain independent.
Latest identifies the newest complete project release. Experimental firmware-specific system components keep their validation limits.
Historical standalone release entries are removed, including owner-visible drafts. All 20 original attachments and release notes are preserved in the historical backup ZIP attached to v0.3.2-system; original source tags remain.
Before publishing, verify the authenticated release list as well as the public list, and explicitly select the newest complete release as Latest.

当前安装包、校验值和完整版本时间表见 [DOWNLOADS.md](DOWNLOADS.md)。
