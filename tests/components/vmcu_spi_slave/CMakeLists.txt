add_executable(vmcu_spi_slave-tests vmcu_spi_slave-tests.cc)
target_link_libraries(vmcu_spi_slave-tests PRIVATE gtest gmock vmcu_spi_slave ${TARGET_LIBS})
add_test(NAME vmcu_spi_slave-tests COMMAND vmcu_spi_slave-tests)
set_tests_properties(vmcu_spi_slave-tests PROPERTIES TIMEOUT 20)
