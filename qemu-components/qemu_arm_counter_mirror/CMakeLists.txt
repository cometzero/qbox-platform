gs_create_dymod(qemu_arm_counter_mirror)
target_link_libraries(qemu_arm_counter_mirror PUBLIC
    arm_system_counter
    qemu_timer_api)
