# 许可与发布范围 / License and publication scope

## 纳入的模块源码

仅 `source-manifest.json` 中的八份确切路径作为模块源码公开，均保留既有 BSD-2-Clause-Patent 声明或 ArmCpu 组件许可。此次进度同步不扩大模块集，也不修改其字节。

- ArmCpu 保留现有版权与原组件 `LICENSE`。
- UEFI 串口模块保留原 SPDX 声明，不添加未声明的作者身份或错误移用 ArmCpu 版权。
- 根 `LICENSE` 提供现有许可条款，不重新许可被排除的文件，不授予 Apple 操作系统软件使用权。
- 新写的发布文档与清单按 BSD-2-Clause-Patent 作为源码快照的一部分提供。

## 明确排除

不再分发 Apple 固件、IPSW、kernelcache、保留 Apple 机器码、corecrypto 实现、launchd、dyld、共享缓存、签名二进制、分区镜像、设备日志、设备序列号/boot ID、密钥、凭据、私人授权或编辑器/session数据。

没有完成再分发核准的 XNU/APSL 修改、本地集成模板/构建配置/测试产物、vendor下载和混合Windows/WOA源码保持排除。文件扩展名、目录存在或未被秘密扫描命中本身不是许可证明；明确模块清单决定发布范围。

## 状态与证据范围

这是有限模块源码快照，不包含独立完整内核/固件构建流程或已启动的iOS。README与中英文页面提供脱敏实测摘要，不声称公开八份模块可独立复现本地测试镜像。

截至2026-11-11，最新phone36测试仍在原Sandbox初始化的未映射OSDecrementAtomic调用处终止。原Apple用户空间尚未执行。诊断入口、编译/匹配/回归通过或Android返回不是系统启动证明。错误的历史接口映射在新文档中明确撤回，不继续当作等价实现成果。

本地诊断使用fail-stop未映射调用记录及明确的原AMFI早期注册；这些机制不作为正常启动完成或签名验收成功的证据，也没有在本源码快照中打包Apple代码或扩大未核准源码发布范围。

## English summary

Only the eight exact module paths in `source-manifest.json` are included. They retain their existing BSD-2-Clause-Patent notices or ArmCpu component license and remain byte-identical in this update. New status documentation and manifests use the same documentation license; excluded files are not relicensed.

Apple firmware/binaries/machine-code leaves, corecrypto implementation, userspace, partition images, device identities/logs, secrets, private authorizations, unreviewed integration code and Windows/WOA sources remain excluded.

This snapshot is not a complete build pipeline or working iOS distribution. The latest local phone36 test still stops during original Sandbox initialization at an unmapped OSDecrementAtomic call. Original Apple userspace has not executed. Historical mis-mappings are explicitly corrected, and diagnostic progress is not presented as system boot or signature acceptance.
