# QBox Apollo Platform

This platform supports the full RSE-first Apollo QVP. Its sole runtime
entrypoint is:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/apollo-qvp.lua
```

The full-system runner always instantiates the real Safety Island CL0
SCP-firmware and CL1 Zephyr domains. There is no Apollo QVP Safety Island mode
selection.

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

The QBox-owned SCMI/PFDI completer validates the shared-memory message length
before protocol dispatch. A length smaller than the four-byte SCMI header or
larger than the channel capacity returns `SCMI_PROTOCOL_ERROR`, publishes the
channel as FREE, and performs no power/reset side effect. The next valid
request on the same channel is accepted. The CL1 Zephyr firmware owns the RPMsg
name-service exchange. PSCI and FF-A error semantics remain owned by TF-A and
OP-TEE rather than being synthesized by the platform model.

AP cold reset now resets every AP-owned MHU PBX/MBX frame while preserving the
SMD-owned shared SRAM. Frame reset clears doorbell, interrupt, pending
name-service, and requester-hold state. A reset peer is treated as offline, so
the sender retains the pending doorbell without receiving a synthetic
completion; after reset deassertion, a normal retry reaches the peer and frees
the requester. This models reset cancellation and recovery without inventing
SCMI, PFDI, or RPMsg replies while the real firmware peer is unavailable.

AP cold reset and Apollo full-system reset have distinct target lists. The
core0 PPU power-on load pulse drives the AP cold-reset path and must not reset
the PPU that generated it. A full-system reset additionally resets the four
active AP core PPUs, RSE/SI QEMU instances, RSE accelerator and local crypto,
SI0 NI-710AE policy, and the live-domain MHU frames. SI0 can then sequence the
AP PPUs back on only after its second initialization, preventing AP measured
boot traffic from being posted before the RSE receiver is ready.

The current Yocto FWU reset qualification reaches a second RSE/SI/TF-A/U-Boot
and Linux Regular State. Capsule A/B acceptance is still open: a copied
`EFI/UpdateCapsule/fw.cap` is present in the per-run ESP, but U-Boot does not
yet emit `FWU: Updating`, `FIP_B`, or Trial State after that reset. Do not use
the second Regular State result as evidence of capsule application or rollback
persistence.

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

The SI1 PFDI postbox also uses the propagated TLM request context to identify
the vCPU that issued each doorbell. It asserts that vCPU's co-simulation
`sync_hold` until the real SI0 firmware publishes the shared-memory channel as
FREE, preventing the requester's virtual timeout from advancing ahead of the
separate SI0 QEMU instance. A channel is not a CPU identity: CPU0 issues all
four initial setup requests before steady-state channels 2 through 5 map to
CPUs 0 through 3. `sync_hold` pauses only QEMU/SystemC scheduling and the
requester's quantum keeper; it does not synthesize an MHU response or expose a
guest-visible halt, reset, IRQ, or power transition.

Override the TCG defaults with `QBOX_APOLLO_FULL_AP_TCG_MODE`,
`QBOX_RDASPEN_RSE_TCG_MODE`, `QBOX_RDASPEN_RSE_SYNC_POLICY`,
`QBOX_APOLLO_FULL_SI_CL0_TCG_MODE`, `QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY`,
`QBOX_APOLLO_FULL_SI_CL1_TCG_MODE`, and
`QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY` for experiments.

Hardware-block helpers used by the full-system entrypoint live under:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/hw-block/
```

The current full-system block helpers are:

```text
hw-block/rse.lua
hw-block/config.lua
hw-block/fabric.lua
hw-block/ap_compute.lua
hw-block/si_cl0.lua
hw-block/si_cl1.lua
hw-block/ros.lua
hw-block/system_mgmt.lua
```

Export and validate the machine-readable contract with:

```bash
python3 scripts/test/validate_qbox_apollo_topology.py \
  --emit build/qbox-apollo-qvp/topology/topology.json
```

Run the same four-CPU smoke contract with either local-build or Yocto-owned
artifacts through the canonical runner's fidelity mode:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py --fidelity \
  --artifacts local --cpus 4 --profile smoke
python3 scripts/run/run_qbox_apollo_fvp_full.py --fidelity \
  --artifacts yocto --cpus 4 --profile smoke
```

Each run writes `manifest.json`, `result.json`,
`full-coverage-audit.json`, `fidelity-contract.json`, and
`fidelity-summary.json` below `build/qbox-apollo-qvp/`. The mode rejects
local/Yocto artifact mixing and requires the Linux CPU IDs to be exactly
0 through 3. It does not impose an emulator performance threshold.

`hw-block/ros.lua` tracks the modeled Rest of System subset from the Arm Zena
CSS FVP RoS peripheral table: AP-visible virtio block/net/rng and PL031 RTC.

`hw-block/system_mgmt.lua` owns cross-domain system-management hardware:
AP/SI/RSE MHU windows, AP/RSE logical aliases, reset/power integration, SMD
shared memory, SCMI/PFDI messaging, ATU windows, and safety/control surfaces.
RSE secure boot and RSE-local security peripherals remain in `hw-block/rse.lua`;
AP firmware-chain and AP hardware construction live in `hw-block/ap_compute.lua`;
SI host-visible SRAM/PPU windows live in `hw-block/si_cl0.lua` and
`hw-block/si_cl1.lua`.

The SI CL0 Cortex-R82 memory path crosses the primary NI-710AE protected
socket. Before the selected APU is enabled, only the configured reset owner or
trusted loader context can access the downstream target. After programming,
normal, debug, and DMI accesses use the same region, requester, security, and
read/write permissions. A downstream DMI grant is exposed only when one
allowed region contains its entire range. Enabling or reprogramming the APU
coalesces protected DMI invalidation into the next SystemC delta so the MMIO
instruction that changes policy can complete before QEMU retranslates code.

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

## PCIe MSI-X/LPI And INTx Test Profile

The Apollo PCIe interrupt endpoint is opt-in. Set
`QBOX_APOLLO_PCIE_IRQ_TEST=true` to instantiate one `virtio-net-pci` endpoint
at `0000:00:01.0`. Its fixed test identity is PCI requester/ITS DeviceID
`0x0008`, SMMU SID `0x0040`, EventID base `0`, and ITS translator
`0x20850040`. The endpoint-only `iommu-map` avoids assigning the host bridge
RID to the same SID.

Prepare the generated DT/initramfs test profile and run both modes with the
top-level helpers:

```bash
python3 scripts/test/prepare_qbox_apollo_pcie_irq_profile.py
QBOX_APOLLO_NUM_CPUS=4 QBOX_APOLLO_PCIE_IRQ_TEST=true \
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --skip-build --timeout 600 \
  --rootfs build/qbox-apollo-fvp/pcie-irq-profile-i4/apollo-qvp-pcie-msix-disk.img \
  --rootfs-bootargs-profile none \
  --out-dir <msix-output>
QBOX_APOLLO_NUM_CPUS=4 QBOX_APOLLO_PCIE_IRQ_TEST=true \
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --skip-build --timeout 600 \
  --rootfs build/qbox-apollo-fvp/pcie-irq-profile-i4/apollo-qvp-pcie-intx-disk.img \
  --rootfs-bootargs-profile none \
  --out-dir <intx-output>
python3 scripts/test/validate_qbox_apollo_pcie_irq_runtime.py \
  --msix-log <msix-output>/qbox-primary-console.log \
  --intx-log <intx-output>/qbox-primary-console.log \
  --output build/qbox-apollo-fvp/i4-pcie-irq-runtime-validation.json
```

The generated MSI-X disk is used for the first run. The INTx disk carries
`pci=nomsi` in its U-Boot script. Linux reports the legacy GIC SPI input 301
as architectural INTID 333. Both full-system tests use four CPUs and pin the
selected interrupt affinity to CPU0 before generating network traffic.

## Fault Event Test Profile

`QBOX_APOLLO_FAULT_EVENT_TEST=true` enables a test-only event observer without
adding an MMIO aperture. The SMMU event-queue level is passed through
`signal_fanout` to its normal GIC SPI 65 sink and to a separate `zena_fmu`
observer. Set `QBOX_APOLLO_FAULT_EVENT_LOG` to write the ordered event JSON.

```bash
QBOX_APOLLO_NUM_CPUS=4 \
QBOX_APOLLO_FAULT_EVENT_TEST=true \
QBOX_APOLLO_FAULT_EVENT_LOG="$PWD/build/qbox-apollo-qvp/fault-events.json" \
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --skip-build \
  --local-build-dir build/local-apollo-qvp \
  --timeout 600 \
  --rootfs-bootargs-profile none \
  --out-dir build/qbox-apollo-qvp/fault-event-construction
```

The observer writes `source`, `record`, `sink_assert`, `clear`,
`sink_deassert`, and `recovery` phases when a fault is injected and cleared.
The component test is the acceptance path for injection and clear; a normal
boot only validates construction when no SMMU fault occurs. The observer is
QBox test instrumentation and does not assert an undocumented physical
SMMU-to-NI-710AE-FMU route in Zena CSS.

## Build Local Artifacts

```bash
./local_build.sh build
```

Local source-build artifacts follow `build/local-${MACHINE}`. The helper reads
the active Yocto machine and currently resolves to `apollo-qvp`; an explicit
`MACHINE` overrides it. `apollo-fvp` is the built-in fallback only when no
active machine is available or Yocto-variable loading is disabled. The
full-system runner consumes the local deploy artifacts, including the rootfs,
RSE ROM/flash/OTP, AP flash, SI CL0 firmware, and SI CL1 Zephyr images.

## Build QBox Targets

```bash
./local_build.sh qbox
```

## Full-System RSE-First Boot

The default Apollo full-system performance path uses shared-memory SRAM DMI.
It forwards `--rse-fast-boot-sram-dmi` to the RSE runner and sets
`QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY=true` so host SI/AP SRAM regions use
transferable shared-memory DMI instead of direct-file aliases.

The default RSE boot-flash backend is `qemu-cfi-local`. One QEMU CFI01
MemoryRegion is mapped into the Cortex-M55 CPU-private address space and
exported through the existing TLM target socket, so local CPU and external
initiators observe one CFI state and backing image. Apollo enables callback
I/O mode and deferred dirty-sector writeback; periodic, reset, migration, and
shutdown boundaries flush the backing image. Use the SystemC Strata backend
only for explicit comparison or rollback:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --rse-flash-backend systemc-strata
```

Use the persistent-state reset option for a fresh PS/ITS state and bounded
U-Boot FWU Regular-State check:

```bash
./run_qbox_local.sh --uboot-only --reset-rse-state
```

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
  --skip-build \
  --timeout 2400 \
  --rootfs-bootargs-profile quiet-console \
  --cc3xx-qemu-native-backend \
  --rse-lms-accel \
  --rse-fast-boot-sram-dmi \
  --out-dir build/qbox-apollo-qvp/full-system-sram-dmi
```

The RSE child `result.json` should report
`rse_fast_boot_sram_dmi.enabled: true`,
`rse_fast_boot_sram_dmi.env.QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY: "true"`, and
`rse_direct_file_aliases_summary.enabled: false`. It should also report
`ap_fip_logical_aperture.mode: "atu_systemc_route"`; the default path must not
install the former logical AP-FIP file alias. The `host_sram_backing` entries
for the host SRAM regions should use `mode: "shared_memory"` and
`file_created: false`.

The 2026-07-17 FVP-aligned four-CPU profile additionally uses live AP/SI1 and
AP/RSE MHU peers, preserves the HIPC SRAM across AP reset, and advertises the
CFG2 CMN-CYPRUS r3p0 graph, a 52-bit MMU-720AE SMMUv3 profile, sixteen GICR
frames, two-byte ITS collection entries, supported Cortex-A720AE TCG
features, PL011 revision 3, and zero-capacity unused block placeholders. A
passing run must include all live SI1 readiness markers, especially
`PFDI service ready` and `RPMSG Endpoint: ATTACHED`, as well as FWU ABI 1.0,
four online AP CPUs, and the Linux login prompt. The top-level Korean
comparison and verification report is
`doc/apollo-qvp-fvp-qbox-yocto-system-log-comparison-2026-07-17-ko.md`.

The default SRAM DMI path should not create file-backed host SRAM images:

```bash
find build/qbox-apollo-qvp/full-system-sram-dmi -type f \( \
  -name 'host-si-cl*-sram.bin' -o \
  -name 'host-ap-*-sram.bin' \
\) -print -quit
```

The command should print nothing. Use the legacy file-backed SRAM aliases only
for explicit debug or compatibility rollback:

```bash
./run_qbox_local.sh --legacy-file-backed-sram
```

For private RSE runtime debugging, the legacy equivalent is:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py --runtime-child \
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

The 2026-07-17 recorded FVP/QBox non-AP differential additionally fixed RSE
CC3XX identification-register writes and the SI1 cross-instance PFDI deadline
race. Two trace-off local runs and one Yocto provider/image run passed with no
PFDI status, protocol-version, timeout-errno, or agent-not-ready marker; the
final coverage audits passed. Evidence is under
`build/qbox-apollo-qvp/pfdi-requester-context-local-20260717-r{2,3}/` and
`build/qbox-apollo-qvp/pfdi-requester-context-yocto-20260717-r1/`. The Korean
analysis is
`doc/apollo-qvp-fvp-qbox-non-ap-pfdi-analysis-2026-07-17-ko.md` in the
top-level project.

## CPU Count And Headless Boot

```bash
export MACHINE="${MACHINE:-apollo-qvp}"
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --timeout 600 \
  --out-dir build/qbox-apollo-qvp/<run-id>
```

The full-system result files are written under:

```text
build/qbox-apollo-qvp/<run-id>/
```

Inspect:

```text
result.json
summary.txt
qbox-platform.log
qbox-rse.log
qbox-safety-island-cl0.log
qbox-safety-island-cl1.log
qbox-secure-console.log
qbox-primary-console.log
```

The full-system and Yocto defaults remain 4 modeled AP CPUs. Build a coherent
optional 16-CPU Yocto image with:

```bash
export BB_ENV_PASSTHROUGH_ADDITIONS="${BB_ENV_PASSTHROUGH_ADDITIONS:-} PC_CPUS_COUNT_DEFAULT"
export PC_CPUS_COUNT_DEFAULT=16
./yocto_build.sh
```

The image recipe writes the effective count to
`nexios-image-apollo-qvp.qboxconf` as `QBOX_APOLLO_NUM_CPUS`. Therefore
`run_qbox_yocto.sh` selects 16 CPUs automatically for that image. Resolution
order is an explicit `QBOX_APOLLO_NUM_CPUS`, the selected qboxconf, active
`build/conf/local.conf`, then fallback 4. Explicit runtime values from 1 to 16
remain available for focused work, but a full-system boot must use firmware,
DT and `maxcpus` built for the same count.

The full-system rootfs patching profile defaults to `quiet-console`, which
replaces stale `maxcpus=` tokens with the resolved full-system AP CPU count. A
profile of `none`, used for the Yocto WIC, leaves the image boot entry
unchanged. The 16-CPU validation covered four clusters, sixteen functional
GIC redistributors, SI0 core-PPU reset release, PFDI monitoring and
representative CPU1/4/8/12 hotplug on both FVP and QBox.

Full-system AP PC tracing uses the RSE-runner controls
`QBOX_RDASPEN_AP_PC_TRACE`, `QBOX_RDASPEN_AP_PC_TRACE_FILE`,
`QBOX_RDASPEN_AP_PC_TRACE_INTERVAL`, and `QBOX_RDASPEN_AP_PC_TRACE_LIMIT`.

## Long-Running Full-System Boot

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --skip-build \
  --keep-running-after-pass \
  --timeout "${QBOX_APOLLO_TIMEOUT:-2400}" \
  --local-build-dir build/local-${MACHINE} \
  --out-dir build/qbox-apollo-qvp/long-running
```
