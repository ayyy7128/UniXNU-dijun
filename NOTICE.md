# License and release scope

## Included source

Only the eight exact paths in `source-manifest.json` are included as module source. They are copied byte-for-byte from the reviewed local files. All have an existing BSD-2-Clause-Patent declaration or, for ArmCpu matching metadata, the included component license.

- ArmCpu retains `Copyright (c) 2026, ArmCpu contributors. All rights reserved.` and its component `LICENSE`.
- The UEFI serial files retain their existing SPDX declarations. No ArmCpu copyright statement is added to those files and no unstated author identity is invented.
- The root `LICENSE` supplies the existing license's terms. It does not relicense any excluded file or grant rights to Apple operating-system software.
- The newly written release documentation and manifests are provided under BSD-2-Clause-Patent as part of this source-only snapshot. Original source attribution remains unchanged.

## Explicit exclusions

No Apple firmware, IPSW, kernelcache or retained Apple machine-code leaf is redistributed. No corecrypto implementation, private userland, launchd, dyld, shared cache, signed binary, partition image, device log, device serial/boot ID, key, token, credential or private authorization record is included.

Unlicensed local templates, XNU/APSL modifications not yet selected for redistribution, build artifacts, integration configurations with private inputs, vendor downloads, editor/session data and mixed Windows/WOA source are excluded. A directory name, source extension or lack of a secret-scanner alert is not enough for inclusion; the explicit reviewed manifest controls the release.

## Status

This repository is an early module-source snapshot. It does not contain a complete firmware, kernel build pipeline or runnable iOS system. The test result in `README.md` and the paired English/Chinese status pages is a sanitized factual description, not a claim that the published module set independently reproduces the full private test image. The paired pages follow the official documentation naming and language conventions where applicable; this independent source repository is not an upstream VitePress documentation submission or a validated format-2 supported-device entry.
