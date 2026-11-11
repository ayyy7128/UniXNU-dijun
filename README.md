# UniXNU-dijun

面向小米 15S Pro（`dijun`，XRING O1）的 iOS 移植实验，目标是执行真正的原版 iPhone iOS 用户空间。

> **截至 2026-11-11，第 36 轮实机测试仍停在原 Sandbox 安全扩展初始化。原 Apple `launchd` / `dyld` 用户代码尚未执行，iOS 尚未开机。UniXNU 菜单不是 iOS 界面。**

本仓库是有限的源码模块与进度记录，不是可刷写发行版，也不能独立重建完整测试内核或固件。

文档：[简体中文进度与纠错记录](docs/zh/porting/dijun-source-snapshot.md) · [English status](docs/porting/dijun-source-snapshot.md)。

## 当前实机位置

最新已完成的是 **第 36 轮（phone36）**：

- 原 Apple 安全扩展代码继续在自建 arm64e 内核上执行，当前可定位的终点在 **Sandbox 初始化**。
- 已越过此前的 AMFI 属性缺失检查，以及 SecureDT、系统调用过滤掩码数量、VM 页掩码和原子增量的阻塞调用点。
- 最后一次记录为 `Stage83 / error 0xe802`，诊断捕获到 Sandbox 调用尚未映射的 **`OSDecrementAtomic`**。Stage83 是故障捕获返回编号，不是完成了第 83 个启动阶段。
- 同台设备正常返回 Android；完整已安装测试 boot 的 SHA-256 验证通过，未写 boot 槽保持不变。本轮测试 boot 保留，`restored=false`、`test_boot_retained=true`。

**尚无原 iPhone 用户空间执行、SpringBoard 或 iOS 系统界面的证据。**

当前保留组件的匹配仍不完整：**306 个不同的未映射目标，492 个组件级目标条目**。这是第 36 轮候选的静态计数，不是失败次数、进度百分比或工期估计；与早期使用较小闭包的统计不能直接比较。

## 本轮关键修复

### 诊断与接口契约

- 修复 panic 日志先追加 PC/LR、后检查长度的越界：实际破坏了恢复所需的启动参数和状态。新捕获器只识别已核准的完整异常格式，写入前预留容量，保留原可读页检查及恢复 ABI。主机模型的 GCC / Clang 普通版与 ASan / UBSan 版各通过 664 个案例；主机测试不是 ARM 或 Apple 用户代码执行证据。
- 更正 `bdevsw_isfree` 被误认为注册函数的映射，撤回 NULL→全零设备开关替身；改用真实只读空槽查询。
- 更正原 NULL-safe `safeMetaCast` 被误绑为不检查 NULL 的成员 `metaCast`，使用受保护入口转调真实静态接口，保留类型/PAC 检查与借用引用语义。
- 撤回固定零“类分派”替身：原函数实际是 `code_signing_configuration`。改用完整核准的真实原生平台配置查询，不伪造 TXM，不隐藏豁免位。
- 收紧重定位回退校验，禁止以地址布局差异为理由接受 PAC→NOP、算术、条件分支或错误路径变化。

### 已核准的后续调用

- 原设备树 `/chosen/amfi-only-platform-code` 的 4 字节输入取自正确 iPhone 固件模板，校验了官方清单摘要；不把模板当作运行中 iPhone 的安全状态，也不复制 production、安全域或证书占位字段。
- SecureDT 接口转调真实原生设备树访问器，保留节点范围、溢出和失败返回。
- 系统调用、Mach trap 和 kobject 过滤掩码尺寸取自真实表与正常 MIG 初始化，不填造常数。
- VM 页掩码与原子增量保留真实 map 对象域、内存序和旧值返回。

错误的历史候选保留为独立记录，不能当作等价实现或启动成功；发现不合格的本地候选明确禁止上机。本轮更新不会把先前的接口误认继续写成成果。

## 仍需完成

1. **内核与安全扩展 ABI 闭合**：当前先处理 `OSDecrementAtomic`，之后继续按实际最早阻塞核准对象、回调与生命周期。许多未映射目标已有原生实现，但同名不代表可直接绑定。
2. **完整签名运行时契约**：AMFI / TrustCache signed-load ABI、immutable runtime `+40` 回调生产者等仍未证明。早期注册运行不等于完整安全链验收。
3. **完整原 iOS 系统根及用户空间**：当前 32 MiB main-OS RAM 根是最小原 launchd/dyld 输入，不是完整 dyld shared cache、服务或 SpringBoard 系统。内核/root/exec/EL0/UI 必须分别证明。
4. **iOS 侧硬件与图形服务**：显示、触摸、存储及其他驱动仍需在 iOS 内核/服务侧闭合；引导器能画菜单不证明 iOS 图形栈可用。

## 诊断模式与安全边界

当前实机路径是明确的 **bring-up 诊断模式**，保留完整 AMFI / Sandbox / Image4 / corecrypto 依赖闭包，但不是已经映射完整的正常启动候选。

本地诊断使用 UniXNU 的未映射调用报告入口：遇到未核准调用立即记录并终止，不返回固定成功；同时在明确位置提前执行原 AMFI 注册例程。**这些诊断机制不是正常系统启动或安全验证成功的证据。**

- 不修改原 Apple 用户空间或原安全扩展代码，不关闭签名、AMFI、CoreTrust 和 TrustCache 检查，不使用替换 launchd / MockFS 冒充启动。
- 正常启动模式仍要求完整映射、无 reporting stub / 提前安全 initializer，并核准完整 provider ABI。
- 本轮设备操作仅限授权设备的经核验现役 boot 槽；不改 GPT、userdata、NV、RPMB、安全存储、vbmeta 或其他原厂分区，不切槽、不使用 `fastboot boot`。
- 双 transport 失联停止自动写入，等待手动 fastboot。诊断日志曾引入的两次挂起和后续恢复不被隐藏；不能承诺任意故障都可自动恢复。
- Windows / WOA 工作独立维护，不纳入本快照。

## 公开源码范围

本次仍包含八份已有 BSD-2-Clause-Patent 许可、逐文件审查的模块：

- `UniXNU-dijun-ios27/drivers/ArmCpu/`：BSP CPU、GIC、架构定时器驱动及匹配信息、原组件许可。
- `uefi/O1Pkg/Drivers/O1SerialDxe/`：RAM 优先 EFI 串口协议驱动。
- `uefi/O1Pkg/Library/O1SerialPortLib/`：RAM 环形输出、需显式启用的 UART 镜像与日志 ABI 定义。

八份模块源码本轮未变，`source-manifest.json` 记录其原始大小与 SHA-256；本次同步重点是最新双语状态与纠错说明。未核准再分发的集成补丁、原 Apple 机器码/二进制、固件、用户空间、设备日志/身份、凭据与分区备份不上传。

这些模块依赖另行集成的 XNU / IOKit、timer bridge 和 EDK II PCD 环境；本仓库不提供一键构建或刷写命令。许可和排除项详见 [NOTICE.md](NOTICE.md)。

## 目标与来源

- 原目标：iPhone18,3 / v57ap，iOS 27.0.1（24A446），Darwin 27 / XNU 13432.2.10。
- 最新本地原生源码：XNU 12377.121.6 / Darwin 25.5，真实 arm64e 构建；不修改版本字符串冒充目标。
- 上游：[UniXNU](https://github.com/UniXNU/UniXNU)、[ArmCpu](https://github.com/UniXNU/ArmCpu)、[Apple 开源 XNU](https://github.com/apple-oss-distributions/xnu)、[EDK II](https://github.com/tianocore/edk2)。
- 参考规范：[文档贡献](https://unixnu.com/zh/contributing/documentation)、[移植记录](https://unixnu.com/zh/porting/records)。本仓库不是已验证的格式 2 支持设备记录，也不声称已获上游合并。

## 许可

保留八份模块原有版权和 BSD-2-Clause-Patent 声明。根 `LICENSE` 不覆盖被排除的 XNU/APSL 或 Apple 软件输入，不授予 Apple 操作系统软件使用权。
