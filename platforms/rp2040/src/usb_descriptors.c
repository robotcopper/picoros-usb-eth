/*
 * USB device: a CDC-NCM network function and the Raspberry Pi reset
 * interface.
 *
 * The VID:PID is the Pico SDK's stdio USB one, 2E8A:000A, which picotool
 * recognises as a running board and which the udev rule shipped with picotool
 * grants access to. picotool then looks for the reset interface, a vendor
 * interface without endpoints, and asks it to reboot into BOOTSEL. Its class
 * driver below follows the SDK's pico_stdio_usb/reset_interface.c.
 */

#include "device/usbd_pvt.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include "pico/usb_reset_interface.h"
#include "tusb.h"

#define USBD_VID 0x2E8A
#define USBD_PID 0x000A
#define USBD_MAX_POWER_MA 250

enum { ITF_NCM = 0, ITF_NCM_DATA, ITF_RESET, ITF_COUNT };
enum { STR_LANGID = 0, STR_MANUFACTURER, STR_PRODUCT, STR_SERIAL, STR_NCM, STR_MAC, STR_RESET, STR_COUNT };

#define EP_NCM_NOTIFY 0x81
#define EP_NCM_OUT 0x02
#define EP_NCM_IN 0x82
#define EP_NCM_NOTIFY_SIZE 64

#define RESET_DESC_LEN 9
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN + RESET_DESC_LEN)

#define RESET_TO_FLASH_DELAY_MS 100
/* Low bits of wValue in a BOOTSEL request: the bootrom interfaces to disable. */
#define RESET_DISABLE_MASK 0x7f

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USBD_VID,
    .idProduct = USBD_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = STR_MANUFACTURER,
    .iProduct = STR_PRODUCT,
    .iSerialNumber = STR_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t desc_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_TOTAL_LEN, 0, USBD_MAX_POWER_MA),
    TUD_CDC_NCM_DESCRIPTOR(ITF_NCM, STR_NCM, STR_MAC, EP_NCM_NOTIFY, EP_NCM_NOTIFY_SIZE, EP_NCM_OUT, EP_NCM_IN,
                           CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU),
    RESET_DESC_LEN, TUSB_DESC_INTERFACE, ITF_RESET, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, RESET_INTERFACE_SUBCLASS,
    RESET_INTERFACE_PROTOCOL, STR_RESET,
};
TU_VERIFY_STATIC(sizeof(desc_config) == CONFIG_TOTAL_LEN, "configuration descriptor length");

const uint8_t *tud_descriptor_device_cb(void) { return (const uint8_t *)&desc_device; }

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_config;
}

#define STR_MAX_CHARS 32

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  static uint16_t desc[1 + STR_MAX_CHARS];
  char text[STR_MAX_CHARS + 1];
  const char *str;

  switch (index) {
    case STR_LANGID:
      desc[1] = 0x0409;
      desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
      return desc;
    case STR_MANUFACTURER:
      str = "Raspberry Pi";
      break;
    case STR_PRODUCT:
      str = "Pico";
      break;
    case STR_SERIAL:
      pico_get_unique_board_id_string(text, sizeof(text));
      str = text;
      break;
    case STR_NCM:
      str = "Board NCM";
      break;
    case STR_MAC:
      for (int i = 0; i < 6; ++i) {
        static const char hex[] = "0123456789ABCDEF";
        text[2 * i] = hex[tud_network_mac_address[i] >> 4];
        text[2 * i + 1] = hex[tud_network_mac_address[i] & 0xf];
      }
      text[12] = '\0';
      str = text;
      break;
    case STR_RESET:
      str = "Reset";
      break;
    default:
      return NULL;
  }

  uint8_t len = 0;
  for (; len < STR_MAX_CHARS && str[len] != '\0'; ++len) {
    desc[1 + len] = (uint8_t)str[len];
  }
  desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
  return desc;
}

static uint8_t reset_itf;

static void resetd_init(void) {}

static void resetd_reset(uint8_t rhport) {
  (void)rhport;
  reset_itf = 0;
}

static uint16_t resetd_open(uint8_t rhport, const tusb_desc_interface_t *itf_desc, uint16_t max_len) {
  (void)rhport;
  TU_VERIFY(itf_desc->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC &&
                itf_desc->bInterfaceSubClass == RESET_INTERFACE_SUBCLASS &&
                itf_desc->bInterfaceProtocol == RESET_INTERFACE_PROTOCOL,
            0);
  TU_VERIFY(max_len >= sizeof(tusb_desc_interface_t), 0);
  reset_itf = itf_desc->bInterfaceNumber;
  return sizeof(tusb_desc_interface_t);
}

static bool resetd_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) {
  (void)rhport;
  if (stage != CONTROL_STAGE_SETUP) {
    return true;
  }
  if (request->wIndex != reset_itf) {
    return false;
  }
  if (request->bRequest == RESET_REQUEST_BOOTSEL) {
    reset_usb_boot(0, request->wValue & RESET_DISABLE_MASK);
  }
  if (request->bRequest == RESET_REQUEST_FLASH) {
    watchdog_reboot(0, 0, RESET_TO_FLASH_DELAY_MS);
    return true;
  }
  return false;
}

static bool resetd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)rhport;
  (void)ep_addr;
  (void)result;
  (void)xferred_bytes;
  return true;
}

static const usbd_class_driver_t resetd_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "RESET",
#endif
    .init = resetd_init,
    .reset = resetd_reset,
    .open = resetd_open,
    .control_xfer_cb = resetd_control_xfer_cb,
    .xfer_cb = resetd_xfer_cb,
    .sof = NULL,
};

const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *driver_count) {
  *driver_count = 1;
  return &resetd_driver;
}
