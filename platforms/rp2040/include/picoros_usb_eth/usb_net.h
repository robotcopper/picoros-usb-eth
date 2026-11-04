#ifndef PICOROS_USB_ETH_USB_NET_H_
#define PICOROS_USB_ETH_USB_NET_H_

/*
 * Ethernet over USB (CDC-NCM) to the host, next to the reset interface that
 * picotool uses to put the board in BOOTSEL mode.
 *
 * The board is USB_NET_DEVICE_IP and leases USB_NET_HOST_IP to the host with
 * neither router nor DNS server, so the link only adds a /24 route on the
 * host. The host is also the SNTP server, see net_time.h. Both addresses are
 * compile definitions set by platform.cmake.
 *
 * The host's MAC address, and so its interface name, enx<mac>, is derived from
 * the board's unique ID and stays the same across reboots.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t rx_frames;
  uint32_t tx_frames;
  /* Frames lost to a full tcpip mailbox. */
  uint32_t rx_dropped;
  /* Receptions postponed because the lwIP heap was full; TinyUSB keeps the
     frame and it is retried. */
  uint32_t rx_deferred;
  /* Frames lost because no USB transmit block freed up in time. */
  uint32_t tx_dropped;
} usb_net_stats_t;

/* Starts lwIP, the DHCP server, SNTP and then TinyUSB. Call once, from a task.
   Returns once lwIP is up, whether or not a host is attached. */
void usb_net_start(void);

/* The host has configured the USB device. */
bool usb_net_host_attached(void);

/* The host has also configured its address and answered ARP for it, which
   the board asks every 200 ms until it does. Before that, a TCP connection
   attempt blocks through every SYN retransmission. */
bool usb_net_host_reachable(void);

void usb_net_get_stats(usb_net_stats_t *stats);

#endif /* PICOROS_USB_ETH_USB_NET_H_ */
