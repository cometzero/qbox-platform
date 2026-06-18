cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(qbox_platform VERSION 0.1)

get_filename_component(_qbox_platform_source_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_qbox_platform_tools_dir "${_qbox_platform_source_dir}/.." ABSOLUTE)

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
if(DEFINED QEMU_SOURCE_DIR AND QEMU_SOURCE_DIR)
    set(_qbox_platform_default_qemu_source_dir "${QEMU_SOURCE_DIR}")
elseif(EXISTS "${_qbox_platform_tools_dir}/qemu/CMakeLists.txt")
    set(_qbox_platform_default_qemu_source_dir "${_qbox_platform_tools_dir}/qemu")
endif()
set(
    QBOX_QEMU_SOURCE_DIR
    "${_qbox_platform_default_qemu_source_dir}"
    CACHE PATH
    "Path to the qemu/libqemu source tree"
)

if(QBOX_QEMU_SOURCE_DIR)
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
if(QBOX_QEMU_SOURCE_DIR)
    message(STATUS "QBOX_QEMU_SOURCE_DIR = ${QBOX_QEMU_SOURCE_DIR}")
endif()

add_subdirectory("${QBOX_CORE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/qbox-core")

if(NOT TARGET qbox)
    message(FATAL_ERROR "QBOX_CORE_SOURCE_DIR did not define the qbox target")
endif()
set(TARGET_LIBS qbox)

add_subdirectory(systemc-components)
add_subdirectory(qemu-components/cc3xx_native)

set(
    QBOX_APOLLO_BUILD_TARGET
    "apollo_fvp_full_system"
    CACHE STRING
    "Aggregate target for Apollo FVP QBox runtime dependencies"
)

set(QBOX_APOLLO_REQUIRED_TARGETS
    platforms-vp
    keep_alive
    addrtr
    router
    gs_memory
    host_scr
    loader
    char_backend_file
    char_backend_stdio
    uart-pl011
    global_peripheral_initiator
    cpu_arm_cortexA720AE
    cpu_arm_cortexR82
    arm_gicv3
    arm_gicv3_its
    qemu_gpex
    virtio_mmio_blk
    virtio_mmio_net
    virtio_mmio_rng
    arm_smmuv3
    mmu720ae
    reset_gpio
    pl031
    sbsa_gwdt
    cpu_arm_cortexM55
    nvic_armv7m
    remote_cpu
    qemu_cc3xx
    qemu_hexagon_qtimer
    mhu320ae
    gicx00_multiview
    zena_fmu
    zena_ssu
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
    rse_protection_ctrl
    rse_sam
    strata_flash_j3
    host_smcf_mgi
    host_system_pll
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
