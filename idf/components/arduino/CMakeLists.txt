# Compile the unchanged Arduino 3.0.7 core and Wire against our own IDF.
# Other Arduino libraries and their optional cloud/network dependencies are unused.
set(core "$ENV{AROSE_ARDUINO_CORE}")
file(GLOB sources "${core}/cores/esp32/*.c" "${core}/cores/esp32/*.cpp"
                  "${core}/cores/esp32/libb64/*.c")
list(APPEND sources "${core}/libraries/Wire/src/Wire.cpp")
idf_component_register(SRCS ${sources}
    INCLUDE_DIRS "${core}/cores/esp32" "${core}/variants/esp32s3" "${core}/libraries/Wire/src"
    PRIV_INCLUDE_DIRS "${core}/cores/esp32/libb64"
    REQUIRES driver esp_timer esp_psram esp_adc esp_wifi esp_eth esp_netif
             spi_flash esp_partition mbedtls wpa_supplicant http_parser
    PRIV_REQUIRES fatfs nvs_flash app_update spiffs bootloader_support bt esp_hid usb)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    ARDUINO=10812 ARDUINO_ARCH_ESP32 ESP32=ESP32
    ARDUINO_ESP32S3_DEV ARDUINO_BOARD="ESP32S3_DEV" ARDUINO_VARIANT="esp32s3"
    ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1 BOARD_HAS_PSRAM F_CPU=240000000L)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _Z5setupv -u _Z4loopv")
