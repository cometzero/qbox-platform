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

The supported Apollo runtime entrypoint is:

```text
platforms/apollo/apollo-qvp.lua
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
qemu-components/rse_cpu_accel/
qemu-components/sbsa_gwdt/
qemu-components/virtio_mmio_rng/
tests/components/
```

The retained platform-neutral boot helper remains available to non-Apollo
platforms and custom configurations:

```text
fw/arm64_bootloader.lua
```

Reusable core components should be upstreamed or kept in
`hsoc-stack/tools/qbox`.

## Workspace Build

From the Arm Auto Solutions workspace root, build the Apollo QBox platform
dependencies with:

```bash
./yocto_build.sh --bsp
```

The `qbox-apollo-qvp-native` recipe configures this overlay as the CMake
source tree and passes the core and QEMU source paths explicitly.

```text
HSOC_APOLLO_QBOX_SRC=hsoc-stack/tools/qbox
HSOC_APOLLO_QBOX_PLATFORM_SRC=hsoc-stack/tools/qbox-platform
HSOC_APOLLO_QEMU_SRC=hsoc-stack/tools/qemu
```

The default aggregate target is:

```text
apollo_fvp_full_system
```

## Run

After Yocto artifacts exist, launch the Apollo full-system QBox demo from the
workspace root:

```bash
./run_qbox_yocto.sh
```

For bounded headless validation of the active QVP deploy image:

```bash
./run_qbox_yocto.sh --headless --exit-after-pass --timeout 900
```

Yocto-built Apollo QVP runs use `build/qbox-apollo-qvp/` and require the
generated QVP `.qboxconf` plus native sysroot provider.

Both Apollo full-system and direct-Linux profiles use native QEMU DMA350
and DW_apb_i2s components. Audio qualification requires the workspace
`scripts/test/verify_qbox_full_audio.py` or `verify_qbox_linux_audio.py`;
boot/login alone does not exercise loopback. The verifiers check DMA
memory, DMA/PIO PCM and 48 kHz whole-WAV equality, and reject observed
XRUN, PFDI timeout and RCU stall messages during the suite.
The Linux PIO stream-lock correction is retained. Native/bridge telemetry,
the unproven IRQ cache and diagnostic scheduling tools were removed from
active sources and archived in `../../../doc/dma-i2s/patches/`.
Freerunning and audio pacing are unchanged. Historical full DMA warm3
success required an explicit experimental guest scheduling policy;
default-policy full DMA remains unresolved. See
`../../../doc/dma-i2s/05-code-cleanup.md` for the retained changes,
patch application instructions and qualification boundaries.

## Related Docs

- Root workspace guide: `../../../README.md`
- Agent/source ownership guide: `../../../AGENTS.md`
- Apollo platform guide: `platforms/apollo/README.md`
