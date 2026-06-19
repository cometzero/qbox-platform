add_library(rse_cpu_accel STATIC
    src/rse_cpu_accel.cc
)

target_include_directories(rse_cpu_accel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${QBOX_CORE_SOURCE_DIR}/qemu-components/common/include
    ${QBOX_PLATFORM_SOURCE_DIR}/systemc-components/cc3xx/include
)

target_link_libraries(rse_cpu_accel PUBLIC ${TARGET_LIBS})
