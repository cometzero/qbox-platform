cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(qbox_platform VERSION 0.1)
include(CTest)
option(QBOX_USE_SYSTEM_LIBQEMU "Use an installed libqemu CMake package" OFF)

get_filename_component(_qbox_platform_source_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_qbox_platform_tools_dir "${_qbox_platform_source_dir}/.." ABSOLUTE)
set(
    QBOX_PLATFORM_SOURCE_DIR
    "${_qbox_platform_source_dir}"
    CACHE PATH
    "Path to the qbox-platform overlay source tree"
    FORCE
)

set(_qbox_platform_default_core_source_dir "")
if(EXISTS "${_qbox_platform_tools_dir}/qbox/CMakeLists.txt")
    set(_qbox_platform_default_core_source_dir "${_qbox_platform_tools_dir}/qbox")
endif()
set(
    QBOX_CORE_SOURCE_DIR
    "${_qbox_platform_default_core_source_dir}"
    CACHE PATH
    "Path to the qbox core source tree"
)

if(NOT QBOX_CORE_SOURCE_DIR)
    message(FATAL_ERROR "QBOX_CORE_SOURCE_DIR must point to the qbox core source tree")
endif()
get_filename_component(QBOX_CORE_SOURCE_DIR "${QBOX_CORE_SOURCE_DIR}" ABSOLUTE)
set(QBOX_CORE_SOURCE_DIR "${QBOX_CORE_SOURCE_DIR}" CACHE PATH "Path to the qbox core source tree" FORCE)
if("${QBOX_CORE_SOURCE_DIR}" STREQUAL "${_qbox_platform_source_dir}")
    message(FATAL_ERROR "QBOX_CORE_SOURCE_DIR must not point at the qbox-platform overlay")
endif()
if(NOT EXISTS "${QBOX_CORE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "QBOX_CORE_SOURCE_DIR does not contain CMakeLists.txt: ${QBOX_CORE_SOURCE_DIR}")
endif()

set(_qbox_platform_default_qemu_source_dir "")
if(NOT QBOX_USE_SYSTEM_LIBQEMU)
    if(DEFINED QEMU_SOURCE_DIR AND QEMU_SOURCE_DIR)
        set(_qbox_platform_default_qemu_source_dir "${QEMU_SOURCE_DIR}")
    elseif(EXISTS "${_qbox_platform_tools_dir}/qemu/CMakeLists.txt")
        set(_qbox_platform_default_qemu_source_dir "${_qbox_platform_tools_dir}/qemu")
    endif()
endif()
set(
    QBOX_QEMU_SOURCE_DIR
    "${_qbox_platform_default_qemu_source_dir}"
    CACHE PATH
    "Path to the qemu/libqemu source tree"
)

if(QBOX_USE_SYSTEM_LIBQEMU)
    set(QBOX_QEMU_SOURCE_DIR "" CACHE PATH "Path to the qemu/libqemu source tree" FORCE)
    set(QEMU_SOURCE_DIR "" CACHE PATH "Compatibility path to qemu/libqemu source tree" FORCE)
    unset(FETCHCONTENT_SOURCE_DIR_QEMU CACHE)
    unset(FETCHCONTENT_SOURCE_DIR_LIBQEMU CACHE)
    unset(qemu_SOURCE_DIR CACHE)
    unset(libqemu_SOURCE_DIR CACHE)
elseif(QBOX_QEMU_SOURCE_DIR)
    get_filename_component(QBOX_QEMU_SOURCE_DIR "${QBOX_QEMU_SOURCE_DIR}" ABSOLUTE)
    set(QBOX_QEMU_SOURCE_DIR "${QBOX_QEMU_SOURCE_DIR}" CACHE PATH "Path to the qemu/libqemu source tree" FORCE)
    if(NOT EXISTS "${QBOX_QEMU_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "QBOX_QEMU_SOURCE_DIR does not contain CMakeLists.txt: ${QBOX_QEMU_SOURCE_DIR}")
    endif()

    set(QEMU_SOURCE_DIR "${QBOX_QEMU_SOURCE_DIR}" CACHE PATH "Compatibility path to qemu/libqemu source tree" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_QEMU "${QBOX_QEMU_SOURCE_DIR}" CACHE PATH "Local qemu source override" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_LIBQEMU "${QBOX_QEMU_SOURCE_DIR}" CACHE PATH "Local libqemu source override" FORCE)
    set(qemu_SOURCE_DIR "${QBOX_QEMU_SOURCE_DIR}" CACHE PATH "Local qemu source override" FORCE)
    set(libqemu_SOURCE_DIR "${QBOX_QEMU_SOURCE_DIR}" CACHE PATH "Local libqemu source override" FORCE)
    if(NOT DEFINED LIBQEMU_GIT OR LIBQEMU_GIT STREQUAL "")
        set(LIBQEMU_GIT "file://${QBOX_QEMU_SOURCE_DIR}" CACHE STRING "libqemu git url")
    endif()
endif()

message(STATUS "QBOX_CORE_SOURCE_DIR = ${QBOX_CORE_SOURCE_DIR}")
if(QBOX_USE_SYSTEM_LIBQEMU)
    message(STATUS "QBOX_USE_SYSTEM_LIBQEMU = ON")
elseif(QBOX_QEMU_SOURCE_DIR)
    message(STATUS "QBOX_QEMU_SOURCE_DIR = ${QBOX_QEMU_SOURCE_DIR}")
endif()

add_subdirectory("${QBOX_CORE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/qbox-core")

if(NOT TARGET qbox)
    message(FATAL_ERROR "QBOX_CORE_SOURCE_DIR did not define the qbox target")
endif()
set(TARGET_LIBS qbox)

add_subdirectory(systemc-components)
add_subdirectory(qemu-components)
add_subdirectory(platforms/apollo/rse-cpu)

if(BUILD_TESTING)
    set(QBOX_PLATFORM_SYSTEMC_COMPONENT_TEST_LABEL "qbox-platform-systemc-components")
    set(qbox_platform_label_systemc_component_tests TRUE)
    macro(add_test)
        _add_test(${ARGV})
        if(qbox_platform_label_systemc_component_tests)
            cmake_parse_arguments(qbox_platform_add_test "" "NAME" "" ${ARGV})
            if(qbox_platform_add_test_NAME)
                set_tests_properties(
                    ${qbox_platform_add_test_NAME}
                    PROPERTIES LABELS "${QBOX_PLATFORM_SYSTEMC_COMPONENT_TEST_LABEL}"
                )
            endif()
        endif()
    endmacro()
    set(QBOX_PLATFORM_SYSTEMC_COMPONENT_TESTS
        arm_system_counter
        apollo_runtime_injection
        apollo_cpu_ras
        cc3xx
        dma350
        gic720ae_messreg
        apollo_sbist
        gicx00_multiview
        host_cmn_cyprus
        host_gtimer
        host_ni710ae_nci
        host_ppu
        host_scr
        host_smcf_mgi
        host_system_pll
        mhu320ae
        mmu720ae
        ras_ffh_stub
        reset_fanout
        signal_or
        rse_atu
        rse_integrity_checker
        rse_kmu
        rse_lcm
        rse_ppc_filter
        rse_protection_ctrl
        rse_sam
        rse_sysctrl
        strata_flash_j3
        zena_fmu
        zena_leaf_fmu
        zena_ssu
        zena_watchdog
        zena_reset_ctrl
        zena_safety_vertical
    )
    set(QBOX_PLATFORM_SYSTEMC_COMPONENT_TEST_TARGETS "")
    foreach(component_test IN LISTS QBOX_PLATFORM_SYSTEMC_COMPONENT_TESTS)
        add_subdirectory("tests/components/${component_test}")
        get_property(
            component_test_targets
            DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/components/${component_test}"
            PROPERTY BUILDSYSTEM_TARGETS
        )
        list(APPEND QBOX_PLATFORM_SYSTEMC_COMPONENT_TEST_TARGETS ${component_test_targets})
    endforeach()
    set(qbox_platform_label_systemc_component_tests FALSE)
    if(QBOX_PLATFORM_SYSTEMC_COMPONENT_TEST_TARGETS)
        add_custom_target(
            qbox_platform_systemc_component_tests
            DEPENDS ${QBOX_PLATFORM_SYSTEMC_COMPONENT_TEST_TARGETS}
        )
    endif()
    add_subdirectory(tests/components/uart)
    add_subdirectory(tests/platforms)
endif()

set(
    QBOX_APOLLO_BUILD_TARGET
    "apollo_fvp_full_system"
    CACHE STRING
    "Aggregate target for Apollo FVP QBox runtime dependencies"
)

set(QBOX_APOLLO_REQUIRED_TARGETS
    platforms-vp
    keep_alive
    monitor
    addrtr
    router
    gs_memory
    host_scr
    loader
    char_backend_file
    char_backend_stdio
    dw-apb-i2c
    dw-apb-ssi
    dw-apb-uart
    uart-pl011
    global_peripheral_initiator
    cpu_arm_cortexA720AE
    cpu_arm_cortexR82
    arm_gicv3
    arm_gicv3_its
    qemu_gpex
    virtio_net_pci
    virtio_mmio_blk
    virtio_mmio_net
    virtio_mmio_rng
    arm_smmuv3
    smmuv3
    mmu720ae
    reset_gpio
    pl031
    qemu_pl061
    sbsa_gwdt
    cpu_arm_cortexM55
    nvic_armv7m
    ApolloRseCPU
    qemu_cc3xx
    qemu_clock_source
    qemu_sse_counter
    qemu_sse_timer
    qemu_arm_counter_mirror
    apollo_timer_snapshot
    qemu_device_cold_reset
    qemu_arm_arch_timer_mmio
    qemu_hexagon_qtimer
    mhu320ae
    gicx00_multiview
    apollo_runtime_injection
    gic720ae_messreg
    apollo_cpu_ras
    apollo_sbist
    zena_fmu
    zena_device_fmu
    zena_ni710ae_fmu
    zena_ssu
    zena_watchdog
    zena_reset_ctrl
    host_cmn_cyprus
    host_gtimer
    host_ni710ae_nci
    host_ppu
    cc3xx
    dma350
    rse_atu
    rse_integrity_checker
    rse_kmu
    rse_lcm
    rse_ppc_filter
    rse_protection_ctrl
    rse_sam
    strata_flash_j3
    pflash_cfi
    host_smcf_mgi
    host_system_pll
    signal_fanout
    signal_or
    reset_fanout
    rse_sysctrl
)

set(QBOX_APOLLO_MISSING_TARGETS "")
foreach(apollo_target IN LISTS QBOX_APOLLO_REQUIRED_TARGETS)
    if(NOT TARGET "${apollo_target}")
        list(APPEND QBOX_APOLLO_MISSING_TARGETS "${apollo_target}")
    endif()
endforeach()
if(QBOX_APOLLO_MISSING_TARGETS)
    message(
        FATAL_ERROR
        "Missing Apollo FVP QBox target(s): ${QBOX_APOLLO_MISSING_TARGETS}"
    )
endif()

if(TARGET "${QBOX_APOLLO_BUILD_TARGET}")
    message(FATAL_ERROR "Target already exists: ${QBOX_APOLLO_BUILD_TARGET}")
endif()
add_custom_target(
    ${QBOX_APOLLO_BUILD_TARGET}
    DEPENDS ${QBOX_APOLLO_REQUIRED_TARGETS}
)
