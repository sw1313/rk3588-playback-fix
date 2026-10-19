# 直连 END=0 单字段重测：test18

状态：用户实屏反馈“相同或更差”，候选已撤回。设备已重启恢复 test17＋Mali test19，不计为新升级或已修复。

test13 曾在矩阵链路下将 VTEM 数据集 END 改为 0，用户反馈无改善，随后撤回。这个负结果保留。其后才完成直连 E-DDC 修复，并确认、修正了两类会引起原厂重建的软件观察问题。因此本次是对当前直连环境的独立重测，不覆盖原记录。

test17 新采集在 24 次完整记录的转换中未看到 VOP 控制／VACT、GCP 控制或 HDCP2 bypass 变化；用户仍见整幅短黑。它没有证明线端包正常，也没有证明 END 位就是根因，但提供了在没有同期重建的条件下做单字段对照的机会。

## 来源与实际改动

[主线 QMS VTEM 打包 RFC](https://lkml.iu.edu/2609.2/14407.html) 在 QMS 下生成数据集标志 0x86；它仍是 RFC，不能称为已合入的通用修复。[Teledyne LeCroy 的 M42h 用户指南](https://cdn.teledynelecroy.com/files/manuals/m42h_atp_user_guide.pdf) 第 98 页的 QMS 发送端测试示例同样显示 End EM Data Set=0。这是独立设备资料的示例，不代替本机合规测试或接收端线端采样。

本候选只将数据集 END 从 1 改为 0，即 0xc6→0x86。保留包 FIRST／LAST、NEW、QMS_EN、VRR_EN、M_CONST／Next_TFR 顺序、通知帧数、所有实际时序、位深和链路门禁。原生稳态校验同时要求新值，不允许新旧报文混搭。编译结果仅 core 与 native_binding 两个运行对象改变；其余对象相同。

集成于 Decode Stack `0.8.6-qms-test18-ddc1`；Mali Global `0.4.2-qms-test20` 仅增加精确配套识别。原 boot、g18/KMD/DVFS、DDC、声音过滤、Auto/HDR 和 test16 迟到确认修复保留，Frame Guard 持续关闭。

## 验证和撤回条件

实际 C 核心通过 66 组精确周期、738 组切换／恢复、94,464 个稳态采样及错误注入；Android 原生寄存器／生命周期注入检查 54 项、精确配套检查 15 项通过。它们验证修改一致性，不证明物理无黑。

已准备匹配的 test17＋Mali test19 双模块备份与重启回退，不写 boot。用户用同一 Exo 场景判断效果；无改善或出现回归则撤回此字段，保留诊断和已有修复。测试记录限量开启、结束后关闭，不常驻采样。

## 实测负结果及实际回退

用户确认与 test17 相比相同或更差。采集已结束，额度为 0，临时 trace 实例已删除；窗口结束时 QMS 累计完成序号为 48、帧错误为 0。这不改变用户的短黑负结果，也不能把累计序号当作本次窗口内的切换次数。

已从安装前完整备份恢复 Decode `0.8.6-qms-test17-ddc1` 与 Mali `0.4.2-qms-test19` 并实际重启。核对两模块版本、QMS、g18/KMD/DVFS、双 ABI 外部 Zygisk／LSPosed、模块启停标记、组件选择、Auto/HDR 及原 boot 摘要均正常。Frame Guard 保持 disable／paused；启动后边界诊断额度为 0。未操作播放器、截图或刷写 boot。源码 END 同时保持恢复后的厂商值 1；此候选与此前 test13 均记为无改善，不重复试改同一字段。
