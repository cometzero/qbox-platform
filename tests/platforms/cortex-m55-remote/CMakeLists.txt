find_package(Python3 COMPONENTS Interpreter REQUIRED)

get_filename_component(
    QBOX_PLATFORM_WORKSPACE_DIR
    "${QBOX_PLATFORM_SOURCE_DIR}/../.."
    ABSOLUTE
)
set(
    ARM_NONE_EABI_HINT
    "${QBOX_PLATFORM_WORKSPACE_DIR}/build/tmp_baremetal/sysroots-components/x86_64/gcc-arm-none-eabi-native/usr/libexec/gcc-arm-none-eabi-13.3.rel1/bin"
    CACHE PATH
    "Optional path to Yocto-provided arm-none-eabi tools"
)

find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc HINTS ${ARM_NONE_EABI_HINT})
find_program(ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy HINTS ${ARM_NONE_EABI_HINT})

if(TARGET cortex-m55-vp AND ARM_NONE_EABI_GCC AND ARM_NONE_EABI_OBJCOPY)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.elf
        COMMAND ${ARM_NONE_EABI_GCC}
                -mcpu=cortex-m55
                -mthumb
                -nostdlib
                -Wl,--build-id=none
                -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/cortex-m55-remote-dmi-byte-store.ld
                -o ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.elf
                ${CMAKE_CURRENT_SOURCE_DIR}/cortex-m55-remote-dmi-byte-store.S
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cortex-m55-remote-dmi-byte-store.S
                ${CMAKE_CURRENT_SOURCE_DIR}/cortex-m55-remote-dmi-byte-store.ld
        COMMENT "Building cortex-m55 remote DMI byte-store firmware"
    )

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.bin
        COMMAND ${ARM_NONE_EABI_OBJCOPY}
                -O binary
                ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.elf
                ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.bin
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.elf
        COMMENT "Creating cortex-m55 remote DMI byte-store binary"
    )

    add_custom_target(cortex-m55-remote-dmi-byte-store-firmware
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.bin)
    add_dependencies(cortex-m55-vp cortex-m55-remote-dmi-byte-store-firmware)

    add_test(
        NAME cortex_m55_remote_dmi_byte_store_on
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m55_dmi.py
                -e $<TARGET_FILE:cortex-m55-vp>
                -l ${CMAKE_CURRENT_SOURCE_DIR}/cortex-m55-dmi.lua
                -f ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.bin
                --dmi true)
    set_tests_properties(cortex_m55_remote_dmi_byte_store_on PROPERTIES TIMEOUT 15)

    add_test(
        NAME cortex_m55_remote_dmi_byte_store_off
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/cortex_m55_dmi.py
                -e $<TARGET_FILE:cortex-m55-vp>
                -l ${CMAKE_CURRENT_SOURCE_DIR}/cortex-m55-dmi.lua
                -f ${CMAKE_CURRENT_BINARY_DIR}/cortex-m55-remote-dmi-byte-store.bin
                --dmi false)
    set_tests_properties(cortex_m55_remote_dmi_byte_store_off PROPERTIES TIMEOUT 15)
else()
    message(WARNING "Skipping cortex-m55 remote DMI byte-store tests")
endif()
