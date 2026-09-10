add_executable(dma350-peripherals-tests dma350-peripherals-tests.cc)
target_link_libraries(dma350-peripherals-tests PRIVATE
    gtest gmock dma350 dw-apb-i2c dw-apb-ssi dw-apb-uart ${TARGET_LIBS})
add_test(NAME dma350-peripherals-i2c COMMAND dma350-peripherals-tests
    --gtest_filter=TestBench__I2cEepromTxRxUsesDreqAndIrq.I2cEepromTxRxUsesDreqAndIrq)
add_test(NAME dma350-peripherals-spi COMMAND dma350-peripherals-tests
    --gtest_filter=TestBench__SpiLoopbackUsesTwoDmaChannels.SpiLoopbackUsesTwoDmaChannels)
add_test(NAME dma350-peripherals-uart COMMAND dma350-peripherals-tests
    --gtest_filter=TestBench__UartCrossconnectUsesTxAndRxDma.UartCrossconnectUsesTxAndRxDma)
set_tests_properties(dma350-peripherals-i2c dma350-peripherals-spi dma350-peripherals-uart PROPERTIES
    LABELS qbox-platform-dma350
    TIMEOUT 20)
