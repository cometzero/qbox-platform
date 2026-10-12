# Direct Linux reset payload

`prepare.py` builds `boot.S` and `fdt.c` with
`${CROSS_COMPILE:-aarch64-linux-gnu-}{as,gcc,objcopy}`. It links the existing
TF-A `libfdt` and freestanding libc sources from
`hsoc-stack/components/primary_compute/trusted-firmware-a`; the workspace
source checkout and AArch64 C toolchain are required.
It copies the deployed DTB unchanged for the default BSP command line;
`fdtput` is used only when bootargs need an override.
It prints the resulting artifact paths and load addresses as JSON. The shell
launcher supplies the matching product initramfs and WIC-backed root filesystem,
or the BSP initramfs with its two-partition BSP WIC. BSP uses
`rdinit=/init` without `root=`; its WIC provides boot/misc block devices only.
The product initramfs retains its normal dm-verity
setup and read-only `rootro_a` root; writable state uses the WIC's overlay/data
partitions. By default the launcher attaches the source disk directly, so guest
writes persist there. `--copy-disk` creates a private `rootfs.wic` using
`cp --reflink=auto --sparse=always`. AutoSD UKI preparation and boot-control
inspection use the selected disk too; without `--copy-disk`, UKI installation
updates the source disk's ESP.
Initramfs images are loaded unchanged, without an appended init overlay.
The root launcher's `--autosd` mode also accepts an AutoSD nightly disk and
its original dracut initramfs via a prepared JSON manifest. It uses the same
reset payload and deployed Apollo kernel, with root UUID and OSTree deployment
arguments extracted from the image's BLS entry. Required drivers, EROFS and
fs-verity are built into that kernel; the AutoSD kernel modules cannot be
loaded across kernel ABIs. This direct boot does not run AutoSD UKI boot
selection or qualify Secure Boot, updates or rollback.
BSP uses `rdinit=/init` and runs the image's original selftests. On failure,
the original init opens `nexios-bsp-failed#`; on success it opens `nexios-bsp#`.
Both are bootable interactive shells. The launcher's `bsp_selftest` result
preserves FAIL independently of the AP boot smoke result. The failure shell
is reached before the original init's network/SSH setup, so use the UART.

```sh
python3 prepare.py --kernel /path/to/Image --dtb /path/to/apollo-qvp.dtb \
    --output-dir /path/to/run/linux-boot \
    --initrd /path/to/nexios-initramfs-image-apollo-qvp.cpio.gz \
    --bootargs 'console=ttyAMA0 earlycon=pl011,0x1a400000 root=PARTLABEL=rootro_a rootwait ro'
```

The fixed addresses are reset payload `0x000f0000` (AP shared SRAM),
uncompressed arm64 Image
`0x80200000`, DTB `0x88000000`, and optional initramfs `0x90000000`.
The helper rejects an invalid Image header or overlapping Image/initramfs.
The Image must use the current kernel's zero text offset. The reset payload
runs at EL3 and enters non-secure EL2h with translation disabled and the arm64
boot register convention. This replaces the boot handoff only; it does not
implement TF-A, U-Boot, secure boot, or physical power transitions.

The CPU wrapper must provide EL3, reset CPU 0 at the payload, start secondary
CPUs powered off, and select native QEMU PSCI via `smc`. Native PSCI powers up
secondary CPUs at Linux's requested EL2 entry point. The optional QEMU
`linux-smc-stub-address` hook forwards non-PSCI SMC requests to the SystemC
Linux-domain stub. Hardware fidelity is not implied by successful responses.

The Saturn DTS owns the memory map, CPU topology, console, PSCI, and device
status. The launcher supplies the initramfs load address and exact file size
as assembly constants, without writing initrd properties to the DTB file.
Before entering Linux, the reset payload uses libfdt to validate and expand
the in-memory DTB, set `/chosen/linux,initrd-start` and `linux,initrd-end`,
then pack it. Without an external initramfs, it removes any stale initrd
properties. A fixup error prints `LINUX_BOOT_FDT_FAILED` and stops the boot.
The helper allows 256 bytes of DTB growth within the arm64 2 MiB limit.

The reset payload lives in the top 64 KiB of the modeled 1 MiB AP shared
SRAM, above SDS and BL2 work areas. The linker reserves the top 4 KiB for
its stack and rejects payloads that overlap it.
This profile runs no TF-A/RSE/SI firmware; the full-system SRAM layout is
unchanged. SRAM is outside Linux's ordinary RAM map, so no reset-payload
reserved-memory node is added.
The runner reads default bootargs
from the DTB; a CPU-count override changes `nr_cpus`, and product/AutoSD boots
supply their root arguments. Explicit bootargs replace the command line.
The root launcher records the final command line in `launch.json.bootargs`;
QBox consumes the DTB, not a separate bootargs environment variable.
CPU population and disk attachment belong to the launcher/Lua profile;
the payload helper has no CPU-count or disk options.
Existing hardware nodes are not changed, removed, or disabled. Unused virtio block slots
retain read-only MMIO identification registers with DeviceID zero (no device),
so Linux can probe every DT-described slot without an external abort. The Saturn SI remoteproc
nodes describe the `apollo_si_stub` service in this profile, which supplies
a loaded resource table and an `ethsi1` RPMsg ARP/ICMP peer.
The driver attaches to this service; it does not load or start SI firmware.
The Linux-only SCMI mock advertises BASE discovery only and returns
`SCMI_NOT_SUPPORTED` (`-1`) for other protocols, including performance and power
management. It does not register a cpufreq service or change QEMU CPU clocks.
This contract avoids synthetic DVFS traffic; it does not qualify the underlying
MHU interrupt timing under load or real DVFS.
