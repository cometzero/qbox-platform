if(NOT TARGET keystone)
    find_package(Git REQUIRED)
    file(GLOB KEYSTONE_PATCH_FILES
        "${QBOX_CORE_SOURCE_DIR}/patch/keystone/*.patch")
    cpmfindpackage(
        NAME keystone
        GIT_REPOSITORY https://github.com/keystone-engine/keystone.git
        GIT_TAG 0.9.2
        GIT_SHALLOW TRUE
        PATCH_COMMAND
            ${GIT_EXECUTABLE}
            -c "user.name=qqvp_ci"
            -c "user.email=qqvp_ci@qualcomm.com"
            am "${KEYSTONE_PATCH_FILES}"
        UPDATE_DISCONNECTED 1
        OPTIONS "BUILD_LIBS_ONLY")
endif()

add_executable(
    apollo-fourcpu-timer-wake-test
    apollo-fourcpu-timer-wake-test.cc)
set_target_properties(
    apollo-fourcpu-timer-wake-test
    PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

if(TARGET apollo-fourcpu-timer-wake-test)
    target_include_directories(
        apollo-fourcpu-timer-wake-test
        PRIVATE
            ${QBOX_CORE_SOURCE_DIR}/tests/qbox/include
            ${keystone_SOURCE_DIR}/include)
    target_link_libraries(
        apollo-fourcpu-timer-wake-test
        PRIVATE
            cpu_arm_cortexA720AE
            arm_gicv3
            qemu_arm_arch_timer_mmio
            qemu_timer_api
            global_peripheral_initiator
            router
            reset_gpio
            gs_memory
            ${TARGET_LIBS}
            keystone)

    foreach(phase IN ITEMS local-ppi mmio-broadcast)
        add_test(
            NAME apollo-fourcpu-${phase}-wake
            COMMAND $<TARGET_FILE:apollo-fourcpu-timer-wake-test>
                -p test-bench.inst.managed_start_in_reset_release=true
                -p test-bench.inst.accel=tcg
                -p test-bench.inst.tcg_mode=MULTI
                -p test-bench.inst.sync_policy=multithread-freerunning
                -p test-bench.inst.time_sync_strategy=quantum_keeper
                -p test-bench.quantum_ns=10000000
                -p test-bench.gic.num_spi=960
                -p test-bench.gic.redist_region=[1,1,1,1]
                -p test-bench.gic.has_security_extensions=true
                -p test-bench.gic.has_lpi=true
                -p test-bench.gic.revision=4
                -p test-bench.gic.has_gicv4_1=true
                -p test-bench.gic.has_direct_lpi=true
                -p test-bench.gic.has_rvpeid=true
                -p test-bench.gic.has_vpend_valid_dirty=true
                -p test-bench.gic.vpeid_bits=16
                -p test-bench.timer.cntfrq=125000000
                -p test-bench.timer.nr_frames=2
                -p test-bench.timer.view_size=65536
                -p test-bench.timer.frame_offset_0=131072
                -p test-bench.timer.frame_id_0=0
                -p test-bench.timer.frame_offset_1=65536
                -p test-bench.timer.frame_id_1=1
                -p test-bench.phase=${phase}
                -p test-bench.target_cpu=2
                -p log_level=0)
        set_tests_properties(
            apollo-fourcpu-${phase}-wake
            PROPERTIES TIMEOUT 30 LABELS "qbox-platform-fourcpu-timer")
    endforeach()

    add_test(
        NAME apollo-fourcpu-invalid-target
        COMMAND $<TARGET_FILE:apollo-fourcpu-timer-wake-test>
            -p test-bench.phase=mmio-broadcast
            -p test-bench.target_cpu=4)
    set_tests_properties(
        apollo-fourcpu-invalid-target
        PROPERTIES WILL_FAIL TRUE TIMEOUT 5 LABELS "qbox-platform-fourcpu-timer")
endif()
