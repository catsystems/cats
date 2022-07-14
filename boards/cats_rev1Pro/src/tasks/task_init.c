/*
 * CATS Flight Software
 * Copyright (C) 2021 Control and Telemetry Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "tasks/task_init.h"

#include <stdbool.h>
#include <stdlib.h>

#include "cmsis_os.h"
#include "comm/fifo.h"
#include "comm/stream.h"
#include "config/cats_config.h"
#include "config/globals.h"
#include "drivers/adc.h"
#include "drivers/spi.h"
#include "drivers/w25q.h"
#include "flash/lfs_custom.h"
#include "lfs.h"
#include "tasks/task_flight_fsm.h"
#include "tasks/task_health_monitor.h"
#include "tasks/task_peripherals.h"
#include "tasks/task_preprocessing.h"
#include "tasks/task_recorder.h"
#include "tasks/task_sensor_read.h"
#include "tasks/task_simulator.h"
#include "tasks/task_state_est.h"
#include "tasks/task_usb_communicator.h"
#include "tasks/task_simulator.h"
#include "util/actions.h"
#include "util/battery.h"
#include "util/buzzer_handler.h"

#include "util/log.h"

/** Task Definitions **/

#define SET_TASK_PARAMS(task, stack_sz)           \
  uint32_t task##_buffer[stack_sz];               \
  StaticTask_t task##_control_block;              \
  const osThreadAttr_t task##_attributes = {      \
      .name = #task,                              \
      .stack_mem = &task##_buffer[0],             \
      .stack_size = sizeof(task##_buffer),        \
      .cb_mem = &task##_control_block,            \
      .cb_size = sizeof(task##_control_block),    \
      .priority = (osPriority_t)osPriorityNormal, \
  };

/* Todo: Check with Trace if can be reduced */
SET_TASK_PARAMS(task_sensor_read, 512)
/* Todo: Check with Trace if can be reduced */
SET_TASK_PARAMS(task_preprocessing, 512)

/* Todo: Check with Trace if can be reduced */
SET_TASK_PARAMS(task_state_est, 512)
SET_TASK_PARAMS(task_health_monitor, 256)

/* Todo: Check with Trace if can be reduced */
SET_TASK_PARAMS(task_flight_fsm, 512)
SET_TASK_PARAMS(task_peripherals, 256)
/* Todo: Check with Trace if can be reduced */
SET_TASK_PARAMS(task_recorder, 1024)
SET_TASK_PARAMS(task_usb_communicator, 512)
#ifdef CATS_DEBUG
SET_TASK_PARAMS(task_simulator, 512)
#endif

/** Private Constants **/

/** Private Function Declarations **/

static void init_system();

static void init_devices();

static void init_lfs();

static void init_communication();

static void init_simulation();

static void init_tasks();

static void init_imu();

static void init_baro();

static void init_magneto();

static void init_buzzer();

static void init_timers();

static void create_event_map();

/** Exported Function Definitions **/

_Noreturn void task_init(__attribute__((unused)) void *argument) {
  osDelay(200);

  init_system();
  log_info("System initialization complete.");

  osDelay(100);
  init_devices();
  log_info("Device initialization complete.");

  osDelay(100);

  init_lfs();

  cc_init();

  cc_load();

  osDelay(100);

  adc_init();
  osDelay(100);
  battery_monitor_init();

  create_event_map();
  init_timers();

  init_tasks();
  log_info("Task initialization complete.");

  servo_set_position(&SERVO1, global_cats_config.config.initial_servo_position[0]);
  servo_set_position(&SERVO2, global_cats_config.config.initial_servo_position[1]);

  servo_start(&SERVO1);
  servo_start(&SERVO2);

  buzzer_queue_status(CATS_BUZZ_BOOTUP);
  log_disable();

  /* Infinite loop */
  while (1) {
    if (global_usb_detection == true && usb_communication_complete == false) {
      init_communication();
    }
    /* Start Simulation Task */
    if (simulation_started == true && simulation_start_complete == false) {
      init_simulation();
    }
    osDelay(100);
  }
}

/** Private Function Definitions **/

static void init_system() {
  log_set_level(LOG_TRACE);
  log_enable();
}

static void init_devices() {
  /* IMU */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_SET);
  osDelay(10);
  init_imu();
  osDelay(10);
  /* BARO */
  init_baro();
  osDelay(10);
  /* MAGNETO */
  init_magneto();
  osDelay(10);
  /* BUZZER */
  init_buzzer();
  /* FLASH */
  w25q_init();
}

static void init_lfs() {
  /* mount the filesystem */
  int err = lfs_mount(&lfs, &lfs_cfg);
  if (err != 0) {
    /* reformat if we can't mount the filesystem, this should only happen on the first boot */
    log_raw("LFS mounting failed with error %d!", err);
    log_raw("Trying LFS format");
    lfs_format(&lfs, &lfs_cfg);
    int err2 = lfs_mount(&lfs, &lfs_cfg);
    if (err2 != 0) {
      log_raw("LFS mounting failed again with error %d!", err2);
    }
  }

  err = lfs_file_open(&lfs, &fc_file, "flight_counter", LFS_O_RDWR | LFS_O_CREAT);
  if (err != 0) {
    log_raw("LFS initialization failed: could not open 'flight_counter' file, error %d", err);
    return;
  }

  /* read how many flights we have */
  if (lfs_file_read(&lfs, &fc_file, &flight_counter, sizeof(flight_counter)) > 0) {
    log_debug("Flights found: %lu", flight_counter);
  } else {
    log_debug("Flights found: %lu", flight_counter);
    lfs_file_rewind(&lfs, &fc_file);
    lfs_file_write(&lfs, &fc_file, &flight_counter, sizeof(flight_counter));
  }
  lfs_file_close(&lfs, &fc_file);

  /* TODO: create a single function for this, it's used in multiple places */
  /* create the flights directory */
  lfs_mkdir(&lfs, "flights");
  lfs_mkdir(&lfs, "stats");

  strncpy(cwd, "/", sizeof(cwd));

  log_raw("LFS mounted successfully!");
}

static void init_communication() {
  osThreadNew(task_usb_communicator, NULL, &task_usb_communicator_attributes);
  usb_communication_complete = true;
}

static void init_simulation() {
  osThreadNew(task_simulator, NULL, &task_simulator_attributes);
  simulation_start_complete = true;
}

static void init_tasks() {
  switch (global_cats_config.config.boot_state) {
    case CATS_FLIGHT: {
#if (configUSE_TRACE_FACILITY == 1)
      baro_channel = xTraceRegisterString("Baro Channel");
      flash_channel = xTraceRegisterString("Flash Channel");
#endif

      // TODO: Check rec_queue for validity here
      rec_queue = osMessageQueueNew(REC_QUEUE_SIZE, sizeof(rec_elem_t), NULL);
      rec_cmd_queue = osMessageQueueNew(REC_CMD_QUEUE_SIZE, sizeof(rec_cmd_type_e), NULL);
      event_queue = osMessageQueueNew(EVENT_QUEUE_SIZE, sizeof(cats_event_e), NULL);
#if (configUSE_TRACE_FACILITY == 1)
      vTraceSetQueueName(rec_queue, "Recorder Queue");
#endif

      osThreadNew(task_recorder, NULL, &task_recorder_attributes);

      osThreadNew(task_sensor_read, NULL, &task_sensor_read_attributes);

      osThreadNew(task_preprocessing, NULL, &task_preprocessing_attributes);

      osThreadNew(task_flight_fsm, NULL, &task_flight_fsm_attributes);

      osThreadNew(task_peripherals, NULL, &task_peripherals_attributes);

      osThreadNew(task_state_est, NULL, &task_state_est_attributes);

      osThreadNew(task_health_monitor, NULL, &task_health_monitor_attributes);

    } break;
    case CATS_CONFIG:
      break;
    case CATS_TIMER:
    case CATS_DROP:
      break;
    default:
      log_fatal("Wrong boot state!");
  }
}

static void init_imu() {
  /* TODO: this delay until 1000 prob. isn't needed anymore */
  /* TODO: Add timeout for sensor init */
  osDelayUntil(1000);
  for (int i = 0; i < NUM_IMU; i++) {
    while (!icm20601_init(&IMU_DEV[i])) {
      log_error("IMU %d initialization failed", i);
      osDelay(10);
    }
  }
  for (int i = 0; i < NUM_ACCELEROMETER; i++) {
    while (!h3lis100dl_init(&ACCEL)) {
      osDelay(10);
      log_error("ACCEL initialization failed");
    }
  }
}

static void init_baro() {
  for (int i = 0; i < NUM_BARO; i++) {
    ms5607_init(&BARO_DEV[i]);
    osDelay(10);
  }
}

static void init_magneto() {
#if NUM_MAGNETO > 0
  spi_init(MAG.spi);
  mmc5983ma_init(&MAG);
  // mmc5983_calibration(&MAG);
#endif
}

static void init_buzzer() {
  buzzer_set_freq(&BUZZER, 3200);
  buzzer_set_volume(&BUZZER, 60);
}

static void create_event_map() {
  /* number of event types + 0th element */
  /* TODO: where to free this? */
  event_action_map = calloc(NUM_EVENTS, sizeof(event_action_map_elem_t));

  uint16_t nr_actions;
  config_action_t action;
  // Loop over all events
  for (int ev_idx = 0; ev_idx < NUM_EVENTS; ev_idx++) {
    nr_actions = cc_get_num_actions(ev_idx);
    // If an action is mapped to the event
    if (nr_actions > 0) {
      event_action_map[ev_idx].num_actions = nr_actions;
      event_action_map[ev_idx].action_list = calloc(nr_actions, sizeof(peripheral_act_t));
      // Loop over all actions
      for (uint16_t act_idx = 0; act_idx < nr_actions; act_idx++) {
        if (cc_get_action(ev_idx, act_idx, &action) == true) {
          event_action_map[ev_idx].action_list[act_idx].func_ptr = action_table[action.action_idx];
          event_action_map[ev_idx].action_list[act_idx].func_arg = action.arg;
        } else {
          // If we cannot find the action in the config, set the num of actions to the last successfully found action
          event_action_map[ev_idx].num_actions = act_idx;
          break;
        }
      }
    }
  }
}

static void init_timers() {
  /* Init timers */
  for (uint32_t i = 0; i < NUM_TIMERS; i++) {
    if (global_cats_config.config.timers[i].duration > 0) {
      ev_timers[i].timer_init_event = (cats_event_e)global_cats_config.config.timers[i].start_event;
      ev_timers[i].execute_event = (cats_event_e)global_cats_config.config.timers[i].trigger_event;
      ev_timers[i].timer_duration_ticks = global_cats_config.config.timers[i].duration;
    }
  }

  /* Create Timers */
  for (uint32_t i = 0; i < NUM_TIMERS; i++) {
    if (global_cats_config.config.timers[i].duration > 0) {
      ev_timers[i].timer_id = osTimerNew((void *)trigger_event, osTimerOnce, (void *)ev_timers[i].execute_event, NULL);
    }
  }

  /* Init mach Timer */
  mach_timer.timer_init_event = EV_LIFTOFF;
  mach_timer.execute_event = EV_MACHTIMER;
  mach_timer.timer_duration_ticks = global_cats_config.config.control_settings.mach_timer_duration;

  /* Create mach timer */
  mach_timer.timer_id = osTimerNew((void *)trigger_event, osTimerOnce, (void *)mach_timer.execute_event, NULL);
}
