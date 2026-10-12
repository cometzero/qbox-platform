// SPDX-License-Identifier: Apache-2.0
#include <libfdt.h>

/* The loader supplies the exact loaded archive size, not its unpacked size. */
int boot_fixup_fdt(void *fdt, unsigned long capacity,
                  uint64_t initrd_start, uint64_t initrd_size)
{
    int ret, chosen;

    if (capacity > 0x200000 || initrd_start + initrd_size < initrd_start)
        return -FDT_ERR_BADVALUE;
    ret = fdt_check_full(fdt, capacity);
    if (ret)
        return ret;
    ret = fdt_open_into(fdt, fdt, capacity);
    if (ret)
        return ret;
    chosen = fdt_path_offset(fdt, "/chosen");
    if (chosen < 0)
        return chosen;
    if (initrd_size) {
        ret = fdt_setprop_u64(fdt, chosen, "linux,initrd-start", initrd_start);
        if (ret)
            return ret;
        ret = fdt_setprop_u64(fdt, chosen, "linux,initrd-end",
                              initrd_start + initrd_size);
    } else {
        ret = fdt_delprop(fdt, chosen, "linux,initrd-start");
        if (ret && ret != -FDT_ERR_NOTFOUND)
            return ret;
        ret = fdt_delprop(fdt, chosen, "linux,initrd-end");
        if (ret == -FDT_ERR_NOTFOUND)
            ret = 0;
    }
    return ret ? ret : fdt_pack(fdt);
}
