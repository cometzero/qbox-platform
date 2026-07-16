# QBox Apollo Platform

This platform contains Apollo QBox entrypoints for primary-compute direct boot,
SI CL1 isolated boot, and the full RSE-first Apollo QVP.

The primary-compute direct-boot entrypoint is not the full Apollo firmware
chain: RSE, TF-A, OP-TEE, and U-Boot are bypassed by the QBox AArch64
direct-boot stub. Use `apollo-qvp.lua` when firmware-chain fidelity is
required.

The primary-compute direct-boot entrypoint is:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/apollo-pc.lua
```

The SI CL1 isolated Zephyr entrypoint is:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/apollo-si-cl1.lua
```

The full-system QBox virtual platform entrypoint is:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/apollo-qvp.lua
```

The full-system entrypoint composes subsystem-owned Apollo hardware blocks:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/config.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/fabric.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/rse.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/ap_compute.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/ros.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/system_mgmt.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/si_cl0.lua
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/si_cl1.lua
```

The full-system machine contract is loaded from the same directory before
platform construction:

```text
hw-block/topology.lua
hw-block/address_map.lua
hw-block/transaction_routes.lua
hw-block/signal_routes.lua
hw-block/boot_control.lua
hw-block/software_contract.lua
hw-block/machine_contract.lua
```

`topology.lua` declares 52-bit system/AP/SMD, 32-bit RSE, and 40-bit SI CL0/CL1
views. The runtime currently instantiates `system_router`, `ap_router`,
`smd_router`, `rse_router`, `si_cl0_router`, and `si_cl1_router`. The
`system_to_smd_nci` bridge decodes only the SMD high-nibble. AP and both SI
views have no broad one-to-one system bridge: cross-domain traffic uses the
RSE-programmed AP/SI/SMDEXP ATUs or an explicitly declared static SCMI/HIPC,
shared-SRAM, or GIC window. The contract phase is `A4_policy_routing`, broad
passthrough is forbidden, and `compatibility_debt` is empty.

AP/SI HIPC windows and the SI CL0-to-SI CL1 SCMI window use explicit bridges.
Local targets, CPU initiators, GPEX, and loaders bind to their domain router,
so overlapping numeric local addresses no longer depend on platform-wide
target-priority mutation.

The AP/SI non-secure MHU shared SRAM is initialized by SI0 before AP reset
release and is therefore SMD-owned even though AP has the canonical address
view. Its contract is `preserve_on_ap_reset`; AP reset fan-out must not clear
the mailbox-free state before TF-A/Linux uses the SCMI transport.

SI0 secure transport initialization must also preserve any valid BUSY request
posted before the completer starts. AP PFDI, SI CL1 PFDI, RSE SCMI, PSCI, and
the other secure completer channels share this policy. Preserving only the MHU
doorbell is insufficient: mailbox status, flags, length, and payload remain
requester-owned until SI0 consumes the message or publishes an error response.

The full-system AP, RSE, and both Safety Islands use multi-thread TCG so each
vCPU has an independent wake condition. QBox completes managed start-in-reset
release on the target vCPU and does not start a reset-held CPU's quantum
keeper. This prevents an idle reset-held timehandler from owning a global
SystemC suspend request. TF-A releases the AP secondary CPUs sequentially,
while CL1 Zephyr's reset voting lock may select any released physical MPID as
logical CPU 0.

CL1 also uses `multithread-quantum` synchronization. Its execution can run at
most one global quantum ahead of SystemC. Managed CPUs stop their quantum
keepers while reset is asserted, then restart time synchronization after the
target-vCPU reset release completes. After release they remain wakeable across
WFI so QEMU deadline timers can wake the CPUs reliably. The host PPU model
preserves the current power state when firmware enables a lower dynamic
minimum policy, so that policy update does not reassert CPU reset. Each CL1
Cortex-R82 generic timer runs at 100 MHz to match the Zephyr system-clock
configuration.
Override the TCG defaults with `QBOX_APOLLO_FULL_AP_TCG_MODE`,
`QBOX_RDASPEN_RSE_TCG_MODE`, `QBOX_RDASPEN_RSE_SYNC_POLICY`,
`QBOX_APOLLO_FULL_SI_CL0_TCG_MODE`, `QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY`,
`QBOX_APOLLO_FULL_SI_CL1_TCG_MODE`, and
`QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY` for experiments. The isolated CL1
entrypoint keeps its single-thread default and can be overridden with
`QBOX_APOLLO_SI_CL1_TCG_MODE`.

Hardware-block helpers used by the full-system entrypoint live under:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/
```

The current full-system block helpers are:

```text
hw-block/rse.lua
hw-block/config.lua
hw-block/fabric.lua
hw-block/primary_compute.lua
hw-block/ap_compute.lua
hw-block/si_cl0.lua
hw-block/si_cl1.lua
hw-block/si_cl1_isolated.lua
hw-block/ros.lua
hw-block/system_mgmt.lua
```

Export and validate the machine-readable contract with:

```bash
python3 scripts/test/validate_qbox_apollo_topology.py \
  --emit build/qbox-apollo-qvp/topology/topology.json
```

`hw-block/ros.lua` tracks the modeled Rest of System subset from the Arm Zena
CSS FVP RoS peripheral table: AP-visible virtio block/net/rng and PL031 RTC.

`hw-block/system_mgmt.lua` owns cross-domain system-management hardware:
AP/SI/RSE MHU windows, AP/RSE logical aliases, reset/power integration, SMD
shared memory, SCMI/PFDI messaging, ATU windows, and safety/control surfaces.
RSE secure boot and RSE-local security peripherals remain in `hw-block/rse.lua`;
AP firmware-chain and AP hardware construction live in `hw-block/ap_compute.lua`;
SI host-visible SRAM/PPU windows live in `hw-block/si_cl0.lua` and
`hw-block/si_cl1.lua`.

## Timer Topology

Apollo QBox keeps CPU-local and platform REFCLK timers on separate model
paths.

- CPU internal Arm generic timers remain per-core QEMU `ARMCPU` timers. Their
  outputs are GIC PPIs, so they are not represented by the AP REFCLK MMIO
  device.
- AP REFCLK is a 125MHz Arm memory-mapped generic timer exposed through the
  reusable Arm MMIO QEMU/QBox path.
- AP REFCLK frame 0 maps the non-secure `AP_SYS_CNT_BASE_NS` view and drives
  SPI 49.
- AP REFCLK frame 1 maps the secure `AP_SYS_CNT_BASE_S` view and drives
  SPI 48.
- AP REFCLK does not use `qemu_hexagon_qtimer`, `qct-qtimer`, or a
  `qct-qtimer` compatibility alias. Those paths remain outside the Apollo
  Arm generic timer contract.
- SI0, CSS, and RSE counter windows use the `host_gtimer` control/read/sync
  frame model for REFCLK counter behavior.

## Build Local Artifacts

```bash
./local_build.sh build
```

Local source-build artifacts follow `build/local-${MACHINE}`. The helper reads
the active Yocto machine and currently resolves to `apollo-qvp`; an explicit
`MACHINE` overrides it. `apollo-fvp` is the built-in fallback only when no
active machine is available or Yocto-variable loading is disabled.

The direct-boot runner consumes:

```text
build/local-${MACHINE}/deploy/boot/Image
build/local-${MACHINE}/deploy/boot/initramfs.cpio.gz
```

The full-system runner also consumes the local firmware deploy artifacts under
`build/local-${MACHINE}/deploy/firmware/`, including RSE ROM/flash/OTP, AP
flash, SI CL0 firmware, and SI CL1 Zephyr images.

The direct-boot runner uses the local-build Linux DTB as its base and applies a
small `/chosen` overlay for direct bootargs and initrd addresses. Generated
artifacts are written to the following legacy compatibility root. Full-system
QVP evidence uses `build/qbox-apollo-qvp/`; do not mix these direct-boot files
with full-system or FVP-reference evidence.

```text
build/qbox-apollo-fvp/apollo-fvp-direct.dtb
build/qbox-apollo-fvp/apollo-fvp-direct.overlay.dts
build/qbox-apollo-fvp/apollo-fvp-direct.overlay.dtbo
```

## Build QBox Targets

```bash
./local_build.sh qbox
```

## Full-System RSE-First Boot

The default Apollo full-system performance path uses shared-memory SRAM DMI.
It forwards `--rse-fast-boot-sram-dmi` to the RSE runner and sets
`QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY=true` so host SI/AP SRAM regions use
transferable shared-memory DMI instead of direct-file aliases.

```bash
./run_qbox_local.sh
```

For a bounded headless active-QVP command, use:

```bash
./run_qbox_yocto.sh --headless --exit-after-pass --timeout 900
```

For an explicit local-source full-system run, use:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --si-mode live-cl0-cl1 \
  --skip-build \
  --timeout 2400 \
  --rootfs-bootargs-profile quiet-console \
  --cc3xx-qemu-native-backend \
  --rse-lms-accel \
  --rse-fast-boot-sram-dmi \
  --out-dir build/qbox-apollo-qvp/full-live-cl0-cl1-sram-dmi
```

The RSE child `result.json` should report
`rse_fast_boot_sram_dmi.enabled: true`,
`rse_fast_boot_sram_dmi.env.QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY: "true"`, and
`rse_direct_file_aliases_summary.enabled: false`. The `host_sram_backing`
entries for the host SRAM regions should use `mode: "shared_memory"` and
`file_created: false`.

The default SRAM DMI path should not create file-backed host SRAM images:

```bash
find build/qbox-apollo-qvp/full-live-cl0-cl1-sram-dmi -type f \( \
  -name 'host-si-cl*-sram.bin' -o \
  -name 'host-ap-*-sram.bin' \
\) -print -quit
```

The command should print nothing. Use the legacy file-backed SRAM aliases only
for explicit debug or compatibility rollback:

```bash
./run_qbox_local.sh --legacy-file-backed-sram
```

For direct RSE-runner debugging, the legacy equivalent is:

```bash
python3 scripts/run/run_qbox_fvp_rd_aspen_rse.py \
  --skip-build \
  --cc3xx-qemu-native-backend \
  --rse-lms-accel \
  --rse-fast-boot-aliases \
  --qbox-perf-profile \
  --timeout 90 \
  --ignore-fail-patterns \
  --out-dir build/qbox-apollo-qvp/rse-legacy-file-backed-sram
```

## A4 Policy-Routing Verification Baseline

The 2026-07-16 baseline completed the SMD/ATU policy-routing migration and the
reset-held CPU quantum-keeper fix. Reproduce the narrow build/test gate first:

```bash
./local_build.sh qbox --qbox-unit-tests
python3 scripts/test/validate_qbox_apollo_fvp_full_map.py
python3 scripts/test/validate_qbox_apollo_topology.py
python3 scripts/test/audit_qbox_core_boundary.py
```

Then run the local-source full-system image and audit its result:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --si-mode live-cl0-cl1 \
  --timeout 600 \
  --out-dir build/qbox-apollo-fvp/<run-id>
python3 scripts/test/audit_qbox_apollo_fvp_full_coverage.py \
  --result-json build/qbox-apollo-fvp/<run-id>/result.json \
  --output build/qbox-apollo-fvp/<run-id>/full-coverage-audit.json
```

For the active Yocto image, build with `./yocto_build.sh` and use
`--rootfs-bootargs-profile none`. Supply the QBox provider and all firmware,
DTB, Safety Island, provisioning, capsule, and WIC arguments from
`build/tmp_baremetal`; do not silently mix a local rootfs with the Yocto
provider. The verified provider and WIC paths were:

```text
build/tmp_baremetal/sysroots-components/x86_64/
  qbox-apollo-qvp-native/usr/bin/platforms-vp
build/tmp_baremetal/deploy/images/apollo-qvp/
  nexios-image-apollo-qvp.wic
```

The baseline passed QBox-platform component tests 33/33, the reset-release
test 50 consecutive times, local full-system boot and coverage 5/5, and Yocto
full-system boot and coverage 3/3. Two post-review local acceptance runs also
proved resolved `maxcpus=4`/four online Linux CPUs and preservation of the
SMD-owned SCMI SRAM across AP reset; both passed 49/49 coverage. After closing
the secure pending-mailbox startup race, SCP module tests passed 77/77 and
trace-off local/Yocto full-system runs each passed 3/3 with 49/49 coverage.
Generated runtime evidence is under
`build/qbox-apollo-fvp/architecture-debt-final-pfdi-preserve-*-20260716/`; the
durable report is
`doc/apollo-qvp-architecture-debt-validation-2026-07-16.md` in the top-level
project.

## Headless Boot

```bash
export MACHINE="${MACHINE:-apollo-qvp}"
python3 scripts/run/run_qbox_apollo_fvp_linux.py \
  --timeout 600
```

The direct-boot result files are currently written under the legacy root:

```text
build/qbox-apollo-fvp/<timestamp>/
```

Inspect:

```text
result.json
summary.txt
qbox-apollo-fvp.log
```

The direct-boot AP path keeps its 16-CPU experiment default and direct local
bootargs keep `maxcpus=16`. The full-system path defaults to 4 modeled AP CPUs,
matching active `apollo-qvp` Yocto configuration. Use
`QBOX_APOLLO_NUM_CPUS=1..16` to override either path deliberately. The
full-system rootfs patching profile defaults to `quiet-console`, which replaces
stale `maxcpus=` tokens with the resolved full-system AP CPU count. A profile
of `none`, used for the Yocto WIC, leaves the image boot entry unchanged.

For direct-boot CPU wake debugging, keep tracing disabled for normal runs and
enable it only on focused reproductions:

```bash
QBOX_APOLLO_PC_TRACE=true \
QBOX_APOLLO_PC_TRACE_FILE=build/qbox-apollo-fvp/trace-16/cpu-pc-trace.log \
python3 scripts/run/run_qbox_apollo_fvp_linux.py \
  --skip-build \
  --timeout 180 \
  --out-dir build/qbox-apollo-fvp/trace-16
```

`QBOX_APOLLO_PC_TRACE_INTERVAL` and `QBOX_APOLLO_PC_TRACE_LIMIT` tune direct
PC trace volume. `QBOX_APOLLO_EXCEPTION_TRACE=true` enables exception-state
trace for the same direct AP CPU models.

For one selected direct-boot CPU GDB stub, set both
`QBOX_APOLLO_GDB_CPU_INDEX` and `QBOX_APOLLO_GDB_PORT`. The selected port must
not collide with any `--netdev hostfwd` TCP port. `QBOX_APOLLO_GDB_PORT_BASE`
is intentionally unsupported on Apollo direct boot.

Full-system AP PC tracing uses the RSE-runner controls
`QBOX_RDASPEN_AP_PC_TRACE`, `QBOX_RDASPEN_AP_PC_TRACE_FILE`,
`QBOX_RDASPEN_AP_PC_TRACE_INTERVAL`, and `QBOX_RDASPEN_AP_PC_TRACE_LIMIT`.

## Interactive Boot

```bash
python3 scripts/run/run_qbox_apollo_fvp_linux.py \
  --skip-build \
  --interactive \
  --timeout "${QBOX_APOLLO_TIMEOUT:-0}" \
  --local-build-dir build/local-${MACHINE}
```

Set `QBOX_APOLLO_TIMEOUT=0` for an unbounded interactive session.
