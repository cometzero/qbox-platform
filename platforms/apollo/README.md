# QBox Apollo Platform

This platform contains Apollo QBox entrypoints for primary-compute direct boot,
SI CL1 isolated boot, and the full RSE-first Apollo QVP.

The primary-compute direct-boot entrypoint is not the full Apollo firmware
chain: RSE, TF-A, OP-TEE, and U-Boot are bypassed by the QBox AArch64
direct-boot stub. Use `apollo-qvp.lua` when firmware-chain fidelity is
required.

The primary-compute direct-boot entrypoint is:

```text
tools/qbox-platform/platforms/apollo/apollo-pc.lua
```

The SI CL1 isolated Zephyr entrypoint is:

```text
tools/qbox-platform/platforms/apollo/apollo-si-cl1.lua
```

The full-system QBox virtual platform entrypoint is:

```text
tools/qbox-platform/platforms/apollo/apollo-qvp.lua
```

The full-system entrypoint composes subsystem-owned Apollo hardware blocks:

```text
tools/qbox-platform/platforms/apollo/hw-block/config.lua
tools/qbox-platform/platforms/apollo/hw-block/fabric.lua
tools/qbox-platform/platforms/apollo/hw-block/rse.lua
tools/qbox-platform/platforms/apollo/hw-block/ap_compute.lua
tools/qbox-platform/platforms/apollo/hw-block/ros.lua
tools/qbox-platform/platforms/apollo/hw-block/system_mgmt.lua
tools/qbox-platform/platforms/apollo/hw-block/si_cl0.lua
tools/qbox-platform/platforms/apollo/hw-block/si_cl1.lua
```

Hardware-block helpers used by the full-system entrypoint live under:

```text
tools/qbox-platform/platforms/apollo/hw-block/
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

`hw-block/ros.lua` tracks the modeled Rest of System subset from the Arm Zena
CSS FVP RoS peripheral table: AP-visible virtio block/net/rng and PL031 RTC.

`hw-block/system_mgmt.lua` owns cross-domain system-management hardware:
AP/SI/RSE MHU windows, AP/RSE logical aliases, reset/power integration, SMD
shared memory, SCMI/PFDI messaging, ATU windows, and safety/control surfaces.
RSE secure boot and RSE-local security peripherals remain in `hw-block/rse.lua`;
AP firmware-chain and AP hardware construction live in `hw-block/ap_compute.lua`;
SI host-visible SRAM/PPU windows live in `hw-block/si_cl0.lua` and
`hw-block/si_cl1.lua`.

## Build Local Artifacts

```bash
./local-build.sh build
```

The direct-boot runner consumes:

```text
build/local-apollo-fvp/deploy/boot/Image
build/local-apollo-fvp/deploy/boot/initramfs.cpio.gz
```

The full-system runner also consumes the local firmware deploy artifacts under
`build/local-apollo-fvp/deploy/firmware/`, including RSE ROM/flash/OTP, AP
flash, SI CL0 firmware, and SI CL1 Zephyr images.

The direct-boot runner uses the local-build Linux DTB as its base and applies a
small `/chosen` overlay for direct bootargs and initrd addresses. Generated
artifacts are written to:

```text
build/qbox-apollo-fvp/apollo-fvp-direct.dtb
build/qbox-apollo-fvp/apollo-fvp-direct.overlay.dts
build/qbox-apollo-fvp/apollo-fvp-direct.overlay.dtbo
```

## Build QBox Targets

```bash
./local-build.sh qbox
```

## Full-System RSE-First Boot

The default Apollo full-system performance path uses shared-memory SRAM DMI.
It forwards `--rse-fast-boot-sram-dmi` to the RSE runner and sets
`QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY=true` so host SI/AP SRAM regions use
transferable shared-memory DMI instead of direct-file aliases.

```bash
./run_qbox.sh
```

For a bounded headless command, use:

```bash
python3 scripts/run/run_qbox_apollo_fvp_full.py \
  --si-mode live-cl0-cl1 \
  --skip-build \
  --timeout 2400 \
  --rootfs-bootargs-profile quiet-console \
  --cc3xx-qemu-native-backend \
  --rse-lms-accel \
  --rse-fast-boot-sram-dmi \
  --out-dir build/qbox-apollo-fvp/full-live-cl0-cl1-sram-dmi
```

The RSE child `result.json` should report
`rse_fast_boot_sram_dmi.enabled: true`,
`rse_fast_boot_sram_dmi.env.QBOX_RDASPEN_HOST_SRAM_SHARED_MEMORY: "true"`, and
`rse_direct_file_aliases_summary.enabled: false`. The `host_sram_backing`
entries for the host SRAM regions should use `mode: "shared_memory"` and
`file_created: false`.

The default SRAM DMI path should not create file-backed host SRAM images:

```bash
find build/qbox-apollo-fvp/full-live-cl0-cl1-sram-dmi -type f \( \
  -name 'host-si-cl*-sram.bin' -o \
  -name 'host-ap-*-sram.bin' \
\) -print -quit
```

The command should print nothing. Use the legacy file-backed SRAM aliases only
for explicit debug or compatibility rollback:

```bash
./run_qbox.sh --legacy-file-backed-sram
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
  --out-dir build/qbox-apollo-fvp/rse-legacy-file-backed-sram
```

## Headless Boot

```bash
python3 scripts/run/run_qbox_apollo_fvp_linux.py \
  --timeout 600
```

The result files are written under:

```text
build/qbox-apollo-fvp/<timestamp>/
```

Inspect:

```text
result.json
summary.txt
qbox-apollo-fvp.log
```

The direct-boot and full-system AP paths default to 16 modeled AP CPUs. Use
`QBOX_APOLLO_NUM_CPUS=1..16` for direct-boot CPU-count experiments; full-system
AP runs use the same value when AP CPUs are enabled. Direct local bootargs also
default to `maxcpus=16`, and the full-system rootfs patching profile defaults
to `quiet-console`, which removes stale `maxcpus=` tokens from rootfs bootargs.

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
  --local-build-dir build/local-apollo-fvp
```

Set `QBOX_APOLLO_TIMEOUT=0` for an unbounded interactive session.
