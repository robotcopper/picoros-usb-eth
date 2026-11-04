# Ethernet over USB (CDC-NCM) for an RP2040 running zenoh-pico over TCP, with
# the picotool reset interface and the wall clock from the host over SNTP.
#
# picoros_usb_eth_init() includes it after pico_sdk_init(). Like every
# platform, it defines:
#   picoros_usb_eth_rp2040_headers  link into libraries that include the lwIP
#                                   or FreeRTOS headers, zenoh-pico included
#   picoros_usb_eth_rp2040          the platform sources, main() included
#   PICOROS_USB_ETH_ZENOH_DEPS      what zenoh-pico links privately
#   PICOROS_USB_ETH_LIBRARY_DEPS    what Pico-ROS links publicly
#   picoros_usb_eth_platform_firmware(<target>)
#
# Its include directory holds the only FreeRTOSConfig.h, lwipopts.h and
# tusb_config.h of the build: do not add zenoh-pico's example include
# directory, whose lwipopts.h is for the CYW43.

set(PICOROS_USB_ETH_RP2040_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(PICOROS_USB_ETH_RP2040_DHSERVER_DIR "${PICO_SDK_PATH}/lib/tinyusb/lib/networking")

# zenoh-pico's rpi_pico system layer.
set(WITH_RPI_PICO ON)

add_library(picoros_usb_eth_rp2040_headers INTERFACE)
target_include_directories(picoros_usb_eth_rp2040_headers INTERFACE
  "${PICOROS_USB_ETH_RP2040_DIR}/include"
  "${PICOROS_USB_ETH_RP2040_DHSERVER_DIR}"
)
target_compile_definitions(picoros_usb_eth_rp2040_headers INTERFACE
  USB_NET_DEVICE_IP="${USB_NET_DEVICE_IP}"
  USB_NET_HOST_IP="${USB_NET_HOST_IP}"
  USB_NET_NETMASK="${USB_NET_NETMASK}"
)
target_link_libraries(picoros_usb_eth_rp2040_headers INTERFACE
  pico_lwip_headers
  pico_lwip_contrib_freertos_headers
)

add_library(picoros_usb_eth_rp2040 INTERFACE)
target_sources(picoros_usb_eth_rp2040 INTERFACE
  "${PICOROS_USB_ETH_RP2040_DIR}/src/freertos_hooks.c"
  "${PICOROS_USB_ETH_RP2040_DIR}/src/main.c"
  "${PICOROS_USB_ETH_RP2040_DIR}/src/net_time.c"
  "${PICOROS_USB_ETH_RP2040_DIR}/src/usb_descriptors.c"
  "${PICOROS_USB_ETH_RP2040_DIR}/src/usb_net.c"
  "${PICOROS_USB_ETH_RP2040_DHSERVER_DIR}/dhserver.c"
)
target_link_libraries(picoros_usb_eth_rp2040 INTERFACE
  picoros_usb_eth_rp2040_headers
  FreeRTOS-Kernel-Heap4
  hardware_watchdog
  pico_bootrom
  pico_lwip
  pico_lwip_contrib_freertos
  pico_lwip_sntp
  pico_unique_id
  pico_usb_reset_interface_headers
  tinyusb_device
)

# FreeRTOS has no header-only target: libraries link FreeRTOS-Kernel-Heap4
# themselves, and the executable's own copy of the kernel is the one linked.
set(PICOROS_USB_ETH_ZENOH_DEPS
  picoros_usb_eth_rp2040_headers
  pico_stdlib
  pico_rand
  FreeRTOS-Kernel-Heap4
)
set(PICOROS_USB_ETH_LIBRARY_DEPS
  picoros_usb_eth_rp2040_headers
  pico_stdlib
  FreeRTOS-Kernel-Heap4
)

function(picoros_usb_eth_platform_firmware target)
  target_link_libraries(${target} PRIVATE
    picoros_usb_eth_rp2040
    hardware_watchdog
    pico_stdlib
  )
  # USB belongs to the network link: printf goes out on UART0 (GP0, GP1).
  pico_enable_stdio_uart(${target} 1)
  pico_enable_stdio_usb(${target} 0)
  pico_add_extra_outputs(${target})
endfunction()
