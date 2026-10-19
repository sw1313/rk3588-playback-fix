# Decode Stack 0.8.4-test3：HEVC 原生输出背压候选

这是尚未通过实际 Exo 验收的私有候选，不能发布或标记根治。test1 已在实际用户暂停／快进后失败（送包耗尽及 codec 0xe），普通 Surface 回退也仅作隔离，不能代替根修。

## 故障和具体来源

新版 1.14.16 的故障栈中，输出线程持缓冲锁等待额外 Surface 缓冲，输入线程无法继续送包；长暂停后可触发 queue watchdog。退到包内 1.14.3 也有送包耗尽错误；原厂 Codec2 本轮对照未见相同报错。不能把旧客户端当原固件对照。

[AOSP V4L2 固定提交](https://android.googlesource.com/platform/external/v4l2_codec2/+/f28937be2f3832ed7f6be9412ff222315e49f42b/v4l2/V4L2DecodeComponent.cpp) 记录 b/184020290：输出缓冲耗尽早于输入背压会触发 queue 超时。这里只参考机制，没有套用 V4L2 的缓冲常数或增加 output delay。

已核对 [当前固定 MPP 的 mpp_put_packet](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/mpp.c)：无输入 task 返回 MPP_ERR_BUFFER_FULL。旧 SDK 用 mBufferLock.tryLock() 失败间接识别暂停；改成非阻塞预取后，该间接判断不再成立。test1 实机日志正好出现输出池 C2_BLOCKING 与输入重试耗尽并存；本地配套候选仍需实机证实。

固定 Codec2 基线 e3c30156793f778541b6eaec54ac2a1ac0f6a70c 保留，新增 native-backpressure.patch 为本地修正，不冒称上游直接回移。

## 模块行为和回退

codec2_native_backpressure 默认关闭，依赖 codec2_sdk_next、codec2_surface 和 codec2_lifecycle，重启生效。

1. HEVC 非 tunnel 原生输出在返回真实帧后，以一次原平台池调用补缓冲；C2_BLOCKING 时延迟重试，输出线程可继续返回已解码帧。单个重试在途，flush／stop 的 generation 作废旧消息。
2. test2 记录实际平台池背压状态；仅当 MPP_ERR_BUFFER_FULL 与该状态同时成立时等待 Surface 恢复或 flush。真实 MPP 错误仍失败，原 CCodec watchdog、非背压时的原重试预算保留。没有伪造输入接收或提前完成 work。
3. 原 dequeue、acquire fence、导入和所有权管理保留；初始化、info-change、复制输出、tunnel 保持原流程。未降低位深、分辨率或帧率。
4. 关闭此项并重启恢复此前取缓冲及送包流程，其它精度／HDR 项保留。codec2_surface 关闭的原厂路由只是临时诊断回退。整包停用并重启恢复原固件。

只更换 next-core，另外 39 个组件 ELF 不变。固件／ABI／HIDL 检查保留，不写 boot，不改 Mali 或 Zygisk。

## 实际验证边界

- 新开关 Android 隔离控制检查 32 项、WebUI 浏览器检查 81 项通过；没有截图或修改播放器输入。
- test1：原生异步合成场景持有 31 个输出索引 8 秒，恢复后 150/150 帧、精确 PTS、EOS、零错误；实际 quality／原生 composer／背压分支均确认。
- test1 关闭项并重启的同源对照也通过（25 个持有索引、150/150 帧）。该场景未复现原故障，不能记为修复收益。
- test1 实际用户入口失败：首次反馈无可见报错后又报错，日志有 MPP 送包耗尽／codec 0xe。未捕获 queue watchdog 不代表根治。
- test2：编译与安装前 ABI／HIDL 检查已通过；实际 Exo、长暂停恢复、seek／flush、EOS、分项回退及完整 HDR 本版仍待验收。

Frame Guard 全程关闭，实际播放器交互由用户执行；不截图、录屏或远程点击。私有日志、媒体、设备地址、原固件及签名不进包。

## 另外两项

快进回开头：递归 SeekHead 的上游讨论与本片索引结构吻合，待既有 LSPosed 集成真实 Cues 解析，详见 decode-seekhead-repair-candidate.zh-CN.md。短暂黑屏：HDMI 模式重协商和端到端 QMS 仍待修。两项均未修复，不以本候选替代。


## test2 用户复测与 test3 范围修正

用户按原 Exo 场景暂停／恢复并快进后反馈“无报错，播放可恢复”；本轮约 122 秒日志无 codec error／queue watchdog，实际观察到 MPP_ERR_BUFFER_FULL 及候选等待分支。只记 test2 的有界通过，不称长期根治或将其它两项记为修复。

test3 的唯一行为差异是输入侧也使用原生路径资格判断（排除 mBufferMode 和 tunnel），与输出侧一致。普通原生 Surface 的处理逻辑不变，复制输出和 tunnel 的送包流程明确保留。test3 尚需新版本启动／回退、原生输出及用户场景检查，不能冒用 test2 完成验收。
