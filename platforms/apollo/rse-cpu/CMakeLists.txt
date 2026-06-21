add_library(ApolloRseCPU SHARED
    src/apollo_rse_cpu_module.cc
)

target_link_libraries(ApolloRseCPU
    rse_cpu_accel
    ${TARGET_LIBS}
)

target_include_directories(ApolloRseCPU PUBLIC
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/systemc-components>
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components>
    $<BUILD_INTERFACE:${QBOX_CORE_SOURCE_DIR}/qemu-components/common/include>
    $<BUILD_INTERFACE:${QBOX_PLATFORM_SOURCE_DIR}/qemu-components/rse_cpu_accel/include>
)

set_target_properties(ApolloRseCPU PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib;$ORIGIN/../lib/libqemu;$ORIGIN/../../../lib"
)
