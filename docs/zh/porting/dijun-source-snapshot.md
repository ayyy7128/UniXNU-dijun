# dijun 源码快照与最新实机状态

[English](../../porting/dijun-source-snapshot.md) · [仓库说明](../../../README.md)

更新：**2026-11-11，最新已完成第 36 轮实机测试（phone36）**。

## 当前结论

**iOS 尚未开机。当前仍停在原 Sandbox 初始化，没有原 Apple `launchd` / `dyld` 用户代码执行、SpringBoard 或 iOS 系统界面的证据。**

UniXNU 图形菜单属于引导器。编译、预链接、检查点通过或正常返回 Android 不构成 iOS 系统启动证明。本仓库也不是即刷即用的 ROM、安装包或上游已支持设备条目。

## 最新实机终点

第 36 轮使用新构建的真实 arm64e 原生内核，保留原 AMFI / Sandbox / Image4 / corecrypto 依赖闭包，并明确运行于 bring-up 诊断模式。

当前记录为 `Stage83 / error 0xe802`，未映射调用报告位于原 Sandbox 的 **`OSDecrementAtomic`** 调用。原函数应进行 32 位原子减计数并返回减之前的旧值；这个目标目前没有进入本轮候选的合格映射。

Stage83 是故障捕获后的终态返回编号，不是“83 个阶段已经完成”。停止点变化必须按实际调用目标和同次启动日志判断。

- 同台设备正常返回 Android，启动完成标志通过。
- 完整已安装测试 boot 的设备端 SHA-256 与候选一致，未写 boot 槽未变。
- 按测试规则保留测试 boot：`restored=false`、`test_boot_retained=true`。
- 原镜像备份保留用于异常恢复，但不把“可以恢复”写作“已经恢复”。

本次不公开设备序列号、boot ID、原始 console/pstore、分区备份或测试镜像。

## 从已定位故障到当前阻塞

| 实机轮次 | 实际观测与处理 | 未完成边界 |
| --- | --- | --- |
| phone28 | 有界 panic 捕获获得真实数据中止原因，定位原 NULL-safe `safeMetaCast` 被误绑为不检查 NULL 的成员 `metaCast` | 不能将形状相似当作 NULL 语义等价 |
| phone30 | 修正 safeMetaCast，并删除固定零代码签名配置替身；原 AMFI 明确拒绝缺失的 `amfi-only-platform-code` 属性 | 必须查原输入来源，不能猜策略值 |
| phone31 | 从官方清单校验的原 v57ap DeviceTree 模板只运输这一 4 字节属性，越过缺失属性点 | 模板不证明运行中 iPhone 的安全状态；SecureDT 调用尚未绑定 |
| phone32 | 加入受保护的真实 SecureDT 查询/属性边界，日志前进到原 Sandbox 的过滤掩码数量查询 | 不宣称全部 AMFI 或 Sandbox 初始化完成 |
| phone34 | 查询真实 BSD 系统调用、Mach trap 与 MIG kobject 数量，日志前进到 VM 页掩码查询 | 计数来自真实表/正常 MIG 初始化，不填造常数 |
| phone35 | 复用真实 VM 页掩码边界并核准原生 `kernel_map` 输入，日志前进到原子增量 | 不复用原私有 map 字段偏移 |
| phone36 | 受保护入口调用真实 `OSIncrementAtomic`，保留 relaxed 内存序和旧值返回 | 当前阻塞是下一处 `OSDecrementAtomic` 未映射调用 |

上述各轮正常返回均有独立本地镜像哈希与同次启动记录。不能把仍有失配的历史候选或尚未上机的本地包称作成功验证。

## 撤回的历史误认及可靠性修复

### 日志越界

先追加 PC/LR 后缀、后检查长度的旧捕获器会越过 128 字节文本区，破坏恢复记录之后的启动状态，曾导致两次需要手动 fastboot 的挂起。已在冻结源码上修复，保持原可读页策略、参数 ABI 与恢复记录格式。

修复后的真实函数在显式 Darwin cursor 主机模型中，由 GCC / Clang 普通版和 ASan / UBSan 版分别测试；四个版本各 **664 个案例通过**，覆盖长格式、NULL/不可读 cursor、完整格式逐字节变异与恢复状态 canary。**这是主机边界测试，不是 ARM/PAC 运行或 Apple 用户空间执行证据。** 随后的 phone28 实测正常返回并捕获确切原因。

### 设备开关接口

原只读 `bdevsw_isfree` 曾被误认为 `bdevsw_add`，NULL→全零开关替身会改变注册和失败语义。已撤回该映射，改用真实只读空槽查询，保留有效 PAC 错误回调哨兵以及另一条真正注册调用。

### NULL-safe 类型转换

原静态 `safeMetaCast` 的 NULL 分支不能用成员 `metaCast` 替代。新受保护边界转调真实静态 API，返回借用指针或 NULL，没有 retain/release，不弱化类型认证。

### 代码签名配置

原函数并非“类分派 no-match”；它读取、缓存并发布代码签名配置。固定写零没有等价性证据，已从新内核删除。真实原生配置查询完整核验，不伪造 TXM 平台、production 状态或隐藏豁免位。原/原生平台差异明确保留，不宣称全输入私有 ABI 等价。

### 重定位证明

负测发现旧回退在布局变化时可错误接受 PAC→NOP。现已收紧为只能接受有确切目标身份的地址立即数重编码；改变 PAC、算术、条件分支或错误路径必须拒绝。需要中间基线时保留两段完整函数/目标证明，而不是跳过检查。

phone29 与 phone33 是在本地审查中被禁止上机的候选，不计作实机成功。旧记录不删除，不能继续以旧准入结果刷写。

## 静态闭包与正常启动准入

第 36 轮候选仍有 **306 个不同未映射目标、492 个组件级目标条目**。同一目标被多个组件引用时会产生多个条目；这不是执行失败次数或剩余工作百分比。

早期公开的第 13 轮使用较小 AppleImage4/corecrypto 子集，完整安全集成当时的静态计数为 252 处引用／214 个目标。闭包范围与计数单位已经不同，不能用两个数字推算进度或倒退。

当前明确区分：

- **bring-up 诊断**：完整早期安全组件闭包仍可能缺少映射；官方未映射调用报告入口记录并终止，不返回固定成功；明确位置执行原 AMFI 早期注册。
- **正常启动候选**：必须完整匹配、无 reporting stub 或提前 initializer，并核准完整安全 provider ABI。当前不满足这一条件。

原安全扩展代码、签名检查和正常错误路径保留。提前注册运行、TrustCache 某个早期调用返回或一次诊断前进，不证明 signed-load ABI、CoreTrust 验收或正常安全启动完成。

## 还差什么

1. 当前 `OSDecrementAtomic`，随后按实际最早阻塞继续闭合内核接口、对象/回调/PAC 与生命周期，不能仅凭名称绑定。
2. AMFI / TrustCache 完整签名加载和运行时 ABI，特别是 immutable runtime `+40` 回调生产者等未证明项。
3. 完整原 iOS 系统根、dyld shared cache、实际 PID1/exec/EL0 与后续服务。现有 32 MiB RAM 根是最小原 launchd/dyld 输入，不是完整系统。
4. iOS 侧显示、触摸、存储、图形与其他硬件服务；引导器画面不代表 iOS 图形栈工作。

不承诺剩余几轮或工期，分别记录内核、root、exec、原用户代码和 UI 的真实证据。

## 本次 GitHub 同步与许可范围

仍仅原样发布 `source-manifest.json` 中八份现有 BSD-2-Clause-Patent 模块：ArmCpu、EFI Serial I/O 和 RAM 优先输出库。模块字节本轮未变；本次同步更新中文 README、双语进度与纠错说明。

未核准再分发的 XNU/APSL 修改、私有集成模板/工具、固件/二进制/机器码、用户空间、设备日志/身份、凭据、分区备份及 Windows/WOA 集成保持排除。公开快照缺少独立构建完整测试镜像所需的 timer bridge 和平台 PCD 集成，不提供一键构建或刷写命令。详见 [NOTICE.md](../../../NOTICE.md)。

## UniXNU 规范的适用范围

本页面遵循[官方文档规范](https://unixnu.com/zh/contributing/documentation)的中英文对应与正文互链原则，但这是独立模块仓库，不是 UniXNU VitePress 文档提交，因此没有伪造上游导航或声称文档已合并。

[仓库规范](https://unixnu.com/zh/contributing/repository)及[移植记录规范](https://unixnu.com/zh/porting/records)要求完整格式 2 设备元数据和验证。此有限模块集不满足正式支持设备登记契约，不伪造 `porting.toml` 或支持状态。
