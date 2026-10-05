add_executable(vmcu_gpio_bridge-tests vmcu_gpio_bridge-tests.cc)
target_link_libraries(vmcu_gpio_bridge-tests PRIVATE gtest gmock vmcu_gpio_bridge ${TARGET_LIBS})
add_test(NAME vmcu_gpio_bridge-tests COMMAND vmcu_gpio_bridge-tests)
set_tests_properties(vmcu_gpio_bridge-tests PROPERTIES TIMEOUT 20)
