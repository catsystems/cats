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

#include "tasks/task_state_est.h"
#include "config/globals.h"
#include "control/kalman_filter.h"
#include "control/orientation_filter.h"

#include <cmath>

/** Private Constants **/

/** Private Function Declarations **/

/** Exported Function Definitions **/

/**
 * @brief Function implementing the task_state_est thread.
 * @param argument: Not used
 * @retval None
 */
[[noreturn]] void task_state_est(__attribute__((unused)) void *argument) {
  /* End Initialization */
  osDelay(1000);

  /* Initialize State Estimation */
  kalman_filter_t filter = {.t_sampl = 1.0f / (float)(CONTROL_SAMPLING_FREQ)};

  init_filter_struct(&filter);
  initialize_matrices(&filter);

  /* local fsm enum */
  flight_fsm_e new_fsm_enum = MOVING;
  flight_fsm_e old_fsm_enum = MOVING;

  /* initialize Orientation State Estimation */
  orientation_filter_t orientation_filter = {};
  init_orientation_filter(&orientation_filter);
  reset_orientation_filter(&orientation_filter);

  uint32_t tick_count = osKernelGetTickCount();
  uint32_t tick_update = osKernelGetTickFreq() / CONTROL_SAMPLING_FREQ;
  while (1) {
    /* update fsm enum */
    new_fsm_enum = global_flight_state.flight_state;

    /* Reset IMU when we go from moving to READY */
    if ((new_fsm_enum != old_fsm_enum) && (new_fsm_enum == READY)) {
      reset_kalman(&filter);
      reset_orientation_filter(&orientation_filter);
    }

    /* Soft reset kalman filter when we go from Ready to thrusting */
    /* Reset Orientation Estimate when going to thrusting */
    if ((new_fsm_enum == THRUSTING) && (new_fsm_enum != old_fsm_enum)) {
      soft_reset_kalman(&filter);
      reset_orientation_filter(&orientation_filter);
    }

    /* Write measurement data into the filter struct */
    /* After apogee we assume that the linear acceleration is zero. This assumption is true if the parachute has been
     * ejected. If this assumption is not done, the linear acceleration will be bad because of movement of the rocket
     * due to parachute forces. */
    if (new_fsm_enum < DROGUE) {
      filter.measured_acceleration = global_estimation_input.acceleration_z;
    } else {
      filter.measured_acceleration = 0;
    }

    filter.measured_AGL = global_estimation_input.height_AGL;

    /* Do a Kalman Step */
    kalman_step(&filter, global_flight_state.flight_state);

    /* write the Data into the global variable */
    global_estimation_data.height = filter.x_bar_data[0];
    global_estimation_data.velocity = filter.x_bar_data[1];
    global_estimation_data.acceleration = filter.measured_acceleration + filter.x_bar_data[2];

    /* Do Orientation Filter */
    quaternion_kinematics(&orientation_filter, &global_SI_data.gyro);
    orientation_info_t orientation_info;
    /*
    log_raw("[%lu] KF: q0: %ld; q1: %ld; q2: %ld; q3: %ld", tick_count, (int32_t)(orientation_filter.estimate_data[0] *
    1000), (int32_t)(orientation_filter.estimate_data[1] * 1000), (int32_t)(orientation_filter.estimate_data[2] * 1000),
              (int32_t)(orientation_filter.estimate_data[3] * 1000));
    */
    for (uint8_t i = 0; i < 4; i++) {
      orientation_info.estimated_orientation[i] = (int16_t)(orientation_filter.estimate_data[i] * 10000.0f);
    }

    record(tick_count, ORIENTATION_INFO, &orientation_info);

    /* record filtered data */
    filtered_data_info_t filtered_data_info = {
        .filtered_altitude_AGL = filter.measured_AGL,
        .filtered_acceleration = filter.measured_acceleration,
    };

    record(tick_count, FILTERED_DATA_INFO, &filtered_data_info);

    /* Log KF outputs */
    flight_info_t flight_info = {.height = filter.x_bar_data[0],
                                 .velocity = filter.x_bar_data[1],
                                 .acceleration = filter.measured_acceleration + filter.x_bar_data[2]};
    if (global_flight_state.flight_state >= DROGUE) {
      flight_info.acceleration = filter.x_bar_data[2];
    }
    record(tick_count, FLIGHT_INFO, &flight_info);

    // log_info("H: %ld; V: %ld; A: %ld; O: %ld", (int32_t)((float)filter.x_bar.pData[0] * 1000),
    //          (int32_t)((float)filter.x_bar.pData[1] * 1000), (int32_t)(filtered_data_info.filtered_acceleration *
    //          1000), (int32_t)((float)filter.x_bar.pData[2] * 1000));
    log_sim("[%lu]: height: %f, velocity: %f, offset: %f", tick_count, filter.x_bar.pData[0], filter.x_bar.pData[1],
            filter.x_bar_data[2]);

    /* reset old fsm enum */
    old_fsm_enum = new_fsm_enum;

    tick_count += tick_update;
    osDelayUntil(tick_count);
  }
}

/** Private Function Definitions **/
