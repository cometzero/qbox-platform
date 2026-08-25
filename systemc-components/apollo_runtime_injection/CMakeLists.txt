gs_create_dymod(apollo_runtime_injection)
target_include_directories(
    apollo_runtime_injection
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    apollo_runtime_injection
    PUBLIC
        arm_system_counter
        arm_gicv3
        gicx00_multiview
        qemu_pl061
        runtime_action_service
        zena_ssu
)
