# dijun source snapshot and test status

[简体中文](../zh/porting/dijun-source-snapshot.md) · [Repository overview](../../README.md)

## Scope

This is an experimental source-module snapshot for Xiaomi 15S Pro (`dijun`, XRING O1), not a supported-device entry, a complete kernel or firmware, an installation package or a working iOS system.

Eight existing BSD-2-Clause-Patent source files are published unchanged: the ArmCpu CPU/GIC/timer module, EFI Serial I/O driver, RAM-first serial-output library and their metadata/license. `source-manifest.json` records exact sizes and SHA-256 hashes.

## Hardware test 13 — 2026-11-08

The continuous kernel-to-userspace attempt used a fresh native arm64e build. Mapping was complete for the selected original v1 AppleImage4/corecrypto diagnostic subset, with no missing-symbol stubs. This subset did not include the complete AMFI/Sandbox dependency closure.

The test ended at:

```text
Stage83 / error 0xe802
trust_cache_runtime_init → panic("amfi interface not available @%s:%d")
```

The callsite was verified against that exact test collection. Passing the earlier Image4-null guard is not proof of full Image4 runtime compatibility or signature acceptance.

Android returned normally. The installed test boot's complete device-side hash was verified, the other boot slot was unchanged, and the test boot was retained (`restored=false`, `test_boot_retained=true`). No raw logs, partition backups or device identifiers are published.

**Original Apple userspace execution, root mount, SpringBoard and an iOS system UI have not been verified. iOS has not booted.** The complete early-security integration attempt remains blocked by 252 unresolved references to 214 distinct targets. Those are static counts, not a percentage of work remaining.

## Rebuild and licensing boundaries

Target original inputs are iPhone18,3 / v57ap, iOS 27.0.1 (24A446), Darwin 27 / XNU 13432.2.10. The native source is XNU 12377.121.6 / Darwin 25.5; its version is not changed to impersonate the target.

The private 32 MiB RAM-root input has original launchd/dyld but lacks a complete dyld cache and SpringBoard. The published snapshot also lacks the timer bridge and platform PCD integration needed for an independent build. It cannot reproduce the complete private test image on its own.

Apple binaries, firmware, retained Apple machine-code leaves, corecrypto implementation, private userland, keys, credentials, device logs, backups and mixed Windows/WOA integration are excluded. Existing copyright and license notices remain intact. These module licenses do not grant rights to Apple operating-system software.

No signature, AMFI, CoreTrust or trust-cache check is relaxed. No generic missing-symbol stub or replacement launchd is used to claim success.

## How the UniXNU documentation rules apply

The [official documentation contribution guide](https://unixnu.com/zh/contributing/documentation#%E6%96%B0%E5%A2%9E%E9%A1%B5%E9%9D%A2) requires paired English and Chinese Markdown pages, navigation registration and reachable body links for manual pages in the **UniXNU main documentation repository**. This snapshot provides paired, mutually linked Markdown pages and a README entry point; it is not that VitePress site, and its upstream sidebar is not modified.

The [repository guide](https://unixnu.com/zh/contributing/repository) and [porting-record guide](https://unixnu.com/zh/porting/records) distinguish component repositories from format-2 device repositories. A real device registration requires a matching `board.toml`, full configuration and a validated `porting.toml`. This limited module set does not satisfy that contract: no device record is fabricated and no supported-device entry, catalog scan or upstream merge is claimed.
