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

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util/types.h"

/* Exported types */

typedef enum {
  CATS_INVALID,
  CATS_IDLE,
  CATS_CONFIG,
  CATS_TIMER,
  CATS_DROP,
  CATS_FLIGHT,
  CATS_HEHE = 0x7FFFFFFF /* TODO <- optimize these enums and remove this guy */
} cats_boot_state;

typedef struct {
  /* State according to /concepts/v1/cats_fsm.jpg */
  cats_boot_state boot_state;

  control_settings_t control_settings;
  /* A bit mask that specifies which readings to log to the flash */
  uint32_t recorder_mask;

  // Timers
  config_timer_t timers[8];
  // Event action map
  config_event_actions_t event_actions[9];
} cats_config_t;

typedef union {
  cats_config_t config;
  uint32_t config_array[sizeof(cats_config_t) / sizeof(uint32_t)];
} cats_config_u;

extern cats_config_u global_cats_config;

extern const uint32_t CATS_STATUS_SECTOR;

/** cats config initialization **/
void cc_init();
void cc_defaults();

/** persistence functions **/
void cc_load();
void cc_save();

/** debug functions **/
void cc_print();