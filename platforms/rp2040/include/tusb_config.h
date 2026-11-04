#ifndef PICOROS_USB_ETH_TUSB_CONFIG_H_
#define PICOROS_USB_ETH_TUSB_CONFIG_H_

/*
 * TinyUSB device with one CDC-NCM function, the network link, and the
 * Raspberry Pi reset interface picotool uses, which is a custom class driver
 * in usb_descriptors.c and needs no class here.
 */

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

/* CFG_TUSB_OS is the SDK's OPT_OS_PICO, whose event queue is guarded by a
   critical section rather than by masking the USB interrupt on one core. */
#define CFG_TUSB_DEBUG 0
#define CFG_TUD_ENABLED 1
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_NCM 1
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

/* 3.2 kB blocks. TinyUSB measured on the RP2040 that three transmit blocks
   never leave tud_network_can_xmit() refusing, and that two receive blocks
   gain up to 30 % on small packets. */
#define CFG_TUD_NCM_IN_NTB_N 3
#define CFG_TUD_NCM_OUT_NTB_N 2
/* One datagram per received block. Allowed more, the Linux cdc_ncm driver
   holds each frame up to three times tx_timer_usecs, 1.2 ms by default, for
   others to fill the block; that delays every command and makes the SNTP
   round trip asymmetric. */
#define CFG_TUD_NCM_OUT_MAX_DATAGRAMS_PER_NTB 1

#endif /* PICOROS_USB_ETH_TUSB_CONFIG_H_ */
