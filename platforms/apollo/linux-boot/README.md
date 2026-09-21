# Direct Linux reset payload

`prepare.py` builds `boot.S` with `${CROSS_COMPILE:-aarch64-linux-gnu-}as`
and `objcopy`, copies the deployed DTB, and edits only the copy with `fdtput`.
It prints the resulting artifact paths and load addresses as JSON. The shell
launcher supplies the matching product initramfs and WIC-backed root filesystem,
or the BSP initramfs with its two-partition BSP WIC. BSP still uses
`rdinit=/init` without `root=`; its WIC provides boot/misc block devices only.
The product initramfs retains its normal dm-verity
setup and read-only `rootro_a` root; writable state uses the WIC's overlay/data
partitions. Each launch uses a private WIC copy.
Initramfs images are loaded unchanged, without an appended init overlay.
BSP uses `rdinit=/init` and runs the image's original selftests. On failure,
the original init opens `nexios-bsp-failed#`; on success it opens `nexios-bsp#`.
Both are bootable interactive shells. The launcher's `bsp_selftest` result
preserves FAIL independently of the AP boot smoke result. The failure shell
is reached before the original init's network/SSH setup, so use the UART.

```sh
python3 prepare.py --kernel /path/to/Image --dtb /path/to/apollo-qvp.dtb \
    --output-dir /path/to/run/linux-boot --cpus 4 --disk \
    --initrd /path/to/nexios-initramfs-image-apollo-qvp.cpio.gz \
    --bootargs 'console=ttyAMA0 earlycon=pl011,0x1a400000 root=PARTLABEL=rootro_a rootwait ro'
```

The fixed addresses are reset payload `0x80000000`, uncompressed arm64 Image
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

The DT copy selects the requested CPU count, removes firmware CPU idle states
and the full-system CPU topology map, preserves MHU/SCMI transports for the
mock's responses, reserves the reset payload, and sets `/chosen` boot arguments
and optional initramfs bounds. Absent virtio block devices are disabled; `--disk`
enables the first disk only. SI remoteproc is enabled for `apollo_si_stub`,
which supplies a loaded resource table and an `ethsi1` RPMsg ARP/ICMP peer.
The driver attaches to this service; it does not load or start SI firmware.
The Linux-only SCMI mock advertises BASE discovery only and returns
`SCMI_NOT_SUPPORTED` (`-1`) for other protocols, including performance and power
management. It does not register a cpufreq service or change QEMU CPU clocks.
This contract avoids synthetic DVFS traffic; it does not qualify the underlying
MHU interrupt timing under load or real DVFS.
