# dijun 源码快照与测试状态

[English](../../porting/dijun-source-snapshot.md) · [仓库说明](../../../README.md)

## 发布范围

这是面向 Xiaomi 15S Pro（`dijun`，XRING O1）的实验性模块源码快照，不是已支持设备条目、完整内核或固件、安装包或已经开机的 iOS 系统。

本次原样公开八份现有 BSD-2-Clause-Patent 许可源码：ArmCpu CPU/GIC/定时器模块、EFI 串口协议驱动、RAM 优先串口输出库及其元数据和许可。`source-manifest.json` 记录每份源文件的大小与完整 SHA-256。

## 第 13 轮实机测试——2026-11-08

连续内核到用户空间尝试使用了新构建的真实 arm64e 原生内核。所选原 v1 AppleImage4/corecrypto 诊断子集映射完整，没有缺失符号替身；该子集不包含完整 AMFI/Sandbox 依赖闭包。

本轮真实终点为：

```text
Stage83 / error 0xe802
trust_cache_runtime_init → panic("amfi interface not available @%s:%d")
```

调用点已根据本轮实际 collection 核准。先前 Image4 非空检查通过，不代表完整 Image4 运行时 ABI 合格或签名验收成功。

手机正常返回 Android。设备端完整测试 boot 哈希验证通过，另一 boot 槽未变，按测试规则保留测试 boot（`restored=false`、`test_boot_retained=true`）。不公开原始日志、分区备份或设备身份信息。

**本轮未验证原 Apple 用户空间执行、根文件系统挂载、SpringBoard 或 iOS 系统界面；iOS 尚未开机。** 完整早期安全集成仍有 252 处未解析引用，涉及 214 个独立目标。这是静态计数，不是剩余工作百分比。

## 重建与许可边界

目标原始输入为 iPhone18,3 / v57ap、iOS 27.0.1（24A446）、Darwin 27 / XNU 13432.2.10。原生源码仍是 XNU 12377.121.6 / Darwin 25.5，没有改版本字符串冒充目标。

私有 32 MiB RAM 根输入包含原 launchd/dyld，但没有完整 dyld cache 和 SpringBoard。公开快照也缺少独立构建需要的 timer bridge 和平台 PCD 集成，不能单靠本仓库重建完整私有测试镜像。

Apple 二进制、固件、保留的原 Apple 机器码叶函数、corecrypto 实现、私有用户空间、密钥、凭据、设备日志、备份和混合 Windows/WOA 集成均不公开。保留现有版权与许可声明；模块许可不授予 Apple 操作系统软件使用权。

不放宽签名、AMFI、CoreTrust 或信任缓存检查，不使用通用缺失符号替身或替换 launchd 冒充成功。

## UniXNU 文档规范的适用范围

[官方文档贡献规范](https://unixnu.com/zh/contributing/documentation#%E6%96%B0%E5%A2%9E%E9%A1%B5%E9%9D%A2)要求在 **UniXNU 主文档仓库**新增手写页面时同步英文、中文 Markdown，注册导航，并增加正文可达链接。本快照提供互链的双语 Markdown 和 README 入口，但不是该 VitePress 站点，不修改上游 sidebar。

[仓库规范](https://unixnu.com/zh/contributing/repository)及[移植记录规范](https://unixnu.com/zh/porting/records)区分组件仓库和格式 2 设备仓库。真实设备登记需要一致的 `board.toml`、完整配置和通过验证的 `porting.toml`。当前有限模块集不满足该契约，因此不伪造设备记录，不宣称已经进入支持目录、完成设备扫描或获得上游合并。
