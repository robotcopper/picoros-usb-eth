#include "picoros_usb_eth/usb_net.h"

#include <string.h>

#include "FreeRTOS.h"
#include "dhserver.h"
#include "lwip/apps/sntp.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "pico.h"
#include "pico/unique_id.h"
#include "semphr.h"
#include "task.h"
#include "tusb.h"

/* Above the tcpip thread: returning receive blocks to the host is what keeps
   the link moving. */
#define USB_TASK_PRIORITY (TCPIP_THREAD_PRIO + 2)
#define USB_TASK_STACK_WORDS 768
/* tud_task() runs on every USB event; this period only bounds the retry of a
   postponed reception. */
#define USB_POLL_MS 5
/* How long the tcpip thread waits for a free transmit block before dropping
   the frame. TCP retransmits it. */
#define TX_WAIT_MS 20
#define DHCP_LEASE_S (24 * 60 * 60)
/* How often the host is asked for its MAC address until it answers. */
#define HOST_PROBE_MS 200

/* The host's address on the link, read by TinyUSB's MAC string descriptor. */
uint8_t tud_network_mac_address[6];

static struct netif usb_netif;
static ip4_addr_t host_ip;
static SemaphoreHandle_t usb_mutex;
static TaskHandle_t usb_task_handle;
static volatile bool rx_retry = false;
static usb_net_stats_t stats;

static dhcp_entry_t dhcp_entries[1];
static dhcp_config_t dhcp_config = {
    .port = 67,
    .num_entry = 1,
    .entries = dhcp_entries,
};

/* Called by TinyUSB from the USB task, with usb_mutex held. */
bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_RAM);
  if (p == NULL) {
    ++stats.rx_deferred;
    rx_retry = true;
    return false;
  }
  memcpy(p->payload, src, size);
  if (usb_netif.input(p, &usb_netif) == ERR_OK) {
    ++stats.rx_frames;
  } else {
    pbuf_free(p);
    ++stats.rx_dropped;
  }
  /* Hands over the next datagram of the block, if any. */
  tud_network_recv_renew();
  return true;
}

/* Called by tud_network_xmit(), with usb_mutex held. */
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  (void)arg;
  struct pbuf *p = (struct pbuf *)ref;
  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

void tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr) {
  (void)rhport;
  (void)eventid;
  if (usb_task_handle == NULL) {
    return;
  }
  if (in_isr) {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(usb_task_handle, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    xTaskNotifyGive(usb_task_handle);
  }
}

/* lwIP output, in the tcpip thread. */
static err_t linkoutput(struct netif *netif, struct pbuf *p) {
  (void)netif;
  TickType_t start = xTaskGetTickCount();
  for (;;) {
    if (!tud_ready()) {
      ++stats.tx_dropped;
      return ERR_IF;
    }
    xSemaphoreTake(usb_mutex, portMAX_DELAY);
    bool sent = tud_network_can_xmit(p->tot_len);
    if (sent) {
      tud_network_xmit(p, 0);
    }
    xSemaphoreGive(usb_mutex);
    if (sent) {
      ++stats.tx_frames;
      return ERR_OK;
    }
    if (xTaskGetTickCount() - start > pdMS_TO_TICKS(TX_WAIT_MS)) {
      ++stats.tx_dropped;
      return ERR_MEM;
    }
    vTaskDelay(1);
  }
}

static err_t netif_setup(struct netif *netif) {
  netif->name[0] = 'u';
  netif->name[1] = 's';
  netif->mtu = CFG_TUD_NET_MTU - SIZEOF_ETH_HDR;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
  netif->linkoutput = linkoutput;
  netif->output = etharp_output;
  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, tud_network_mac_address, ETH_HWADDR_LEN);
  /* The board's end of the link differs from the host's in one bit. */
  netif->hwaddr[5] ^= 0x01;
  return ERR_OK;
}

/* With the core lock held. */
static bool host_resolved(void) {
  struct eth_addr *eth;
  const ip4_addr_t *ip;
  return etharp_find_addr(&usb_netif, &host_ip, &eth, &ip) >= 0;
}

/* SNTP starts once the host answers ARP. Started earlier, its first request
   would wait in the ARP queue with its transmit timestamp already taken, and
   the round-trip compensation would split that wait between the two
   directions. Runs in the tcpip thread. */
static void host_probe(void *arg) {
  (void)arg;
  if (host_resolved()) {
    sntp_init();
    return;
  }
  if (tud_ready()) {
    etharp_request(&usb_netif, &host_ip);
  }
  sys_timeout(HOST_PROBE_MS, host_probe, NULL);
}

/* Runs in the tcpip thread, with the core lock held. */
static void tcpip_ready(void *arg) {
  ip4_addr_t ip;
  ip4_addr_t mask;
  ip4_addr_t gateway;
  ip4_addr_t host;
  ip4addr_aton(USB_NET_DEVICE_IP, &ip);
  ip4addr_aton(USB_NET_NETMASK, &mask);
  ip4_addr_set_zero(&gateway);
  ip4addr_aton(USB_NET_HOST_IP, &host);

  netif_add(&usb_netif, &ip, &mask, &gateway, NULL, netif_setup, tcpip_input);
  netif_set_default(&usb_netif);
  netif_set_up(&usb_netif);

  dhcp_entries[0].addr = host;
  dhcp_entries[0].lease = DHCP_LEASE_S;
  if (dhserv_init(&dhcp_config) != ERR_OK) {
    panic("usb_net: no DHCP server");
  }

  host_ip = host;
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  ip_addr_t server;
  ip_addr_copy_from_ip4(server, host);
  sntp_setserver(0, &server);
  host_probe(NULL);

  xSemaphoreGive((SemaphoreHandle_t)arg);
}

static void usb_task(void *arg) {
  (void)arg;
  xSemaphoreTake(usb_mutex, portMAX_DELAY);
  tud_init(BOARD_TUD_RHPORT);
  xSemaphoreGive(usb_mutex);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(USB_POLL_MS));
    xSemaphoreTake(usb_mutex, portMAX_DELAY);
    tud_task();
    if (rx_retry) {
      rx_retry = false;
      tud_network_recv_renew();
    }
    xSemaphoreGive(usb_mutex);
  }
}

void usb_net_start(void) {
  /* Locally administered, unicast. */
  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id);
  tud_network_mac_address[0] = 0x02;
  memcpy(&tud_network_mac_address[1], &id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 5], 5);

  usb_mutex = xSemaphoreCreateMutex();
  SemaphoreHandle_t ready = xSemaphoreCreateBinary();
  tcpip_init(tcpip_ready, ready);
  xSemaphoreTake(ready, portMAX_DELAY);
  vSemaphoreDelete(ready);

  xTaskCreate(usb_task, "usb", USB_TASK_STACK_WORDS, NULL, USB_TASK_PRIORITY, &usb_task_handle);
}

bool usb_net_host_attached(void) { return tud_mounted(); }

bool usb_net_host_reachable(void) {
  if (!tud_mounted()) {
    return false;
  }
  LOCK_TCPIP_CORE();
  bool resolved = host_resolved();
  UNLOCK_TCPIP_CORE();
  return resolved;
}

void usb_net_get_stats(usb_net_stats_t *out) { *out = stats; }
