# UniXNU-dijun

面向小米 15S Pro（`dijun`，XRING O1）的 iOS 移植实验项目，目标是运行真正的原版手机版 iOS 用户空间。

**iOS 尚未开机，原 Apple 用户空间尚未执行。本仓库不是可刷写发行版，也不包含可独立完整重建的内核或固件。**

文档：[简体中文进度说明](docs/zh/porting/dijun-source-snapshot.md) · [English status](docs/porting/dijun-source-snapshot.md)。

## 首批公开源码

本快照包含八份经过逐文件审查、已有 BSD-2-Clause-Patent 许可的源码：

- `UniXNU-dijun-ios27/drivers/ArmCpu/`：BSP CPU、GIC 和架构定时器驱动，以及匹配信息和原组件许可。
- `uefi/O1Pkg/Drivers/O1SerialDxe/`：基于 RAM 优先输出接口的 EFI 串口协议驱动。
- `uefi/O1Pkg/Library/O1SerialPortLib/`：预留 RAM 环形输出、需显式启用的可选 UART 镜像，以及日志 ABI 定义。不包含实际采集的日志。

这八份源文件保持逐字节不变，`source-manifest.json` 记录各自的完整 SHA-256 和大小。这是有限的模块源码发布，不是整个私有集成工作区。

## 已验证的实机结果——2026-11-08，第 13 轮

本轮在同一台设备上测试了新构建的真实 arm64e 原生内核，沿连续内核到用户空间路径尝试启动。所选诊断 collection 中，原 v1 AppleImage4 和 corecrypto 子集映射完整，没有未解析引用或缺失符号替身；但该子集**不包含完整 AMFI/Sandbox 安全依赖闭包**。

本轮真实终点为：

```text
Stage83 / error 0xe802
trust_cache_runtime_init → panic("amfi interface not available @%s:%d")
```

panic 调用点根据本轮实际 collection 核准，不是沿用上轮结果。先前的 Image4 非空检查已通过，但这不代表完整 Image4 运行时 ABI 合格，也不代表签名验收成功。

手机正常返回 Android。设备端完整新测试 boot 的 SHA-256 验证通过，另一 boot 槽未变。按测试流程保留本轮测试 boot：`restored=false`、`test_boot_retained=true`。不公开原始设备身份、boot ID、分区备份或采集日志。

本轮未验证：

- 根文件系统挂载，或原 `launchd` / `dyld` 执行；
- AMFI/CoreTrust 签名运行时兼容；
- SpringBoard、可用的 iOS 界面或完整系统启动。

严格的完整早期安全集成尝试仍有 **252 处未解析引用，涉及 214 个独立目标**。这些是保留组件中的静态映射计数，不是实际执行失败次数、进度百分比或剩余时间估计。

## 目标输入与当前源码

- 目标原始输入：iPhone18,3 / v57ap、iOS 27.0.1、构建号 24A446；原 collection 为 Darwin 27 / XNU 13432.2.10。
- 当前原生源码基线：XNU 12377.121.6 / Darwin 25.5，编译为真实 arm64e。没有改版本字符串冒充 Darwin 27；同名符号不代表私有 ABI 兼容。
- 当前私有 32 MiB main-OS RAM 根输入包含原 launchd 和 dyld，但没有完整 dyld shared cache 或 SpringBoard，因此不是完整 iOS 系统根。

## 构建边界

本快照不提供一键构建或刷写命令。

`ArmCpu.cpp` 需要另行集成的 `arm/o1_timer_bridge.h` 和原生 XNU 平台适配。`O1SerialPortLib.inf` 引用了 `O1Pkg/O1Pkg.dec` 中的平台 PCD 声明；本仓库没有包含该混合集成包。集成这些模块需要具备相应许可的 XNU/IOKit 环境，以及明确预留、映射日志 RAM 并提供所列 PCD 的 EDK II 平台包。

完整本地集成仍依赖私有输入。本仓库不提供 Apple 固件、签名二进制、kernel collection、用户空间、dyld cache 或原厂 boot 镜像。获取和使用这些输入须遵守各自适用的许可；本源码许可不授予 Apple 操作系统软件使用权。

## 安全边界与非目标

- 签名、AMFI、CoreTrust 和信任缓存失败仍按真实失败处理；本发布不包含绕过、强制成功的提供者、替换 launchd 或通用缺失符号替身。
- 当前实机实验仅限已获授权设备经核验的现役 boot 槽。不安装 iOS，也不涵盖 GPT、userdata、NV、RPMB、安全存储、vbmeta 或原厂固件写入。
- 不切换槽位，不使用 `fastboot boot` 路线。如果设备的两种传输方式都不可用，无法保证自动恢复。
- Windows/WOA 工作独立维护，不属于本源码快照。

## 上游来源

以下仅为来源引用，不打包上游检出目录或私有产物：

| 项目 | 固定源码版本 |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | `c96b29975645b711dc06bc435c3ad27321ce817c` |
| [ArmCpu](https://github.com/UniXNU/ArmCpu) | `93a5030c5d983d0ef90f0fca93879d6e36ad1ba8` |
| [Apple 开源 XNU](https://github.com/apple-oss-distributions/xnu) | `ac9718fb1af618d5ce8678d0dc6e8a58f252216f` |
| [EDK II](https://github.com/tianocore/edk2) | `0ea22ef04f3bef379ad15cac46fd9ceaa13d1e9a` |

首批快照不再分发 XNU 检出目录及其 APSL 覆盖的修改。上游许可仍适用于各自文件，不被本发布的模块许可替代。

## 许可

这八份模块文件已有 BSD-2-Clause-Patent 声明，或由随附的 ArmCpu 组件许可覆盖。保留现有版权和 SPDX 声明。根目录 `LICENSE` 提供许可条款，不将其他组件的版权声明套到 UEFI 文件上。具体适用范围与排除项见 `NOTICE.md`。
