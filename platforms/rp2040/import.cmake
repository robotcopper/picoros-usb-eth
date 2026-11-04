# Before project(): the Pico SDK, from PICO_SDK_PATH, and the FreeRTOS kernel,
# from FREERTOS_KERNEL_PATH, each given with -D or in the environment.

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT DEFINED PICO_SDK_PATH)
  message(FATAL_ERROR "PICO_SDK_PATH is not set.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
include("${PICOROS_USB_ETH_DIR}/third_party/zenoh-pico/examples/rpi_pico/FreeRTOS_Kernel_import.cmake")
