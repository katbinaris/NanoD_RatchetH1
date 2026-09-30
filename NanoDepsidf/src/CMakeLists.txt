# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources} REQUIRES driver esp_driver_i2c esp_driver_tsens nvs_flash)
# joltwallet/littlefs and espressif/cjson (idf_component.yml) are linked in by the component
# manager, like esp_tinyusb and led_strip.
