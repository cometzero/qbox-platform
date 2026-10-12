#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build the SRAM reset payload and copy the Apollo DTB, optionally overriding bootargs."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess


ADDRESSES = {"boot_stub": 0x000f0000, "kernel": 0x80200000,
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
    initrd_size = initrd.stat().st_size if initrd else 0
    if addresses["initrd"] + initrd_size > 0xff000000:
        raise ValueError("initramfs exceeds Apollo low DRAM")

    dtb = out / "linux.dtb"
    if source_dtb == dtb:
        raise ValueError("input DTB must differ from generated linux.dtb")
    shutil.copyfile(source_dtb, dtb)

    if args.bootargs and args.bootargs != command("fdtget", dtb, "/chosen", "bootargs"):
        command("fdtput", "-p", "-t", "s", dtb, "/chosen", "bootargs", args.bootargs)
    capacity = (dtb.stat().st_size + 256 + 7) & ~7
    if capacity > 0x200000:
        raise ValueError("DTB leaves no space for the initramfs handoff properties")

    boot = out / "boot.bin"
    obj = out / "boot.o"
    elf = out / "boot.elf"
    source = Path(__file__).resolve().parent
    tfa = source.parents[4] / "components/primary_compute/trusted-firmware-a"
    libfdt = tfa / "lib/libfdt"
    libc = tfa / "lib/libc"
    prefix = os.environ.get("CROSS_COMPILE", "aarch64-linux-gnu-")
    command(prefix + "as", "-march=armv8-a", "--defsym",
            f"BOOT_ENTRY={addresses['kernel']}", "--defsym",
            f"DTB_CAPACITY={capacity}", "--defsym",
            f"INITRD_START={addresses['initrd'] if initrd else 0}", "--defsym",
            f"INITRD_SIZE={initrd_size}",
            "-o", obj, source / "boot.S")
    command(prefix + "gcc", "-Os", "-ffreestanding", "-fno-builtin",
            "-fno-stack-protector", "-fno-pic", "-mgeneral-regs-only",
            "-ffunction-sections", "-fdata-sections", "-nostdlib", "-static",
            "-no-pie", "-Wl,--gc-sections", "-Wl,--build-id=none",
            "-Wl,-T," + str(source / "boot.ld"),
            "-I" + str(libfdt), "-I" + str(tfa / "include/lib/libc"),
            "-I" + str(tfa / "include/lib/libc/aarch64"),
            obj, source / "fdt.c",
            *(libfdt / name for name in
              ("fdt.c", "fdt_ro.c", "fdt_rw.c", "fdt_wip.c", "fdt_check.c")),
            *(libc / (name + ".c") for name in
              ("memcpy", "memmove", "memset", "memcmp", "memchr",
               "strlen", "strnlen", "strcmp", "strncmp", "strrchr")),
            "-o", elf)
    command(prefix + "objcopy", "-O", "binary", elf, boot)
    return {"boot_stub": str(boot), "kernel": str(kernel), "dtb": str(dtb),
            "initrd": str(initrd) if initrd else None,
            "addresses": addresses, "firmware": firmware}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--firmware", action="store_true",
                        help="load Apollo U-Boot Linux-header image at 0x80080000")
    parser.add_argument("--dtb", type=Path, required=True)
    parser.add_argument("--initrd", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--bootargs", default="", help="override the DTB bootargs when different")
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args), sort_keys=True))
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"prepare Linux boot: {exc}\n")


if __name__ == "__main__":
    main()
