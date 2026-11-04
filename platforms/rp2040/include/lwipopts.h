#ifndef PICOROS_USB_ETH_LWIPOPTS_H_
#define PICOROS_USB_ETH_LWIPOPTS_H_

/*
 * lwIP for a single Ethernet-over-USB (CDC-NCM) link to the host, under
 * FreeRTOS. The board has a fixed address, leases the other end of the link to
 * the host with a one-entry DHCP server, and takes its wall clock from the host
 * over SNTP. zenoh-pico talks TCP through the socket API.
 */

#include <stdint.h>

/* Operating system and APIs */
#define NO_SYS 0
#define LWIP_SOCKET 1
#define LWIP_NETCONN 0
#define LWIP_SOCKET_POLL 0
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_ERRNO_STDINCLUDE 1
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 1
#define LWIP_SO_LINGER 1

/* Protocols */
#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_ETHERNET 1
#define LWIP_ARP 1
#define LWIP_ICMP 1
#define LWIP_RAW 0
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_DHCP 0
#define LWIP_AUTOIP 0
#define LWIP_IGMP 0
/* A DHCP client asks from 0.0.0.0, which ip4_input() drops as a broadcast
   source unless the destination port accepts link-layer addressed packets:
   the DHCP server's. The port is in network order. */
#define LWIP_IP_ACCEPT_UDP_PORT(port) ((port) == PP_NTOHS(LWIP_IANA_PORT_DHCP_SERVER))
/* getaddrinfo(), through which zenoh-pico parses even a numeric locator. */
#define LWIP_DNS 1

/* Memory. Received frames are copied into the heap at their exact size rather
   than into 1.5 kB pool buffers, so a burst of small commands cannot exhaust
   the pool. */
#define MEM_LIBC_MALLOC 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE (24 * 1024)
#define PBUF_POOL_SIZE 4
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 8
/* SNTP and the host probe in usb_net.c schedule their own timeouts, which the
   internal count leaves out. */
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 2)

#define TCP_MSS 1460
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define TCP_LISTEN_BACKLOG 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define LWIP_CHKSUM_ALGORITHM 3
#define LWIP_STATS 0

/* Threads. zenoh-pico's tasks run at configMAX_PRIORITIES / 2, that is 16. */
#define TCPIP_THREAD_NAME "tcpip"
#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_THREAD_PRIO 20
#define TCPIP_MBOX_SIZE 16
#define DEFAULT_TCP_RECVMBOX_SIZE 16
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

/* Locking. The SDK's default core lock goes through the CYW43 async_context;
   this board has none, so lwIP's own mutex is used. Received frames are posted
   to the tcpip thread instead of being processed under the core lock by the
   USB task: that task holds the TinyUSB mutex, which transmission takes with
   the core lock held, and the two orders would deadlock. */
#define PICO_LWIP_CUSTOM_LOCK_TCPIP_CORE 0
#define LWIP_TCPIP_CORE_LOCKING 1
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0
#define LWIP_FREERTOS_CHECK_CORE_LOCKING 1
void sys_lock_tcpip_core(void);
void sys_unlock_tcpip_core(void);
void sys_check_core_locking(void);
void sys_mark_tcpip_thread(void);
#define LOCK_TCPIP_CORE() sys_lock_tcpip_core()
#define UNLOCK_TCPIP_CORE() sys_unlock_tcpip_core()
#define LWIP_ASSERT_CORE_LOCKED() sys_check_core_locking()
#define LWIP_MARK_TCPIP_THREAD() sys_mark_tcpip_thread()

/* SNTP: the host is the only server. The originate timestamp check rejects
   a late reply to an earlier request, and the round trip is compensated as in
   RFC 4330. 15 s is the minimum polling interval the RFC allows. */
#define SNTP_SERVER_DNS 0
#define SNTP_CHECK_RESPONSE 2
#define SNTP_COMP_ROUNDTRIP 1
#define SNTP_STARTUP_DELAY 0
#define SNTP_UPDATE_DELAY 15000
#define SNTP_RECV_TIMEOUT 2000
#define SNTP_RETRY_TIMEOUT SNTP_RECV_TIMEOUT
#define SNTP_RETRY_TIMEOUT_EXP 0
void net_time_sntp_get(uint32_t *sec, uint32_t *us);
void net_time_sntp_set(uint32_t sec, uint32_t us);
#define SNTP_GET_SYSTEM_TIME(sec, us) net_time_sntp_get(&(sec), &(us))
#define SNTP_SET_SYSTEM_TIME_US(sec, us) net_time_sntp_set((sec), (us))

#endif /* PICOROS_USB_ETH_LWIPOPTS_H_ */
