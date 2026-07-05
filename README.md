# QBox Platform Overlay

This repository is the Apollo full-system platform overlay for QBox. It is
built against a separate QBox core source tree and a separate QEMU/libqemu
source tree.

In the Arm Auto Solutions workspace the expected topology is:

```text
hsoc-stack/tools/qbox/            QBox core: platforms-vp, libqbox/libqemu
                                  integration, reusable SystemC and
                                  QEMU-backed components
hsoc-stack/tools/qbox-platform/   Apollo overlay: Lua platforms, Zena/RSE
                                  models, Apollo-specific wrappers and tests
hsoc-stack/tools/qemu/            checkout-local QEMU/libqemu source used by
                                  QBox
```

Apollo platform source should live in this overlay, not in the QBox core tree.

## Owned Surface

Primary platform entrypoints:

```text
platforms/apollo/apollo-qvp.lua
platforms/apollo/apollo-pc.lua
platforms/apollo/apollo-si-cl1.lua
```

Apollo hardware block helpers live under:

```text
platforms/apollo/hw-block/
```

Overlay-owned component source includes Zena/RSE hardware models and
Apollo-specific wrappers under:

```text
systemc-components/
qemu-components/arm_smmuv3/
qemu-components/cc3xx_native/
qemu-components/cpu_arm/cpu_arm_cortex_a720ae/
qemu-components/cpu_arm/cpu_arm_cortex_r82/
qemu-components/sbsa_gwdt/
qemu-components/virtio_mmio_rng/
tests/components/
```

The Apollo direct-boot configurations also share the retained
platform-neutral helper:

```text
fw/arm64_bootloader.lua
```

Reusable core components should be upstreamed or kept in
`hsoc-stack/tools/qbox`.

## Workspace Build

From the Arm Auto Solutions workspace root, build the Apollo QBox platform
dependencies with:

```bash
./local_build.sh qbox
```

The workspace helper configures this overlay as the CMake source tree and
passes the core and QEMU source paths explicitly:

```text
QBOX_CORE_DIR=hsoc-stack/tools/qbox
QBOX_PLATFORM_DIR=hsoc-stack/tools/qbox-platform
QBOX_PLATFORM_BUILD_DIR=build/local-apollo-fvp/work/qbox-platform
QBOX_QEMU_DIR=hsoc-stack/tools/qemu
```

`QBOX_BUILD_DIR` is accepted only as a compatibility alias for
`QBOX_PLATFORM_BUILD_DIR`.

The default aggregate target is:

```text
apollo_fvp_full_system
```

## Manual Configure

When debugging CMake directly from the workspace root:

```bash
cmake \
  -S hsoc-stack/tools/qbox-platform \
  -B build/local-apollo-fvp/work/qbox-platform \
  -DCMAKE_BUILD_TYPE=Release \
  -DQBOX_CORE_SOURCE_DIR="${PWD}/hsoc-stack/tools/qbox" \
  -DQBOX_QEMU_SOURCE_DIR="${PWD}/hsoc-stack/tools/qemu" \
  -DLIBQEMU_GIT="file://${PWD}/hsoc-stack/tools/qemu" \
  -DFETCHCONTENT_SOURCE_DIR_LIBQEMU="${PWD}/hsoc-stack/tools/qemu"

cmake --build build/local-apollo-fvp/work/qbox-platform \
  --target apollo_fvp_full_system \
  --parallel 8
```

For focused iteration, replace `apollo_fvp_full_system` with a component or
test target, for example `mhu320ae-tests`, `mmu720ae`, or `platforms-vp`.

## Run

After local build artifacts exist, launch the Apollo full-system QBox demo from
the workspace root:

```bash
./run_qbox_local.sh
```

For bounded headless validation:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --si-mode live-cl0-cl1 \
  --skip-build \
  --timeout 900
```

Primary-compute direct boot and SI CL1 isolated smoke paths remain available:

```bash
python3 scripts/run/run_qbox_apollo_fvp_linux.py --timeout 600
python3 scripts/run/run_qbox_apollo_fvp_si_cl1.py --timeout 300
```

Runtime evidence is written under `build/qbox-apollo-fvp/`.

## Packaging

To package existing local-build outputs into a QBox-runnable image set:

```bash
./local_build.sh --package
./run_qbox_local.sh --local-build-dir build/local-apollo-fvp/package/qbox/local-build
```

## Related Docs

- Root workspace guide: `README.md`
- Agent/source ownership guide: `AGENTS.md`
- Apollo platform guide: `platforms/apollo/README.md`
