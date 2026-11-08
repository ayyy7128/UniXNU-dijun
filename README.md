# dijun iOS port — early source modules

Experimental work toward running original mobile iOS userspace on Xiaomi 15S Pro (`dijun`, XRING O1).

**iOS has not booted. Original Apple userspace has not executed. This repository is not a flashable release and is not a complete, independently rebuildable kernel or firmware.**

Documentation: [English status](docs/porting/dijun-source-snapshot.md) · [简体中文状态说明](docs/zh/porting/dijun-source-snapshot.md).

## First public snapshot

This snapshot contains eight explicitly reviewed, existing BSD-2-Clause-Patent source files:

- `UniXNU-dijun-ios27/drivers/ArmCpu/`: BSP CPU, GIC and architectural timer driver, its matching information and the original component license.
- `uefi/O1Pkg/Drivers/O1SerialDxe/`: EFI Serial I/O protocol over the RAM-first output sink.
- `uefi/O1Pkg/Library/O1SerialPortLib/`: reserved-RAM ring output, optional explicitly enabled UART mirroring and the log ABI definition. No captured logs are included.

The files are preserved byte-for-byte. `source-manifest.json` records their exact SHA-256 hashes and sizes. This is a limited module-source release, not the entire private integration workspace.

## Verified hardware result — 2026-11-08, test 13

A new native arm64e kernel was tested on the same device through the continuous kernel-to-userspace attempt. The selected diagnostic collection had a complete mapping for original v1 AppleImage4 and corecrypto, no unresolved references within that selected subset, and no missing-symbol stubs. It did **not** contain the full AMFI/Sandbox security dependency closure.

The actual terminal result was:

```text
Stage83 / error 0xe802
trust_cache_runtime_init → panic("amfi interface not available @%s:%d")
```

The panic caller was resolved from this test's own collection, not copied from a prior result. The earlier Image4-null guard had been passed; that does not establish complete Image4 runtime ABI compatibility or acceptance of signatures.

The device returned to Android normally. The newly installed test boot's full device-side SHA-256 was verified and the other boot slot was unchanged. Per the test procedure, the test boot was retained: `restored=false`, `test_boot_retained=true`. No raw device identifiers, boot IDs, partition backups or captured logs are published.

Not verified in this test:

- root filesystem mount or execution of original `launchd`/`dyld`;
- AMFI/CoreTrust signature-runtime compatibility;
- SpringBoard, a working iOS UI or a complete system boot.

The strict full early-security integration attempt still has **252 unresolved references to 214 distinct targets** across retained components. These are static mapping counts, not executed failures, a progress percentage or an estimate of time remaining.

## Target and current source

- Target original inputs: iPhone18,3 / v57ap, iOS 27.0.1, build 24A446; original collection Darwin 27 / XNU 13432.2.10.
- Current native source baseline: XNU 12377.121.6 / Darwin 25.5, compiled for real arm64e. It is not renamed to impersonate Darwin 27; matching symbol names do not establish a compatible private ABI.
- The current private 32 MiB main-OS RAM-root input contains original launchd and dyld, but not a complete dyld shared cache or SpringBoard. It is not a complete iOS system root.

## Build boundaries

There is deliberately no turnkey build or flash command in this snapshot.

`ArmCpu.cpp` needs the separately integrated `arm/o1_timer_bridge.h` and native XNU platform adaptations. `O1SerialPortLib.inf` references platform PCD declarations in `O1Pkg/O1Pkg.dec`; the mixed integration package is not included here. To integrate these modules, an appropriate licensed XNU/IOKit environment and an EDK II platform package with explicitly reserved, mapped logging RAM and the listed PCDs are required.

The complete local integration still needs private inputs. None of the Apple firmware, signed binaries, kernel collections, userland, dyld caches or stock boot images are supplied. Obtaining any such inputs remains the integrator's responsibility under their applicable licenses. This source license does not grant a license to Apple operating-system software.

## Safety and non-goals

- Signature, AMFI, CoreTrust and trust-cache failures remain real failures; there is no bypass, forced-success provider, replacement launchd or generic missing-symbol stub in this release.
- Current hardware experiments are limited to the verified active boot slot of the authorized device. They do not install iOS or authorize GPT, userdata, NV, RPMB, security-store, vbmeta or stock-firmware writes.
- No slot switching or `fastboot boot` route is used. Recovery is not guaranteed if both device transports become unavailable.
- Windows/WOA work is maintained separately and is not part of this snapshot.

## Upstream provenance

References only; upstream checkouts and private artifacts are not vendored:

| Project | Pinned source revision |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | `c96b29975645b711dc06bc435c3ad27321ce817c` |
| [ArmCpu](https://github.com/UniXNU/ArmCpu) | `93a5030c5d983d0ef90f0fca93879d6e36ad1ba8` |
| [Apple open-source XNU](https://github.com/apple-oss-distributions/xnu) | `ac9718fb1af618d5ce8678d0dc6e8a58f252216f` |
| [EDK II](https://github.com/tianocore/edk2) | `0ea22ef04f3bef379ad15cac46fd9ceaa13d1e9a` |

The XNU checkout and its APSL-covered changes are not redistributed in this first snapshot. Upstream licenses remain applicable to their own files; they are not replaced by this release's module license.

## Licensing

The eight module files already declare BSD-2-Clause-Patent, or are covered by the included ArmCpu component license. Existing copyright and SPDX notices are retained. `LICENSE` reproduces the license terms without assigning another component's copyright to the UEFI files. See `NOTICE.md` for scope and exclusions.
