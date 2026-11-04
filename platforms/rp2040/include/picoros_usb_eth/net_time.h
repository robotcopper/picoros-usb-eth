#ifndef PICOROS_USB_ETH_NET_TIME_H_
#define PICOROS_USB_ETH_NET_TIME_H_

/*
 * Wall clock from the host over SNTP, kept as an offset over the 1 MHz
 * hardware timer that time_us_64() reads. lwIP's SNTP client sets it through
 * the hooks in lwipopts.h, and each update is also passed to settimeofday(),
 * so gettimeofday(), zenoh-pico's sample timestamps and Pico-ROS's rmw
 * attachments follow it. Between two
 * updates the clock runs at the board's crystal rate, and each update steps it
 * by the drift accumulated since the previous one.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool synced;
  uint32_t updates;
  /* Change of the offset at the last update; 0 after the first one. */
  int64_t last_step_us;
  /* time_us_64() at the last update. */
  uint64_t last_update_us;
} net_time_stats_t;

/* At least one SNTP reply has been accepted. */
bool net_time_synced(void);

/* UNIX time in nanoseconds at a time_us_64() reading. Meaningless until
   net_time_synced(). */
int64_t net_time_ns_at(uint64_t local_us);

int64_t net_time_now_ns(void);

void net_time_get_stats(net_time_stats_t *stats);

#endif /* PICOROS_USB_ETH_NET_TIME_H_ */
