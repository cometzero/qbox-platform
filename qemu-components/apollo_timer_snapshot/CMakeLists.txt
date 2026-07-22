gs_create_dymod(apollo_timer_snapshot)
target_link_libraries(apollo_timer_snapshot PRIVATE
    arm_system_counter
    host_gtimer
    cpu_arm_cortexA720AE
    cpu_arm_cortexR82
    qemu_arm_arch_timer_mmio
    qemu_sse_timer)
