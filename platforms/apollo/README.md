# QBox Apollo Platform

This platform supports the full RSE-first Apollo QVP. Its sole runtime
entrypoint is:

```text
hsoc-stack/tools/qbox-platform/platforms/apollo/apollo-qvp.lua
```

The full-system runner instantiates the real Safety Island CL0 SCP-firmware
and CL1 Zephyr domains with the normal two-instance, two-GIC CL0/CL1 graph.

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

There is no separate machine-contract Lua layer. Runtime values are owned by
the hardware block that consumes them. `config.lua` contains shared runtime
options, assembly helpers, and request-context identifiers only. Hardware
addresses, window sizes, IRQs, and fixed hardware parameters are declared in
top-level local constant tables in `fabric.lua`, `rse.lua`, `ap_compute.lua`,
`ros.lua`, `system_mgmt.lua`, `si_cl0.lua`, or `si_cl1.lua`, then consumed by
the block that implements the corresponding behavior. Active SI interrupt
routing remains owned by `si_cl0.lua`.

The runtime currently instantiates `system_router`, `ap_router`,
`smd_router`, `rse_router`, `si_cl0_router`, and `si_cl1_router`. The
`system_to_smd_nci` bridge decodes only the SMD high-nibble. AP and both SI
views have no broad one-to-one system bridge: cross-domain traffic uses the
RSE-programmed AP/SI/SMDEXP ATUs or an explicitly declared static SCMI/HIPC,
shared-SRAM, or GIC window. Broad passthrough is forbidden.

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

The Apollo reset fanout retains exactly one reset for each SI CL0/CL1 QEMU
instance and orders the SI GIC multiview reset before both QEMU reset targets.

The current Yocto FWU reset qualification reaches a second RSE/SI/TF-A/U-Boot
and Linux Regular State. Capsule A/B acceptance is still open: a copied
`EFI/UpdateCapsule/fw.cap` is present in the per-run ESP, but U-Boot does not
yet emit `FWU: Updating`, `FIP_B`, or Trial State after that reset. Do not use
the second Regular State result as evidence of capsule application or rollback
persistence.

The full-system RSE and SI CL0 instances use single-thread TCG, while SI CL1
and AP use multi-thread TCG. All four QEMU instances use
`multithread-freerunning` synchronization with the `quantum_keeper` time-sync
strategy to avoid global-quantum rendezvous between independently scheduled
domains. QBox completes managed start-in-reset release on the target vCPU and
does not start a reset-held CPU's quantum keeper. This prevents an idle
reset-held timehandler from owning a global SystemC suspend request. TF-A
releases the AP secondary CPUs sequentially, while CL1 Zephyr's reset voting
lock may select any released physical MPID as logical CPU 0.

The Apollo SCMI Performance model exposes one domain per four AP CPUs and the
1.8, 2.0, and 2.5 GHz operating points used by the guest cpufreq contract.
Each domain keeps independent limits and selected levels. These values model
the guest-visible SCMI interface only; QEMU TCG execution rate is not coupled
to the selected operating point.

Managed CPUs stop their quantum keepers while reset is asserted, then restart
time synchronization after the target-vCPU reset release completes. After
release they remain wakeable across WFI so QEMU deadline timers can wake the
CPUs reliably. The host PPU model preserves the current power state when
firmware enables a lower dynamic minimum policy, so that policy update does
not reassert CPU reset. Each CL1 Cortex-R82 generic timer runs at 100 MHz to
match the Zephyr system-clock configuration. Use a one-run platform parameter
override when a bounded `multithread-quantum` comparison is required.

The SI1 PFDI postbox also uses the propagated TLM request context to identify
the vCPU that issued each doorbell. It asserts that vCPU's co-simulation
`sync_hold` until the real SI0 firmware publishes the shared-memory channel as
FREE, preventing the requester's virtual timeout from advancing ahead of the
separate SI0 QEMU instance. A channel is not a CPU identity: CPU0 issues all
four initial setup requests before steady-state channels 2 through 5 map to
CPUs 0 through 3. `sync_hold` pauses only QEMU/SystemC scheduling and the
requester's quantum keeper; it does not synthesize an MHU response or expose a
guest-visible halt, reset, IRQ, or power transition.

The QVP SCP-firmware build gives AP and SI1 online PFDI watchdogs 500 ms. This
reserves five complete heartbeat, response, and SystemC-quantum windows for
the four independently scheduled QEMU instances. The Arm FVP reference keeps
its original AP 100 ms and SI1 60 ms firmware values; the additional margin
is owned by the Apollo QVP metadata as a host-scheduling budget rather than a
guest-visible protocol change. The full-system runner treats any SI0 `PFDI
monitor timeout` report as a failed validation even when later boot markers
are present.

The AP cluster-utility SBIST FCTLR windows are backed by the `apollo_sbist`
SystemC model. Writing the force-failure bit injects the CPU-specific SBISTC
EQ failure into SI0 FMU device 1, records 210 through 213. The `zena_fmu`
model latches child-device critical and non-critical state into the matching
root summary record before raising the SI0 FMU interrupt, so SCP-firmware can
traverse and acknowledge the normal FMU hierarchy. The AP PFDI logical MHU
window is explicitly bridged to its SMD-owned PBX window; no broad AP-to-SMD
passthrough is used.

CPU RAS pseudo-fault generation is implemented in the Cortex-A720AE QEMU
error records. Correctable and deferred errors drive per-CPU fault PPI 17
through a SystemC delta synchronizer, and TF-A publishes CPER records through
the backed `0xFFA00000` buffer and notification SPI 89. Uncontainable errors
populate the SI0-visible per-core RAS records at cluster-utility offset
`0x010A0000` and assert cluster IRQs 325, 327, 329, or 331 until SCP-firmware
clears the status. Run the product qualification with:

```bash
./run_test.sh --machine apollo-qvp --test-profile ras_cpu
```

Run the Yocto BSP PFDI qualification through the root test interface:

```bash
./run_test.sh --machine apollo-qvp --bsp --test-profile pfdi \
  --fvp-reference build/tests/<fvp-pfdi-run>/summary.json
```

Run the SI0 SSU and FMU integration diagnostics with:

```bash
./run_test.sh --machine apollo-qvp --bsp \
  --test-profile safety-diagnostics-tests \
  --fvp-reference build/tests/<fvp-safety-diagnostics-run>/summary.json
```

The diagnostics exercise the SSU safety-state sequence, System FMU software
injection and fault upgrade, eleven GIC/MHU device FMUs, and six NI-710AE
FMUs. Device faults use the configured System FMU parent bank and record, so
the normal SCP-firmware root-first discovery and acknowledgement path is
preserved.

## Runtime actions

Apollo runtime actions are disabled by default. Enable them only with the
loopback Monitor endpoint:

```bash
QBOX_APOLLO_MONITOR=true \
QBOX_APOLLO_RUNTIME_INJECTION=true \
./run_qbox_yocto.sh
```

The opt-in configuration also interposes `ap_i2c5_irq_fault` on the I2C5
IRQ and exposes the AP-to-SI1 HIPC postbox one-shot drop plus the existing full
system reset fanout. Absolute simulation-time requests, expected-generation
checks, and structured lifecycle logs are supported. See the workspace
`doc/qbox-event-injection.md` for CLI examples, reset semantics and evidence.

`QBOX_APOLLO_MONITOR_BIND_ADDRESS` defaults to `127.0.0.1`. The Monitor
rejects runtime mutation on a non-loopback address. When runtime actions are
disabled, `platform.apollo_runtime_injection` is absent and the Monitor has no
mutation service path.

The allow-list contains the SI GIC multiview SPI pulse target, the SI0 SSU
typed fault event, CSS system-counter control, and all eight pins on each of
`platform.host_smd_gpio`, `platform.rse_gpio_0`, and
`platform.rse_gpio_1`. GPIO direction and data come from live PL061 register
readback. Input pins support drive, simulation-time pulse, and release to the
configured initial level. Output pins support side-effect-free observation
and PL061 data-register writes. Output forcing is not exported because the
current Apollo graph has no verified one-to-one downstream fault proxy.

Apollo's multiview extension windows own the IVIEWR state, but its `spi_out`
signals are not interposed on the two existing QEMU GIC graphs. Runtime SPI
actions therefore validate the requested owner against `gicx00_multiview` and
drive the selected `arm_gicv3` input through its typed runtime helper. The
qualified P0 scenario uses View1 architectural INTID 105; a raw INTID 128 FMU
interrupt is not safe because it has no corresponding error record.

All mutations run in the SystemC context selected by the Monitor. Delayed
requests and pulse widths use simulation time. Reset cancels scheduled and
active requests, restores GPIO input defaults, and advances the manager
generation. P0/P0.1 accepts only `clear_on_reset=true`.

Run the checked-in scenarios through the canonical full-system runner with:

```bash
python3 scripts/test/run_qbox_runtime_injection.py \
  qa-tests/qbox-runtime-injection/gpio-rse0-pin0-input.json \
  qa-tests/qbox-runtime-injection/gpio-rse0-pin1-output.json \
  qa-tests/qbox-runtime-injection/system-counter-control.json \
  qa-tests/qbox-runtime-injection/pulse-spi.json \
  qa-tests/qbox-runtime-injection/si-cl0-ssu-critical-event.json \
  --runner-arg=--no-post-login-probe \
  --out-dir build/qbox-apollo-qvp/runtime-actions
```

The harness records capabilities and pre/post/release snapshots, rejects new
post-boot fatal console patterns, and terminates only process groups carrying
its current-UID per-run token.

The profile boots the canonical Yocto QBox provider, checks the same four-CPU
prerequisite, service, CLI, OnL, monitoring, force-error, FMU, SBISTC, and
PFDI-monitor failure evidence as the FVP OEQA profile, and writes its result
below `build/tests/<timestamp>-qbox-bsp-pfdi/`.

## Validation Profiles

All named QBox validation profiles are FVP-reference gated. Run the matching
FVP profile first, then pass its `summary.json` with `--fvp-reference`; the
root runner rejects stale, failed, skipped, image-mismatched, CPU-count
mismatched, or contract-drifted references before QBox starts.

The currently implemented QBox profile surfaces are:

| Profile | Image | Command shape | Current state |
| --- | --- | --- | --- |
| `bsp-core` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile bsp-core --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented; reuses existing firmware, Linux, device, and topology probes. |
| `si-cl1` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile si-cl1 --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented; reuses CL1 console and multicore markers. |
| `smcf` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile smcf --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented; reuses SMCF command/result markers. |
| `pfdi` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile pfdi --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented. |
| `pfdi-si-cl1` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile pfdi-si-cl1 --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented for SI CL1 PFDI plus SI monitoring. |
| `safety-diagnostics-tests` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile safety-diagnostics-tests --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented. |
| `cpuidle` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile cpuidle --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented; current-SHA final runtime remains deferred. |
| `cpufreq` | BSP | `./run_test.sh --machine apollo-qvp --bsp --test-profile cpufreq --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented for the guest SCMI contract; QEMU TCG rate coupling is not claimed. |
| `ras_cpu` | product | `./run_test.sh --machine apollo-qvp --test-profile ras_cpu --fvp-reference build/tests/<fvp-run>/summary.json` | Implemented. |

The remaining non-Xen profiles are not current QBox PASS claims:

- `platform-devices` is blocked on final FVP product network/device evidence.
  QBox may only claim semantic network transport coverage: guest link, address,
  route, and a runner-owned host HTTP response. It is not FVP-identical host
  ping or SSH transport.
- `trusted-services` is blocked on current-SHA FVP evidence.
- `crypto-extension` is semantic on QBox: deterministic OpenSSL known-answer
  equality plus bounded AES/SHA instruction-use evidence. It must not reuse
  FVP crypto-plugin wall-time thresholds.
- `hipc` is blocked on a final FVP HIPC reference.
- `mbpp` is blocked on the isolated 16-CPU FVP lane and QBox prerequisites.

The complete 14 FVP plus 14 QBox aggregate and final `coverage.json` are
pending Todo 23. Current deferred and blocked items are tracked in
`.work/validation-plan/final-review-backlog.md`; do not treat fixture
aggregate output as final runtime coverage.

Override the QEMU defaults with the `QBOX_APOLLO_FULL_AP_*`,
`QBOX_RDASPEN_RSE_*`, `QBOX_APOLLO_FULL_SI_CL0_*`, and
`QBOX_APOLLO_FULL_SI_CL1_*` environment-variable families. Each family
provides `ACCEL`, `TCG_MODE`, `SYNC_POLICY`, and `TIME_SYNC_STRATEGY` controls.
The canonical runner's `--platform-param` option has final precedence and is
the preferred surface for one-run performance comparisons.

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

Run the canonical local-source full-system boot and its coverage audit:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --timeout 600 --out-dir build/qbox-apollo-qvp/<run-id>
python3 scripts/test/audit_qbox_apollo_fvp_full_coverage.py \
  --result-json build/qbox-apollo-qvp/<run-id>/result.json \
  --output build/qbox-apollo-qvp/<run-id>/full-coverage-audit.json
```

Enable the QBox web monitor for an interactive full-system boot with
`--monitor`. The dashboard listens on port 18080 by default. Supplying
`--monitor-port` also enables the monitor:

```bash
./run_qbox_yocto.sh --monitor --monitor-port 19090
```

Open `http://127.0.0.1:<port>/` while the QBox process is running. The monitor
is disabled unless either option is present, so the default boot path does not
open a listening socket. The existing QBox Crow monitor binds the requested
port to all host interfaces. Use host firewall rules or an isolated network on
shared systems when the dashboard must remain local-only.

`hw-block/ros.lua` tracks the modeled Rest of System subset from the Arm Zena
CSS FVP RoS peripheral table: AP-visible virtio block/net/rng, PL031 RTC, and
QVP-only DesignWare APB peripheral validation targets.

## DesignWare APB Peripherals

Apollo QVP exposes a bounded DesignWare APB validation island in the AP RoS
expansion window. The models are Linux-driver compatibility models, not full
Synopsys RTL configuration replicas.

| Block | Instances | AP base range | AP GIC SPI range | Guest validation |
| --- | ---: | --- | --- | --- |
| `dw_apb_i2c` + `dw_i2c_eeprom` | 6 | `0x30100000`-`0x3015ffff` | 320-325 | AT24 EEPROM read/write/compare at `0x50` |
| `dw_apb_ssi` | 4 | `0x30160000`-`0x3019ffff` | 326-329 | `spi-loopback-test` with `SPI_LOOP` |
| `dw_apb_uart` | 4 | `0x301a0000`-`0x301dffff` | 330-333 | `ttyS0` <-> `ttyS1`, `ttyS2` <-> `ttyS3` |

Run the opt-in guest probe through the normal QBox launcher:

```bash
./run_qbox_yocto.sh --headless --exit-after-pass --dwc-peripheral-probe
```

The probe records `dwc_peripheral_probe` in `result.json`. It is a pass only
when all six EEPROM comparisons, all four SPI loopback bindings, and both UART
pairs complete with zero return codes.

`hw-block/system_mgmt.lua` owns cross-domain system-management hardware:
AP/SI/RSE MHU windows, AP/RSE logical aliases, reset/power integration, SMD
shared memory, SMD GPIO, SCMI/PFDI messaging, ATU windows, and safety/control
surfaces.
RSE secure boot and RSE-local security peripherals remain in `hw-block/rse.lua`;
AP firmware-chain and AP hardware construction live in `hw-block/ap_compute.lua`;
SI host-visible SRAM/PPU windows live in `hw-block/si_cl0.lua` and
`hw-block/si_cl1.lua`.

## GPIO Topology

Apollo GPIO controllers reuse QEMU's functional PL061 model through the QBox
`qemu_pl061` wrapper. RSE GPIO0 and GPIO1 expose secure/non-secure aliases at
`0x50100000`/`0x40100000` and `0x50101000`/`0x40101000`. Their accesses pass
through the RSE PPCEXP0 filter and their interrupt outputs are ORed onto RSE
NVIC IRQ 34.

The SMD GPIO exists only at physical address `0x20000D0310000` on
`smd_router`. AP software reaches it at logical `0x40750000` through the
RSE-programmed `host_ap_atu`; there is no direct logical target that could
bypass translation. Its interrupt drives AP GIC SPI 193. Set
`QBOX_APOLLO_SMD_GPIO_INIT_INPUTS` to an eight-bit value for deterministic
input stimulus. Controller reset re-applies the current input bitmap.

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
`0x0008`, SMMU SID `0x0040`, EventID base `0`, ITS translator `0x20850040`,
and ITS collection-entry size 2. The endpoint-only `iommu-map` avoids
assigning the host bridge RID to the same SID.

Profile generation is admitted only by the immutable Apollo FVP reference
gate. The gate records that the packaged FVP applies its PCIe/ITS
configuration but cannot enumerate the endpoint because its first AP-visible
ECAM access at `0x10040000000` raises an EL3 SError
(`ESR_EL3=0x00000000be000211`, `ELR_EL3=0xffff8000807d08f0`). It does not
claim FVP physical ITS delivery or QBox equivalence. The current gate artifact is
`.omo/evidence/apollo-gic-its/final/F2/cycle2/integration-current/fvp-reference-gate-current.json`
with SHA256
`5ceb377244eb0e4fddd6e4346a701fa1a75185d0dc689d2e396c917cb3549a82`.
Downloaded Fast Models 11.31.25 components/configuration exist; this gate is
PASS while FVP PCIe/ITS qualification is `UNSUPPORTED`.

Prepare gate-bound A/B UKI disks with the shared endpoint-bound guest probe,
then run both modes with the top-level helpers:

```bash
python3 scripts/test/prepare_qbox_apollo_pcie_irq_profile.py \
  --fvp-reference-gate \
    .omo/evidence/apollo-gic-its/final/F2/cycle2/integration-current/fvp-reference-gate-current.json \
  --base-disk build/tmp_baremetal/deploy/images/apollo-qvp/nexios-bsp-initramfs-apollo-qvp.wic \
  --base-dtb build/tmp_baremetal/deploy/images/apollo-qvp/apollo-qvp.dtb \
  --base-initramfs build/tmp_baremetal/deploy/images/apollo-qvp/nexios-bsp-initramfs-apollo-qvp.cpio.gz \
  --output-dir build/qbox-apollo-qvp/pcie-irq-profile
QBOX_APOLLO_NUM_CPUS=4 QBOX_APOLLO_PCIE_IRQ_TEST=true \
./run_qbox_yocto.sh --headless --exit-after-pass --timeout 600 \
  --rootfs build/qbox-apollo-qvp/pcie-irq-profile/apollo-qvp-pcie-msix-disk.img \
  --rootfs-bootargs-profile none \
  --out-dir <msix-output>
QBOX_APOLLO_NUM_CPUS=4 QBOX_APOLLO_PCIE_IRQ_TEST=true \
./run_qbox_yocto.sh --headless --exit-after-pass --timeout 600 \
  --rootfs build/qbox-apollo-qvp/pcie-irq-profile/apollo-qvp-pcie-intx-disk.img \
  --rootfs-bootargs-profile none \
  --out-dir <intx-output>
python3 scripts/test/validate_qbox_apollo_pcie_irq_runtime.py \
  --profile-manifest build/qbox-apollo-qvp/pcie-irq-profile/manifest.json \
  --msix-log <msix-output>/qbox-primary-console.log \
  --intx-log <intx-output>/qbox-primary-console.log \
  --output build/qbox-apollo-qvp/pcie-irq-runtime-validation.json
```

The profile rejects a copied, stale, non-PASS, or hash-mismatched FVP gate and
stale base UKI inputs before creating output. It hashes every base/source and
generated artifact, builds independent MSI-X and INTx UKIs and disks, and
records the exact builder command line. Only the INTx UKI carries `pci=nomsi`.
The shared guest probe discovers the endpoint and workload target through
sysfs, proves the IRQ-domain chain, CPU0/CPU1 affinity, CPU1 hotplug fallback,
replay, and cleanup, and fails closed on workload or affinity errors. Linux
reports GPEX SPI input 301 as architectural INTID 333. The platform keeps the
Apollo ITS collection entry size at two bytes.

Current bounded evidence is the F3 r5 result
`.omo/evidence/apollo-gic-its/final/F3/cycle2/run-current-r5/result.json`
SHA256
`db8e74ebdc13c27eb9ba61bde2b2f3d0e2ae181cd93f94c5f477e763817ff8e5` and
Task10 boundary comparison
`.omo/evidence/apollo-gic-its/final/F3/cycle2/run-current-r5/boundary-comparison.json`
SHA256
`131f94749f1aa39243e19d8605c6b32b019a0e65cf5348a3f261754bf0cf676f`.
Task10 records 24 semantic rows: four FVP `UNSUPPORTED` rows, twenty QBox
`PASS` rows, and `device_equivalence=NOT_COMPARABLE`; r5 did not start FVP.
The known risk is one r3 freerunning RSE/SCP readiness ordering failure. Quiet
same-input r4 and current-source r5 pass without a retry wrapper, fixed sleep,
or source synchronization change; the supported cause is not causal proof.

Allowed wording: QBox opt-in PCIe IRQ profile PASS for MSI-X→ITS physical LPI,
INTx, SPI control, affinity/offline/replay, and cleanup. Forbidden wording:
FVP PCIe/ITS success, QBox/FVP parity, vLPI proof, full GIC-720AE parity, or
default-deploy endpoint contamination.

## Fault Event Test Profile

`QBOX_APOLLO_FAULT_EVENT_TEST=true` enables a test-only event observer without
adding an MMIO aperture. The SMMU event-queue level is passed through
`signal_fanout` to its normal GIC SPI 65 sink and to a separate `zena_fmu`
observer. Set `QBOX_APOLLO_FAULT_EVENT_LOG` to write the ordered event JSON.

```bash
QBOX_APOLLO_NUM_CPUS=4 \
QBOX_APOLLO_FAULT_EVENT_TEST=true \
QBOX_APOLLO_FAULT_EVENT_LOG="$PWD/build/qbox-apollo-qvp/fault-events.json" \
./run_qbox_yocto.sh --headless --exit-after-pass \
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

## Build Yocto Artifacts

```bash
./yocto_build.sh --bsp
./yocto_build.sh
```

The launcher consumes the deployed Yocto rootfs, RSE ROM/flash/OTP, AP flash,
SI CL0 firmware, SI CL1 Zephyr image, QBox provider, and generated
`.qboxconf`.

## Build QBox Targets

```bash
./yocto_build.sh qbox-apollo-qvp-native -c compile
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
./run_qbox_yocto.sh --uboot-only --reset-rse-state
```

```bash
./run_qbox_yocto.sh
```

For a bounded headless active-QVP command, use:

```bash
./run_qbox_yocto.sh --headless --exit-after-pass --timeout 900
```

For an explicit headless full-system run, use:

```bash
./run_qbox_yocto.sh --headless --exit-after-pass \
  --timeout 2400 \
  --rootfs-bootargs-profile quiet-console \
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
./run_qbox_yocto.sh --legacy-file-backed-sram
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
./yocto_build.sh qbox-apollo-qvp-native -c compile
python3 scripts/test/validate_qbox_apollo_fvp_full_map.py
python3 scripts/test/audit_qbox_core_boundary.py
```

Then run the Yocto full-system image and audit its result:

```bash
./run_qbox_yocto.sh --headless --exit-after-pass --timeout 600 \
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

### PCA9539 board demo

`board/pca9539.lua` connects PCA9539 at address `0x74` alongside the existing
I2C0 EEPROM (`0x50`). SMD PL061 GPIO0 drives RESET_N and GPIO1 receives INT_N;
both have board pull-ups. PCA lines 0→1 and 8→9 provide two-bank loopbacks.
The kernel uses the existing `gpio-pca953x` driver, not a platform-specific driver.
After building and booting the Yocto BSP, run from the workspace root:

```bash
ssh -p 8022 root@127.0.0.1 'sh -s' < scripts/test/verify_qbox_pca9539.sh
```

The test temporarily unbinds the driver to exercise PL061-controlled reset,
then checks GPIO readback, edge events, and the serving PL061 GPIO1 IRQ counter.
The upstream GIC route is INTID 225; its chained counter is not exposed in
`/proc/interrupts` on this kernel.
Use an isolated test guest: the test owns these GPIO lines and resets the expander.
See workspace `doc/board/pca9539.md` for wiring, model limitations, and results.

### TPS6594 PMIC board demo

`board/tps6594.lua` adds a TPS6594-Q1 on I2C0 at base address `0x48`
(page aliases `0x49` through `0x4c`). Its active-low interrupt connects to
SMD PL061 GPIO2. GPIO offsets 0→1 and 8→9 are board loopbacks.
Linux uses the existing TPS6594 MFD, regulator, pinctrl/GPIO, and RTC drivers.
Two `regulator-output` consumers exercise BUCK1 (900 mV) and LDO1 (1.8 V).
From the workspace root after BSP boot:

```bash
./scripts/run/ssh_run.sh scripts/test/verify_qbox_tps6594.sh
```

The script exercises regulator on/off, GPIO loopbacks, RTC time, and alarm
interrupts. See workspace `doc/board/tps6594.md` for the NVM profile,
functional modeling limits, and recorded results.

### HSOC GPIO / PERI0-PERI1 pinctrl

`hw-block/pinctrl.lua` adds `pinctrl_peri0` at `0x301e0000` with 14 bank
IRQs (GIC SPIs 334–347). Banks 0–5 contain eight pins; banks 6–13 contain
one each. Both instances use the `hsoc_gpio` model. Bank register pages
are spaced by `0x1000`, starting at offset zero. There are no topology
registers: the model uses CCI `bank_sizes`, and Linux uses the explicit
bank declarations in `pinctrl.dtsi`. GPIO loopbacks are composed in
`board/peri0-loopback.lua`.

`pinctrl_peri1` at `0x301f0000` has eight bank IRQs (GIC SPIs 348–355).
Banks 0–3 contain eight pins and banks 4–7 contain one pin (36 total),
with the same `0x1000` bank stride. SPI2/3 use bank0 and UART2/3 use
bank1; I2C0–5, SPI0/1 and UART0/1 remain on PERI0. Consumer pinctrl
properties are declared directly in the device nodes in `apollo-qvp.dts`.

After a fresh BSP boot, run from the workspace root:

```bash
./scripts/run/ssh_run.sh scripts/test/verify_qbox_hsoc_pinctrl.sh
./scripts/run/ssh_run.sh scripts/test/verify_qbox_hsoc_peripherals.sh
```

The first script changes I2C5 mux temporarily and restores it. The second
uses four short SPI loopback tests and both UART pairs. Use a dedicated
test guest; these scripts own the selected pins and peripheral endpoints.
The current ABI uses packed SEL/DAT/PS/PE/DS/IS/IE registers and
INTR_CON/PEND/MIRR_PEND/MASK/FLT_TYP/FLT_DEPTH. See
`doc/board/hsoc-gpio.md` for bit packing, reset and digital modeling limits.

### Keep the guest running

```bash
./run_qbox_yocto.sh --headless \
  --keep-running-after-pass \
  --timeout "${QBOX_APOLLO_TIMEOUT:-2400}" \
  --out-dir build/qbox-apollo-qvp/long-running
```
