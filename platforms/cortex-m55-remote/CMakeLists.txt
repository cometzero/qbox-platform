set(LIBQEMU_TARGETS aarch64)

add_library(ApolloRseRemoteCPU SHARED
    src/apollo_rse_remote_cpu_module.cc
)

target_link_libraries(ApolloRseRemoteCPU
    rse_cpu_accel
    ${TARGET_LIBS}
)

target_include_directories(ApolloRseRemoteCPU PUBLIC
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/systemc-components>
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components>
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components/common/include>
    $<BUILD_INTERFACE:${QBOX_PLATFORM_SOURCE_DIR}/qemu-components/rse_cpu_accel/include>
)

set_target_properties(ApolloRseRemoteCPU PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib;$ORIGIN/../lib/libqemu;$ORIGIN/../../../lib"
)

add_executable(apollo_rse_remote_cpu
    src/apollo_rse_remote_cpu.cc
)

target_link_libraries(apollo_rse_remote_cpu
    rse_cpu_accel
    ${TARGET_LIBS}
)

target_include_directories(apollo_rse_remote_cpu PUBLIC
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/systemc-components>
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components>
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components/common/include>
    $<BUILD_INTERFACE:${QBOX_PLATFORM_SOURCE_DIR}/qemu-components/rse_cpu_accel/include>
)

set_target_properties(apollo_rse_remote_cpu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib;$ORIGIN/../lib/libqemu;$ORIGIN/../../../lib"
)
