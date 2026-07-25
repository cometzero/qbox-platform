gs_create_dymod(qemu_sse_counter)
target_link_libraries(qemu_sse_counter PUBLIC
    arm_system_counter
    qemu_clock_source
    qemu_timer_api)
