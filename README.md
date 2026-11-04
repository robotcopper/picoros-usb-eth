# picoros-usb-eth

A microcontroller as an ordinary ROS 2 node, over one USB cable.

The board shows up on the host as a USB Ethernet adapter (CDC-NCM, a standard
USB class with a built-in driver on Linux, macOS and recent Windows), connects
over TCP to the `rmw_zenoh_cpp` router, takes the host's clock over SNTP, and is
flashed with `picotool` while it runs. On the board,
[Pico-ROS](https://github.com/Pico-ROS/Pico-ROS-software) maps ROS entities onto
[zenoh-pico](https://github.com/eclipse-zenoh/zenoh-pico): there is no
micro-ROS agent, no bridge and no serial protocol, and the stock `ros2` tools
see the board's topics and services directly.

## Platforms

| Platform | Chip | Built on | Status |
| --- | --- | --- | --- |
| `rp2040` | RP2040 (Raspberry Pi Pico) | Pico SDK, FreeRTOS, lwIP, TinyUSB | validated on hardware |

Other platforms are welcome, see [Adding a platform](#adding-a-platform). A
platform is listed as validated once it has run on hardware.

Measured on the RP2040 with ROS 2 Lyrical, the host on Linux:

| Quantity | Result |
| --- | --- |
| ICMP round trip | 0.92 ms mean |
| `JointState` host to board at 100 Hz | 1.5 ms mean, 0.95 to 2.0 ms |
| Clock step at each 15 s SNTP update | about 0.1 ms |
| Idle time, 100 Hz in each direction | 81 % |

## Layout

```text
picoros_usb_eth_import.cmake   included by a firmware before project()
cmake/picoros_usb_eth.cmake    picoros_usb_eth_init(), picoros_usb_eth_add_firmware()
platforms/<platform>/          the link, the clock and main() for one chip
examples/<platform>_basic/     a complete firmware, also the platform's test
third_party/                   zenoh-pico, Pico-ROS and Micro-CDR, pinned
tools/observe.py               dumps what rmw_zenoh_cpp puts on the wire
```

## Using it in a firmware

Add it to the firmware's repository:

```sh
git submodule add https://github.com/robotcopper/picoros-usb-eth.git lib/picoros-usb-eth
git submodule update --init --recursive
```

Generate the header of the message types the firmware exchanges with Pico-ROS's
generator, `third_party/Pico-ROS/tools/type-gen`, then:

```cmake
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD "pico" CACHE STRING "")
set(PICOROS_USB_ETH_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/picoros-usb-eth" CACHE PATH "")
include("${PICOROS_USB_ETH_PATH}/picoros_usb_eth_import.cmake")

project(my_board C CXX ASM)
pico_sdk_init()

picoros_usb_eth_init(TYPES_HEADER my_types.h TYPES_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

add_executable(my_board src/app_main.c)
picoros_usb_eth_add_firmware(my_board)
```

The firmware defines `int app_main(void)`, which the platform's `main()` runs in
a FreeRTOS task. It starts the link with `usb_net_start()` from
`picoros_usb_eth/usb_net.h`, waits for `usb_net_host_reachable()`, and opens its
Pico-ROS session on `ZENOH_CONFIG_CONNECT`. `examples/rp2040_basic` is a complete
one, with publishers, a subscriber and the restart on a lost session.

```sh
export PICO_SDK_PATH=<pico-sdk> FREERTOS_KERNEL_PATH=<FreeRTOS-Kernel>
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
picotool load -f -x build/my_board.uf2
```

Defaults, to change with `-D` or with `set(... CACHE ...)` before
`picoros_usb_eth_init()`, or before the import for the platform:

| Variable | Default | Meaning |
| --- | --- | --- |
| `PICOROS_USB_ETH_PLATFORM` | `rp2040` | directory of `platforms/` |
| `USB_NET_DEVICE_IP` | `192.168.7.1` | board address |
| `USB_NET_HOST_IP` | `192.168.7.2` | address leased to the host, and SNTP server |
| `USB_NET_NETMASK` | `255.255.255.0` | link netmask |
| `ZENOH_CONFIG_MODE` | `client` | Zenoh mode |
| `ZENOH_CONFIG_CONNECT` | `tcp/<USB_NET_HOST_IP>:7447` | router endpoint |
| `Z_TRANSPORT_LEASE` | `60000` | lease announced to the router, in ms |
| `Z_TRANSPORT_LEASE_EXPIRE_FACTOR` | `12` | keepalive attempts per lease |

## Host setup

* **Network.** The board leases `USB_NET_HOST_IP` to the host, with neither
  gateway nor DNS, so a default DHCP profile is enough and the link adds only a
  /24 route. The interface is named after a MAC address derived from the
  board's unique ID, so its name survives reboots.
* **Router.** `ros2 run rmw_zenoh_cpp rmw_zenohd`, with its default
  configuration.
* **Clock.** `chrony` serving the link even without an upstream source:
  `allow 192.168.7.0/24` and `local stratum 10`. A board waits for its first
  SNTP answer before it stamps any data.
* **Firewall.** TCP 7447 and UDP 123 open to the link's subnet.
* **Flashing.** The udev rule shipped with `picotool`, to flash without root.

## Behaviour worth knowing

* zenoh-pico expires the router after the shorter of the two leases, and the
  router's default configuration sends a keepalive only every 30 s, hence the
  60 s lease. A keepalive is tried every 5 s and sent only when nothing else has
  been: a stopped router is detected 5 to 10 s later, a silent link only when
  the lease runs out, after 60 to 120 s.
* Closing a zenoh-pico session and opening another hangs the `rpi_pico` port,
  so the examples restart the board when their session ends.
* The board accepts one datagram per USB transfer. Otherwise Linux's
  `cdc_ncm` holds each frame for up to 1.2 ms, waiting for others to batch
  with it.
* zenoh-pico writes `include/zenoh-pico/config.h` into its own source tree at
  configure time, so its submodule always shows modified files; `.gitmodules`
  ignores them. Two builds of one checkout with different options overwrite
  each other's: give each its own checkout.

## Pinned forks

`third_party/` pins zenoh-pico 1.8.0, the Zenoh version Lyrical's
`rmw_zenoh_cpp` bundles, and Pico-ROS, both from forks on the branch
`picoros-usb-eth`, until their fixes are merged upstream:

* zenoh-pico decoded a batch holding a best-effort frame followed by a reliable
  one as a single frame, and its read task stopped. `rmw_zenohd` sends such
  batches under load.
* zenoh-pico called a query reply callback after releasing its lock.
* zenoh-pico's `rpi_pico` network layer did not build a TCP link without the
  Pico W's Wi-Fi chip.
* Pico-ROS stamped the `rmw_zenoh_cpp` attachment with the time since boot
  instead of the wall clock.
* Pico-ROS resolved `~` in service names but not in topic names.
* Pico-ROS's type generator could give a service the hash of its request.

## Adding a platform

A platform is a directory `platforms/<name>/`:

* `import.cmake`, included before `project()`: imports the SDK and the RTOS.
* `platform.cmake`, included by `picoros_usb_eth_init()`. It selects
  zenoh-pico's system layer and defines the targets
  `picoros_usb_eth_<name>_headers` and `picoros_usb_eth_<name>`, the variables
  `PICOROS_USB_ETH_ZENOH_DEPS` and `PICOROS_USB_ETH_LIBRARY_DEPS`, and the
  function `picoros_usb_eth_platform_firmware(<target>)`. `platforms/rp2040`
  documents each.
* The sources: a `main()` that runs `app_main()` in a task, and
  `include/picoros_usb_eth/usb_net.h` and `net_time.h` with the API of the
  `rp2040` ones. Every SNTP update also goes to `settimeofday()`, which is what
  zenoh-pico's timestamps and Pico-ROS's attachments read.
* `examples/<name>_basic`, the firmware that validates it: the node in
  `ros2 node list`, topics echoed in both directions, a service called, and
  `tools/observe.py` to compare what goes on the wire.

## Licence

BSD-3-Clause, see [LICENSE](LICENSE). The components in `third_party/` keep
their own: zenoh-pico is EPL-2.0 or Apache-2.0, Pico-ROS BSD-3-Clause and
Micro-CDR Apache-2.0.
