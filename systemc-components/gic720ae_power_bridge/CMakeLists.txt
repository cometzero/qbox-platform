add_library(
    gic720ae_power_bridge
    SHARED
    src/gic720ae_power_bridge.cpp
)
set_target_properties(
    gic720ae_power_bridge
    PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
if(TARGET_LIBS)
    add_dependencies(gic720ae_power_bridge ${TARGET_LIBS})
    target_link_libraries(gic720ae_power_bridge PUBLIC ${TARGET_LIBS})
endif()
foreach(target IN LISTS TARGET_LIBS)
    target_include_directories(
        gic720ae_power_bridge
        PUBLIC $<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>
    )
endforeach()
target_include_directories(
    gic720ae_power_bridge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}>
)
install(
    TARGETS gic720ae_power_bridge
    EXPORT qqvpTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT "${LIBRARY_NAME}_Runtime"
        NAMELINK_COMPONENT "${LIBRARY_NAME}_Development"
)
