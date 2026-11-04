/*
 * FreeRTOS entry point. It runs the application's app_main() in a task at the
 * priority of the zenoh-pico examples, below zenoh-pico's tasks (16), the
 * tcpip thread (20) and the USB task (22).
 */

#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "task.h"

#define APP_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define APP_TASK_STACK_WORDS (configMINIMAL_STACK_SIZE * 16)

int app_main(void);

static void app_task(void *params) {
  (void)params;
  app_main();
  vTaskDelete(NULL);
}

int main(void) {
  stdio_init_all();
  xTaskCreate(app_task, "app", APP_TASK_STACK_WORDS, NULL, APP_TASK_PRIORITY, NULL);
  vTaskStartScheduler();
  return 0;
}
