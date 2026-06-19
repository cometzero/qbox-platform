add_subdirectory(cc3xx_native)
add_subdirectory(arm_smmuv3)
add_subdirectory(cpu_arm/cpu_arm_cortex_a720ae)
add_subdirectory(cpu_arm/cpu_arm_cortex_r82)
add_subdirectory(sbsa_gwdt)
add_subdirectory(virtio_mmio_rng)

foreach(qbox_platform_qemu_target
    arm_smmuv3
    cpu_arm_cortexA720AE
    cpu_arm_cortexR82
    sbsa_gwdt
    virtio_mmio_rng)
    if(TARGET "${qbox_platform_qemu_target}")
        target_include_directories(
            "${qbox_platform_qemu_target}" PUBLIC
            $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components>
            $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components/common/include>
            $<BUILD_INTERFACE:${QBOX_PLATFORM_SOURCE_DIR}/qemu-components>)
    endif()
endforeach()
