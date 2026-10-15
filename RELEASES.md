# Release naming / 发布命名

Tags and display titles start with a lowercase `v` and a three-part version:
`v0.5.1 — Decode Stack`, `v0.3.3 — Yamby/mpv plugin`.
Tag 与标题均以版本号开头，不再用 `decode-v…`、`rga-v…` 或日期开头的系统合集 Tag。

Versions belong to the named component. Equal version numbers use component
suffixes in their tags, such as `v0.1.0-rga` and `v0.1.0-codec2`. The suffix
distinguishes components; the GitHub pre-release flag and release notes describe
validation status. Old installation links are updated to the new Release tags.

Historical Git tags are retained as source references; no historical source
commit or installer content is rewritten. Existing Release assets remain on
their original Release IDs while their tag association and download URLs change.
旧 Git Tag 作为源码引用保留；历史提交与附件内容不变，文档使用规范后的下载链接。

| Former release tag / 旧 Tag | Version-first release tag / 新 Tag |
| --- | --- |
| `decode-v0.5.0` | `v0.5.0` |
| `decode-v0.4.0` | `v0.4.0` |
| `system-v2026.10.15` | `v0.3.2-system` — snapshot containing Decode 0.3.2 and Mali 0.2.0 |
| `v0.3.3`, `v0.3.2`, `v0.3.1`, `v0.3.0` | `v0.3.3-yamby`, `v0.3.2-yamby`, `v0.3.1-yamby`, `v0.3.0-yamby` |
| `mpp-v0.1.1`, `mpp-v0.1.0` | `v0.1.1-mpp`, `v0.1.0-mpp` |
| `rga-v0.1.0` | `v0.1.0-rga` |
| `codec2-v0.1.0` | `v0.1.0-codec2` |

The new decoder release is `v0.5.1`. No additional date-based navigation Release
is created. The [download table](DOWNLOADS.md) lists the current packages and
checksums; system modules remain firmware-pinned pre-releases.
