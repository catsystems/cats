/*
 * task_flight_fsm.c
 *
 *  Created on: Dec 21, 2020
 *      Author: Jonas
 */

#include "cmsis_os.h"
#include "config/globals.h"
#include "util/log.h"
#include "tasks/task_flight_fsm.h"
#include "control/flight_phases.h"
#include "config/cats_config.h"

/** Private Constants **/

/** Private Function Declarations **/

/** Exported Function Definitions **/

/**
 * @brief Function implementing the task_state_est thread.
 * @param argument: Not used
 * @retval None
 */
_Noreturn void task_flight_fsm(__attribute__((unused)) void *argument) {
  /* For periodic update */
  uint32_t tick_count, tick_update;

  flight_fsm_t fsm_state = {.flight_state = MOVING};
  imu_data_t local_imu = {0};
  estimation_output_t local_kf_data;

  control_settings_t settings = global_cats_config.config.control_settings;

  tick_count = osKernelGetTickCount();
  tick_update = osKernelGetTickFreq() / CONTROL_SAMPLING_FREQ;

  float max_v = 0;
  float max_a = 0;
  float max_h = 0;

  // osDelay(1000);

  while (1) {
    /* Update KF data */
    local_kf_data = global_kf_data;

    /* Update Imu data depending on the sensor elimination data */
    for (int i = 0; i < 3; i++) {
      if (global_elimination_data.faulty_imu[i] == 0) {
        local_imu = global_imu[i + 1];
        break;
      }
    }

    /* Check Flight Phases */
    check_flight_phase(&fsm_state, &local_imu, &local_kf_data, &settings);

    /* Update Global Flight phase */
    global_flight_state = fsm_state;

    // Keep track of max speed, velocity and acceleration for flight stats
    if (fsm_state.flight_state >= THRUSTING_1 && fsm_state.flight_state <= APOGEE) {
      if (max_v < local_kf_data.velocity) max_v = local_kf_data.velocity;
      if (max_a < local_kf_data.acceleration) max_a = local_kf_data.acceleration;
      if (max_h < local_kf_data.height) max_h = local_kf_data.height;
    }

    if (fsm_state.state_changed == 1) {
      log_error("State Changed to %s", flight_fsm_map[fsm_state.flight_state]);
      flight_state_t flight_state = {.ts = osKernelGetTickCount(),
                                     .flight_or_drop_state.flight_state = fsm_state.flight_state};
      record(FLIGHT_STATE, &flight_state);

      // When we are in any flight state update the flash sector with last
      // flight phase
      if (fsm_state.flight_state == TOUCHDOWN) {
        // TODO - create a stats file
        //        cs_set_flight_phase(fsm_state.flight_state);
        //        cs_set_max_altitude(max_h);
        //        cs_set_max_velocity(max_v);
        //        cs_set_max_acceleration(max_a);
        //        cs_save();
      }
    }

    tick_count += tick_update;
    osDelayUntil(tick_count);
  }
}

/** Private Function Definitions **/
