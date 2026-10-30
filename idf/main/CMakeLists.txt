set(firmware "$ENV{AROSE_PROJECT}/ARoseWaveShare")
file(GLOB sources "${firmware}/*.cpp")
idf_component_register(SRCS "main.cpp" ${sources}
    INCLUDE_DIRS "${firmware}"
    REQUIRES arduino esp_lcd nvs_flash esp_psram)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
target_compile_definitions(${COMPONENT_LIB} PRIVATE AROSE_REQUIRE_XIP=1 AROSE_START_FRAME=${AROSE_START_FRAME})
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_flash_erase_region")
