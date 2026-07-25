gs_create_dymod(apollo_timer_snapshot)
target_link_libraries(apollo_timer_snapshot PRIVATE
    arm_system_counter
    host_gtimer
    qemu_arm_counter_mirror
    qemu_sse_counter
    qemu_sse_timer)
