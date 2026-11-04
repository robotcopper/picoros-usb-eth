# Include before project(). It imports what the platform needs before
# project(), the Pico SDK and the FreeRTOS kernel for rp2040, and defines
# picoros_usb_eth_init() and picoros_usb_eth_add_firmware(), see
# cmake/picoros_usb_eth.cmake.
#
#   set(PICOROS_USB_ETH_PATH <checkout> CACHE PATH "")
#   include("${PICOROS_USB_ETH_PATH}/picoros_usb_eth_import.cmake")
#   project(<app> C CXX ASM)
#   pico_sdk_init()
#   picoros_usb_eth_init(TYPES_HEADER <header> TYPES_DIR <dir>)
#   add_executable(<app> <sources defining app_main()>)
#   picoros_usb_eth_add_firmware(<app>)

set(PICOROS_USB_ETH_PLATFORM "rp2040" CACHE STRING "platform, a directory of platforms/")

set(PICOROS_USB_ETH_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(PICOROS_USB_ETH_PLATFORM_DIR "${PICOROS_USB_ETH_DIR}/platforms/${PICOROS_USB_ETH_PLATFORM}")

if(NOT EXISTS "${PICOROS_USB_ETH_PLATFORM_DIR}/platform.cmake")
  message(FATAL_ERROR "picoros-usb-eth has no platform ${PICOROS_USB_ETH_PLATFORM} in ${PICOROS_USB_ETH_DIR}/platforms")
endif()

include("${PICOROS_USB_ETH_PLATFORM_DIR}/import.cmake")
include("${PICOROS_USB_ETH_DIR}/cmake/picoros_usb_eth.cmake")
