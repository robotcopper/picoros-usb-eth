cmake_minimum_required(VERSION 3.13)

# A Pico-ROS node on a bare Raspberry Pi Pico over Ethernet over USB and TCP,
# stamped with the host's clock over SNTP, and flashed with picotool through
# the reset interface. Its topics are listed in src/app_main.c.

set(PICO_BOARD "pico" CACHE STRING "Pico SDK board")
set(CMAKE_C_STANDARD 11)

include("${CMAKE_CURRENT_LIST_DIR}/../../picoros_usb_eth_import.cmake")

project(rp2040_basic C CXX ASM)

pico_sdk_init()

picoros_usb_eth_init(
  TYPES_HEADER example_types.h
  TYPES_DIR "${PICOROS_USB_ETH_DIR}/third_party/Pico-ROS/examples"
  DEBUG
)

add_executable(rp2040_basic src/app_main.c)
picoros_usb_eth_add_firmware(rp2040_basic)
