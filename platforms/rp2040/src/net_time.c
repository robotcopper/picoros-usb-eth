#include "picoros_usb_eth/net_time.h"

#include <sys/time.h>

#include "hardware/sync.h"
#include "hardware/timer.h"

/* lwIP only compensates the round trip when the clock is within 34 years of
   the server's, so the clock starts at 2026-01-01T00:00:00Z rather than at 0:
   the first exchange is then compensated like the others. */
#define INITIAL_OFFSET_US (1767225600ll * 1000000ll)

/* Written by the tcpip thread, read by any task: a 64-bit access is two on
   the Cortex-M0+, hence the interrupt masking. */
static int64_t offset_us = INITIAL_OFFSET_US;
static net_time_stats_t stats;

static int64_t read_offset(void) {
  uint32_t irq = save_and_disable_interrupts();
  int64_t offset = offset_us;
  restore_interrupts(irq);
  return offset;
}

bool net_time_synced(void) { return stats.synced; }

int64_t net_time_ns_at(uint64_t local_us) { return ((int64_t)local_us + read_offset()) * 1000ll; }

int64_t net_time_now_ns(void) { return net_time_ns_at(time_us_64()); }

void net_time_get_stats(net_time_stats_t *out) {
  uint32_t irq = save_and_disable_interrupts();
  *out = stats;
  restore_interrupts(irq);
}

void net_time_sntp_get(uint32_t *sec, uint32_t *us) {
  int64_t now = (int64_t)time_us_64() + read_offset();
  *sec = (uint32_t)(now / 1000000ll);
  *us = (uint32_t)(now % 1000000ll);
}

void net_time_sntp_set(uint32_t sec, uint32_t us) {
  uint64_t local = time_us_64();
  int64_t offset = (int64_t)sec * 1000000ll + (int64_t)us - (int64_t)local;
  const struct timeval tv = {.tv_sec = (time_t)sec, .tv_usec = (suseconds_t)us};
  uint32_t irq = save_and_disable_interrupts();
  stats.last_step_us = stats.synced ? offset - offset_us : 0;
  offset_us = offset;
  stats.synced = true;
  ++stats.updates;
  stats.last_update_us = local;
  /* Also the libc clock, which zenoh-pico timestamps samples with. Its 64-bit
     offset is unguarded, hence inside the masked section. */
  settimeofday(&tv, NULL);
  restore_interrupts(irq);
}
