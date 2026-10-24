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
platforms/apollo/apollo-qvp-saturn-v.lua
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

### Standalone SI CL1 Zephyr

`platforms/apollo/apollo-qvp-zephyr.lua` composes four Cortex-R82 functional
CPUs, a direct GICv3 view, PL011 and 8 MiB CL1 LLRAM without AP/RSE/SCP/PPU
boot dependencies. The dedicated `zephyr-si-cl1-bench` board uses a 100 MHz
counter, CNTP PPI29 and CNTV PPI27; these overrides apply only to this profile.
The full Apollo profile retains its production timer assignment and 125 MHz
counter. MPIDRs are `0x10000 + cpu * 0x100`, GICD is `0x30200000`, GICR starts
at `0x30260000`, and UART is `0x2a410000` (SPI7 / INTID39).

From the workspace, run `./run_qbox_zephyr.sh` after building the QBox native
provider and `zephyr-si-cl1-boot`. This separate boot-only firmware disables
MPU setup because the current QEMU Cortex-R82 backend only implements the older
EL2 PMSA interface; it is not a substitute for the gem5 MPU/TCM benchmark. The launcher validates the ELF64 AArch64
image, obtains its entry point and PT_LOAD bytes, and writes a reproducible
`run.json`. `--headless --timeout 120 --exit-after-pass` performs bounded boot
validation using all four `SI_CL1_BOOT_CPU` records and the final
`SI_CL1_BOOT_RESULT` with a 100 MHz counter; `result.json` explicitly limits
PASS to boot. Interactive mode displays UART and platform logs in tmux; F12
stops the session and its managed QBox process group. Logs are retained under
`build/qbox-apollo-qvp/zephyr/`.

QBox runtime throughput and latency are not cycle-accurate measurements.
This composition provides benchmark MM/NC SRAM windows but does not emulate
per-core private DTCM or certify the full benchmark result. Use the gem5 SI
profile for the ported benchmark; retain failures if unsupported TCM accesses
prevent further QBox execution.

The 2026-10-24 integration run observed the Zephyr banner, all four distinct
CPU affinities and the 100 MHz boot result using the deployed native provider.
Actual F12 input to an attached tmux client removed the session and reaped the
managed QBox process. Evidence: workspace
`build/qbox-apollo-qvp/gem5/si-cl1-integration-20261024/qbox-boot-only/` and
`qbox-boot-f12/cleanup.json`. The MPU-enabled benchmark ELF was separately
observed failing the early `z_arm64_mm_init()` MPU capability check, and is not
reported as a successful QBox benchmark.
