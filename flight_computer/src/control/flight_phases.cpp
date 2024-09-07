/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#include "control/flight_phases.hpp"
#include "config/cats_config.hpp"
#include "tasks/task_peripherals.hpp"

static void check_calibrating_phase(flight_fsm_t *fsm_state, vf32_t acc_data, vf32_t gyro_data);
static void check_ready_phase(flight_fsm_t *fsm_state, vf32_t acc_data, const control_settings_t *settings);
static void check_thrusting_phase(flight_fsm_t *fsm_state, estimation_output_t state_data);
static void check_coasting_phase(flight_fsm_t *fsm_state, estimation_output_t state_data);
static void check_drogue_phase(flight_fsm_t *fsm_state, estimation_output_t state_data);
static void check_main_phase(flight_fsm_t *fsm_state, vf32_t acc_data, vf32_t gyro_data);

static void clear_fsm_memory(flight_fsm_t *fsm_state);
static void change_state_to(flight_fsm_e new_state, cats_event_e event_to_trigger, flight_fsm_t *fsm_state);

void check_flight_phase(flight_fsm_t *fsm_state, vf32_t acc_data, vf32_t gyro_data, estimation_output_t state_data,
                        const control_settings_t *settings) {
  /* Save old FSM State */
  const flight_fsm_t old_fsm_state = *fsm_state;

  /* Check FSM State */
  switch (fsm_state->flight_state) {
    case CALIBRATING:
      check_calibrating_phase(fsm_state, acc_data, gyro_data);
      break;
    case READY:
      check_ready_phase(fsm_state, acc_data, settings);
      break;
    case THRUSTING:
      check_thrusting_phase(fsm_state, state_data);
      break;
    case COASTING:
      check_coasting_phase(fsm_state, state_data);
      break;
    case DROGUE:
      check_drogue_phase(fsm_state, state_data);
      break;
    case MAIN:
      check_main_phase(fsm_state, acc_data, gyro_data);
      break;
    case TOUCHDOWN:
    default:
      break;
  }

  fsm_state->state_changed = old_fsm_state.flight_state != fsm_state->flight_state;
}

static void check_calibrating_phase(flight_fsm_t *fsm_state, vf32_t acc_data, vf32_t gyro_data) {
  /* Check if the IMU moved between two timesteps */
  /* Add an error bound as the IMU is noisy which is accepted */
  if ((fabsf(fsm_state->old_acc_data.x - acc_data.x) < ALLOWED_ACC_ERROR_CALIB) &&
      (fabsf(fsm_state->old_acc_data.y - acc_data.y) < ALLOWED_ACC_ERROR_CALIB) &&
      (fabsf(fsm_state->old_acc_data.z - acc_data.z) < ALLOWED_ACC_ERROR_CALIB) &&
      (fabsf(fsm_state->old_gyro_data.x - gyro_data.x) < ALLOWED_GYRO_ERROR_CALIB) &&
      (fabsf(fsm_state->old_gyro_data.y - gyro_data.y) < ALLOWED_GYRO_ERROR_CALIB) &&
      (fabsf(fsm_state->old_gyro_data.z - gyro_data.z) < ALLOWED_GYRO_ERROR_CALIB)) {
    fsm_state->memory[0]++;
  } else {
    fsm_state->memory[0] = 0;
  }

  /* Update old IMU value */
  fsm_state->old_acc_data = acc_data;
  fsm_state->old_gyro_data = gyro_data;

  /* Check if we reached the threshold */
  if (fsm_state->memory[0] > TIME_THRESHOLD_CALIB_TO_READY) {
    change_state_to(READY, EV_READY, fsm_state);
  }
}

static void check_ready_phase(flight_fsm_t *fsm_state, vf32_t acc_data, const control_settings_t *settings) {
  /* Check if we move from READY To THRUSTING */
  /* The absolute value of the acceleration is used here to make sure that we detect liftoff */
  const float32_t accel_x = acc_data.x * acc_data.x;
  const float32_t accel_y = acc_data.y * acc_data.y;
  const float32_t accel_z = acc_data.z * acc_data.z;
  const float32_t acceleration = accel_x + accel_y + accel_z;

  if (acceleration > (static_cast<float32_t>(settings->liftoff_acc_threshold) *
                      static_cast<float32_t>(settings->liftoff_acc_threshold))) {
    fsm_state->memory[1]++;
  } else {
    fsm_state->memory[1] = 0;
  }

  if (fsm_state->memory[1] > LIFTOFF_SAFETY_COUNTER) {
    change_state_to(THRUSTING, EV_LIFTOFF, fsm_state);
  }
}

static void check_thrusting_phase(flight_fsm_t *fsm_state, estimation_output_t state_data) {
  /* When acceleration is below 0, liftoff concludes */
  if (state_data.acceleration < 0) {
    fsm_state->memory[0]++;
  } else {
    fsm_state->memory[0] = 0;
  }

  if (fsm_state->memory[0] > COASTING_SAFETY_COUNTER) {
    change_state_to(COASTING, EV_MAX_V, fsm_state);
  }
}

static void check_coasting_phase(flight_fsm_t *fsm_state, estimation_output_t state_data) {
  /* When velocity is below 0, coasting concludes */
  if (state_data.velocity < 0) {
    fsm_state->memory[0]++;
  }

  if (fsm_state->memory[0] > APOGEE_SAFETY_COUNTER) {
    /* If the duration between thrusting and apogee is smaller than defined, go to touchdown */
    if ((osKernelGetTickCount() - fsm_state->thrust_trigger_time) < MIN_TICK_COUNTS_BETWEEN_THRUSTING_APOGEE) {
      change_state_to(TOUCHDOWN, EV_TOUCHDOWN, fsm_state);
    } else {
      change_state_to(DROGUE, EV_APOGEE, fsm_state);
    }
  }
}

static void check_drogue_phase(flight_fsm_t *fsm_state, estimation_output_t state_data) {
  /* If the height is smaller than the configured Main height, main deployment needs to be actuated */
  if (state_data.height < static_cast<float32_t>(global_cats_config.control_settings.main_altitude)) {
    /* Achieved Height to deploy Main */
    fsm_state->memory[0]++;
  } else {
    /* Did Not Achieve */
    fsm_state->memory[0] = 0;
  }

  if (fsm_state->memory[0] > MAIN_SAFETY_COUNTER) {
    change_state_to(MAIN, EV_MAIN_DEPLOYMENT, fsm_state);
  }
}

static void check_main_phase(flight_fsm_t *fsm_state, vf32_t acc_data, vf32_t gyro_data) {
  /* Check if the IMU moved between two timesteps */
  /* Add an error bound as the IMU is noisy which is accepted */
  if ((fabsf(fsm_state->old_acc_data.x - acc_data.x) < ALLOWED_ACC_ERROR_TD) &&
      (fabsf(fsm_state->old_acc_data.y - acc_data.y) < ALLOWED_ACC_ERROR_TD) &&
      (fabsf(fsm_state->old_acc_data.z - acc_data.z) < ALLOWED_ACC_ERROR_TD) &&
      (fabsf(fsm_state->old_gyro_data.x - gyro_data.x) < ALLOWED_GYRO_ERROR_TD) &&
      (fabsf(fsm_state->old_gyro_data.y - gyro_data.y) < ALLOWED_GYRO_ERROR_TD) &&
      (fabsf(fsm_state->old_gyro_data.z - gyro_data.z) < ALLOWED_GYRO_ERROR_TD)) {
    fsm_state->memory[0]++;
  } else {
    /* Touchdown not achieved */
    fsm_state->memory[0] = 0;
  }

  /* Update old IMU value */
  fsm_state->old_acc_data = acc_data;
  fsm_state->old_gyro_data = gyro_data;

  /* Check if we reached the threshold */
  if (fsm_state->memory[0] > TOUCHDOWN_SAFETY_COUNTER) {
    change_state_to(TOUCHDOWN, EV_TOUCHDOWN, fsm_state);
  }
}

/* Function that needs to be called every time that a state transition is done */
static void clear_fsm_memory(flight_fsm_t *fsm_state) {
  fsm_state->clock_memory = 0;
  fsm_state->memory[0] = 0;
  fsm_state->memory[1] = 0;
  fsm_state->memory[2] = 0;
}

static void change_state_to(flight_fsm_e new_state, cats_event_e event_to_trigger, flight_fsm_t *fsm_state) {
  if (fsm_state->flight_state == THRUSTING) {
    fsm_state->thrust_trigger_time = osKernelGetTickCount();
  }

  trigger_event(event_to_trigger);
  osEventFlagsClear(fsm_flag_id, 0xFF);
  osEventFlagsSet(fsm_flag_id, new_state);
  fsm_state->flight_state = new_state;
  clear_fsm_memory(fsm_state);
}
