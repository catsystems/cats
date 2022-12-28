/*
 * CATS Flight Software
 * Copyright (C) 2022 Control and Telemetry Systems
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

#include "tasks/task_flight_fsm.h"
#include "cmsis_os.h"
#include "config/cats_config.h"
#include "config/globals.h"
#include "control/flight_phases.h"
#include "tasks/task_peripherals.h"
#include "util/enum_str_maps.h"
#include "util/log.h"

/**
 * @brief Function implementing the task_flight_fsm thread.
 * @param argument: Not used
 * @retval None
 */
[[noreturn]] void task_flight_fsm(__attribute__((unused)) void *argument) {
  const control_settings_t settings = global_cats_config.config.control_settings;

  fsm_flag_id = osEventFlagsNew(NULL);
  osEventFlagsSet(fsm_flag_id, MOVING);

  trigger_event(EV_MOVING);

  uint32_t tick_count = osKernelGetTickCount();
  uint32_t tick_update = osKernelGetTickFreq() / CONTROL_SAMPLING_FREQ;
  while (1) {
    /* Check Flight Phases */
    /* Todo: Check for global arming trigger */
    check_flight_phase(&global_flight_state, &global_SI_data.acc, &global_SI_data.gyro, &global_estimation_data,
                       global_estimation_input.height_AGL, global_arming_bool, &settings);

    if (global_flight_state.state_changed) {
      log_error("State Changed FlightFSM to %s", fsm_map[global_flight_state.flight_state]);
      log_sim("State Changed FlightFSM to %s", fsm_map[global_flight_state.flight_state]);
      record(tick_count, FLIGHT_STATE, &global_flight_state.flight_state);
    }

    tick_count += tick_update;
    osDelayUntil(tick_count);
  }
}

/** Private Function Definitions **/
