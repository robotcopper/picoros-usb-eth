/* The hooks FreeRTOSConfig.h enables: stop with the cause on the UART rather
   than run on with corrupted memory, and the run-time statistics clock. */

#include "FreeRTOS.h"
#include "hardware/timer.h"
#include "pico.h"
#include "task.h"

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
  (void)task;
  panic("stack overflow in task %s", name);
}

void vApplicationMallocFailedHook(void) { panic("FreeRTOS heap exhausted"); }

uint64_t picoros_usb_eth_run_time_us(void) { return time_us_64(); }
