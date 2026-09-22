#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build the reset payload and a private DTB for Apollo direct Linux boot."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess


ADDRESSES = {"boot_stub": 0x80000000, "kernel": 0x80200000,
             "dtb": 0x88000000, "initrd": 0x90000000}


def command(*args):
    return subprocess.check_output([str(a) for a in args], text=True).strip()


def prepare(args):
    firmware = getattr(args, "firmware", False)
    addresses = dict(ADDRESSES)
    if firmware:
        addresses["kernel"] = 0x80080000
        if args.initrd:
            raise ValueError("firmware mode embeds initrd in the on-disk UKI")
    kernel = args.kernel.resolve(strict=True)
    source_dtb = args.dtb.resolve(strict=True)
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with kernel.open("rb") as stream:
        header = stream.read(64)
    if len(header) != 64 or header[56:60] != b"ARM\x64":
        raise ValueError("--kernel must be an uncompressed arm64 Linux Image")
    if struct.unpack_from("<Q", header, 8)[0] != (0x80000 if firmware else 0):
        raise ValueError("Image text_offset does not match the selected boot placement")
    image_size = max(kernel.stat().st_size, struct.unpack_from("<Q", header, 16)[0])
    if addresses["kernel"] + image_size > addresses["dtb"]:
        raise ValueError("Linux Image overlaps the fixed DTB address")
    if source_dtb.stat().st_size > 0x200000:
        raise ValueError("DTB exceeds the arm64 boot protocol 2 MiB limit")
    initrd = args.initrd.resolve(strict=True) if args.initrd else None
    if initrd and ADDRESSES["initrd"] + initrd.stat().st_size > 0xff000000:
        raise ValueError("initramfs exceeds Apollo low DRAM")

    boot = out / "boot.bin"
    obj = out / "boot.o"
    prefix = os.environ.get("CROSS_COMPILE", "aarch64-linux-gnu-")
    command(prefix + "as", "-march=armv8-a", "--defsym",
            f"BOOT_ENTRY={addresses['kernel']}", "-o", obj,
            Path(__file__).with_name("boot.S"))
    command(prefix + "objcopy", "-O", "binary", "-j", ".text.boot", obj, boot)
    dtb = out / "linux.dtb"
    if source_dtb == dtb:
        raise ValueError("input DTB must differ from generated linux.dtb")
    shutil.copyfile(source_dtb, dtb)

    def put(node, prop, *values, kind="x"):
        command("fdtput", "-p", "-t", kind, dtb, node, prop, *values)

    def remove(node, prop):
        if prop in command("fdtget", "-p", dtb, node).splitlines():
            command("fdtput", "-d", dtb, node, prop)

    put("/chosen", "bootargs", args.bootargs, kind="s")
    put("/chosen", "stdout-path", "/soc/serial@1a400000:115200n8", kind="s")
    if initrd:
        put("/chosen", "linux,initrd-start", "0", "90000000")
        put("/chosen", "linux,initrd-end", "0",
            f"{ADDRESSES['initrd'] + initrd.stat().st_size:x}")
    else:
        remove("/chosen", "linux,initrd-start")
        remove("/chosen", "linux,initrd-end")
    put("/memory@80000000", "device_type", "memory", kind="s")
    put("/memory@80000000", "reg", "0", "80000000", "0", "7f000000",
        "200", "0", "0", "80000000")
    put("/psci", "method", "smc", kind="s")
    put("/reserved-memory/linux-boot@80000000", "reg", "0", "80000000", "0", "10000")
    put("/reserved-memory/linux-boot@80000000", "no-map")

    cpu_nodes = [name for name in command("fdtget", "-l", dtb, "/cpus").splitlines()
                 if name.startswith("cpu@")]
    for i, name in enumerate(cpu_nodes):
        if i >= args.cpus:
            # arm64 treats disabled PSCI CPUs as possible hotplug CPUs.
            command("fdtput", "-r", dtb, "/cpus/" + name)
            continue
        put("/cpus/" + name, "status", "okay", kind="s")
        remove("/cpus/" + name, "cpu-idle-states")
    # Native PSCI supports hotplug, but firmware power-domain topology is absent.
    command("fdtput", "-r", dtb, "/cpus/cpu-map")
    for cluster in range(4):
        if (cluster + 1) * 4 > args.cpus:
            put(f"/dsu-pmu-{cluster}", "status", "disabled", kind="s")
    for index, address in enumerate((0x30020000, 0x30030000, 0x30040000, 0x30050000)):
        put(f"/soc/virtio@{address:x}", "status",
            "okay" if args.disk and index == 0 else "disabled", kind="s")
    # The SystemC SI substitute provides the loaded resource table and RPMsg
    # queues expected by the attach-only driver; no remote CPU is executed.
    put("/soc/si_remoteproc", "status", "okay", kind="s")
    put("/soc/si_remoteproc/si-cl1", "status", "okay", kind="s")
    return {"boot_stub": str(boot), "kernel": str(kernel), "dtb": str(dtb),
            "initrd": str(initrd) if initrd else None,
            "addresses": addresses, "cpus": args.cpus, "firmware": firmware}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--firmware", action="store_true",
                        help="load Apollo U-Boot Linux-header image at 0x80080000")
    parser.add_argument("--dtb", type=Path, required=True)
    parser.add_argument("--initrd", type=Path)
    parser.add_argument("--disk", action="store_true",
                        help="enable the first virtio block device for a Yocto disk")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--bootargs", required=True)
    parser.add_argument("--cpus", type=int, choices=range(1, 17), default=4)
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args), sort_keys=True))
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"prepare Linux boot: {exc}\n")


if __name__ == "__main__":
    main()
