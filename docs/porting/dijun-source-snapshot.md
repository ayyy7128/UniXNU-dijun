# dijun source snapshot and latest hardware status

[简体中文](../zh/porting/dijun-source-snapshot.md) · [Repository overview](../../README.md)

Updated **2026-11-11; the latest completed hardware test is phone36**.

## Current conclusion

**iOS has not booted. Execution still stops during original Sandbox initialization. There is no evidence of original Apple launchd/dyld user-code execution, SpringBoard, or an iOS system UI.**

The graphical UniXNU menu is a bootloader UI. Compilation, prelinking, checkpoints, and returning to Android are not proof of iOS system boot. This repository is not a flashable ROM, installer, or validated upstream supported-device entry.

## Latest hardware endpoint

Test 36 used a freshly built native arm64e kernel, retained the original AMFI/Sandbox/Image4/corecrypto dependency closure, and explicitly used bring-up diagnostic admission.

Its record is `Stage83 / error 0xe802`, with the unmapped-call observer stopping at an original Sandbox **OSDecrementAtomic** call. The original interface atomically decrements a 32-bit counter and returns its previous value; that target is not yet admitted into this candidate's mapping.

Stage83 is the terminal fault-capture return identifier, not a claim that 83 initialization stages completed. Progress is determined from the actual call target and same-boot evidence.

- Android returned normally on the same device, with boot completion verified.
- The complete installed test boot's device-side SHA-256 matched the candidate, and the unwritten boot slot remained unchanged.
- The verified test boot was retained: `restored=false`, `test_boot_retained=true`.
- The original image backup remains available for abnormal recovery; availability is not reported as completed restoration.

Device serial numbers, boot IDs, raw console/pstore logs, partition backups, and test images are not published.

## From diagnosed faults to the current blocker

| Hardware test | Observation and correction | Evidence limit |
| --- | --- | --- |
| phone28 | Bounded panic capture obtained the real data-abort cause: original NULL-safe static safeMetaCast had been misbound to a member metaCast without a NULL guard | Similar authentication/type-walk shape is not NULL-semantic equivalence |
| phone30 | Corrected safeMetaCast and removed the fixed-zero code-signing substitute; original AMFI explicitly rejected missing amfi-only-platform-code input | Policy values must be traced, not guessed |
| phone31 | Transported only that four-byte property from the original v57ap DeviceTree template verified against the official manifest | A static template is not live iPhone security-state evidence; SecureDT remained unmapped |
| phone32 | Added guarded genuine SecureDT lookup/property calls; the observer advanced to original Sandbox filter-mask count queries | Not proof that all AMFI or Sandbox initialization completed |
| phone34 | Queried actual BSD syscall, Mach trap, and MIG kobject counts; the observer advanced to a VM page-mask query | Counts come from real tables and normal MIG initialization, not fabricated constants |
| phone35 | Reused the genuine VM page-mask boundary and verified the native kernel_map input; the observer advanced to atomic increment | Original private map offsets are not reused |
| phone36 | Called genuine OSIncrementAtomic through a guarded boundary, preserving relaxed ordering and the previous-value return | The next blocker is an unmapped OSDecrementAtomic call |

Each listed normal return has separate local candidate hashes and same-boot records. Historical mismatched candidates and packages never tested on hardware are not counted as successful verification.

## Retracted identities and reliability corrections

### Panic-record overflow

The old observer appended a PC/LR suffix before checking capacity. It could overrun the 128-byte text area and corrupt the following boot/recovery state, producing two stalls requiring manual fastboot. A frozen-source repair preserves the readable-page policy, parameter ABI, and recovery-record layout.

The exact repaired function was exercised using an explicit Darwin-cursor host model, with GCC/Clang plain and ASan/UBSan builds. All four builds passed **664 cases each**, covering long formats, NULL/unreadable cursors, byte-by-byte complete-format mutations, and recovery-state canaries. **This is host bounds coverage, not ARM/PAC runtime or Apple userspace execution evidence.** Subsequent phone28 returned normally and captured a precise fault.

### Device-switch query

The read-only bdevsw_isfree interface had been misidentified as bdevsw_add. A NULL-to-zero-switch substitute changed registration and failure behavior. That binding was retracted and replaced with the genuine read-only free-slot query, preserving the nonzero PAC-signed error-callback sentinel and the separate true registration call.

### Nullable cast

Original static safeMetaCast cannot be replaced by member metaCast. The new guarded boundary calls the real static NULL-safe API, returns the same borrowed pointer or NULL, and adds no retain/release or type-authentication relaxation.

### Code-signing configuration

The original function was not a class-pattern/no-match dispatcher. It reads, caches, and publishes code-signing configuration. A fixed-zero substitute had no equivalence proof and was removed from new kernels. The complete real native platform query is verified without fabricating TXM/production state or concealing exemption bits. Native/original platform differences remain explicit; full private-ABI equivalence is not claimed.

### Relocation proof

Negative tests found that the old layout fallback could accept a PAC-to-NOP mutation alongside address changes. It now permits only exact-target-identity address-immediate re-encodings. PAC, arithmetic, conditional, and failure-path changes must fail. When an intermediate baseline is required, both complete function/target proofs are retained instead of skipping checks.

phone29 and phone33 were locally rejected before device testing. They are not successful hardware trials. Historical records remain available locally, and stale admission must not be reused for flashing.

## Static closure and normal-launch admission

The test-36 candidate still has **306 distinct unmapped targets and 492 component-target entries**. A shared target can contribute entries in multiple components. These are static counts, not execution failures, work percentages, or time estimates.

The previously published test 13 used a smaller AppleImage4/corecrypto subset; its contemporary full-security attempt had 252 references to 214 targets. Closure scope and counting units differ, so those values cannot establish a progress rate or regression.

Current admission distinguishes:

- **Bring-up diagnostics:** the complete early-security component closure can remain incompletely mapped. The official unmapped-call reporter records and terminates; it does not return fixed success. Original AMFI registration runs at an explicit early initializer.
- **Normal-launch candidates:** complete mapping, no reporting stub or early initializer, and a qualified full security-provider ABI are mandatory. The current candidate does not satisfy this gate.

Original security-extension code, signature checks, and normal failure paths remain intact. Early registration, one TrustCache initializer returning, or diagnostic progress does not prove signed-load ABI, CoreTrust acceptance, or normal secure boot.

## Remaining work

1. Admit the actual OSDecrementAtomic contract, then continue from real earliest blockers through kernel interfaces, object/callback/PAC contracts, and lifetimes. Names alone are insufficient.
2. Qualify the full AMFI/TrustCache signed-load and runtime ABI, including the unproved immutable-runtime +40 callback producer.
3. Transport a complete original iOS system root and dyld shared cache; prove actual PID1/exec/EL0 and subsequent services. The current 32 MiB RAM root is a minimal original launchd/dyld input, not a complete system.
4. Complete iOS-side display, touch, storage, graphics, and other device services. Bootloader rendering does not establish iOS graphics support.

No number of remaining trials or completion deadline is promised. Kernel, root, exec, original user-code execution, and UI evidence must be recorded separately.

## GitHub update and licensing scope

Only the eight existing BSD-2-Clause-Patent module paths in source-manifest.json are published unchanged: ArmCpu, EFI Serial I/O, and the RAM-first output library. Their bytes have not changed in this update; the Chinese README and paired status/correction pages are updated.

Unreviewed XNU/APSL modifications, integration templates/tools, firmware/binaries/machine code, userspace, device logs/identities, credentials, partition backups, and Windows/WOA integration remain excluded. This snapshot lacks the timer bridge and platform PCD integration needed to independently reproduce the complete test image. It supplies no one-command build or flashing instructions. See [NOTICE.md](../../NOTICE.md).

## Application of UniXNU documentation rules

This page follows the [official documentation guide](https://unixnu.com/zh/contributing/documentation) principles of paired language pages and reachable body links. It is an independent module repository, not a UniXNU VitePress submission; no upstream navigation changes or merge are claimed.

The [repository guide](https://unixnu.com/zh/contributing/repository) and [porting-record guide](https://unixnu.com/zh/porting/records) require consistent format-2 metadata and validation for supported devices. This limited module snapshot does not satisfy that registration contract. No porting.toml or support status is fabricated.
