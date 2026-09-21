/* SPDX-License-Identifier: Apache-2.0 */
/* Minimal reset payload; the native QEMU PSCI service owns secondary CPUs. */
.section .text.boot, "ax"
.global _start
_start:
    msr daifset, #15
    /* Permit non-secure AArch64 EL2 and its GIC system-register interface. */
    /* Linux booting.rst EL3 permissions, also used by QEMU's native PSCI
     * arm_emulate_firmware_reset(). Unimplemented enable bits are RES0 in
     * this QEMU profile. In particular FGTEn must precede Linux's EL2 setup.
     */
    ldr x4, =(0x501 | (1 << 16) | (1 << 17) | (1 << 26) | (1 << 27) | (1 << 28) | (1 << 38) | (1 << 39) | (1 << 41) | (1 << 43) | (1 << 44) | (1 << 45) | (1 << 46) | (1 << 49) | (1 << 59))
    msr scr_el3, x4
    /* Do not trap FP/SIMD, SVE or SME into the absent secure firmware. */
    mov x4, #0x1100
    msr cptr_el3, x4
    mrs x5, id_aa64pfr0_el1
    ubfx x5, x5, #32, #4
    cbz x5, 1f
    mov x4, #15
    msr S3_6_C1_C2_0, x4       /* ZCR_EL3.LEN */
1:
    mrs x5, id_aa64pfr1_el1
    ubfx x5, x5, #24, #4
    cbz x5, 2f
    mov x4, #15
    msr S3_6_C1_C2_6, x4       /* SMCR_EL3.LEN */
2:
    mov x4, #15
    msr icc_sre_el3, x4
    isb
    mov x4, #9
    msr icc_sre_el2, x4
    mov x4, #0x33ff
    msr cptr_el2, x4
    mov x4, #0x80000000
    msr hcr_el2, x4
    mov x4, #3
    msr cnthctl_el2, x4
    msr cntvoff_el2, xzr
    ldr x4, =125000000
    msr cntfrq_el0, x4
    ldr x4, =0x30c50830
    msr sctlr_el2, x4
    /* Linux arm64 boot protocol: MMU off, x0 DTB, x1..x3 zero, EL2h. */
    ldr x0, =0x88000000
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    ldr x4, =0x80200000
    msr elr_el3, x4
    mov x4, #0x3c9
    msr spsr_el3, x4
    dsb sy
    isb
    eret
.ltorg
