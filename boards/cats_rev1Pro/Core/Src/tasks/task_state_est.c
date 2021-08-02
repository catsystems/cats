/*
 * task_baro_read.c
 *
 *  Created on: Nov 1, 2019
 *      Author: Jonas
 */

#include "tasks/task_state_est.h"
#include "control/kalman_filter.h"
#include "control/sensor_elimination.h"
#include "control/calibration.h"
#include "util/recorder.h"
#include "config/globals.h"
#include "control/orientation_kf.h"
#include "control/data_processing.h"

#include <math.h>

/** Private Constants **/

static const float P_INITIAL = 101250.f;
static const float GRAVITY = 9.81f;

/** Private Function Declarations **/

inline static float calculate_height(float pressure_initial, float pressure, float temperature);

static void transform_data(state_estimation_data_t *state_data, kalman_filter_t *filter,
                           calibration_data_t *calibration, flight_fsm_t *fsm_state);

static void average_data(imu_data_t *rolling_imu, uint8_t *imu_counter, int32_t *rolling_pressure,
                         uint8_t *pressure_counter, sensor_elimination_t *elimination, imu_data_t *average_imu,
                         float *average_pressure);

#ifdef USE_MEDIAN_FILTER

static void median_filter(median_filter_t *filter_data, state_estimation_data_t *state_data);

#endif

/** Exported Function Definitions **/

/**
 * @brief Function implementing the task_state_est thread.
 * @param argument: Not used
 * @retval None
 */
_Noreturn void task_state_est(__attribute__((unused)) void *argument) {
  /* For periodic update */
  uint32_t tick_count, tick_update;
  /* local flight phase */
  flight_fsm_t fsm_state = {.flight_state = MOVING};
  flight_fsm_e old_fsm_enum = MOVING;
  /* end local flight phase */

  /* calibration data */
  calibration_data_t calibration = {.angle = 1, .axis = 2};
  uint8_t imu_counter = 0;
  imu_data_t rolling_imu[10];
  imu_data_t average_imu = {};
  uint8_t pressure_counter = 0;
  int32_t rolling_pressure[10];
  float average_pressure = P_INITIAL;
  /* end calibration data */

  /* End Initialization */
  osDelay(1000);

  /* Initialize State Estimation */
  state_estimation_data_t state_data = {};
  sensor_elimination_t elimination = {};
  kalman_filter_t filter = {.pressure_0 = P_INITIAL, .t_sampl = 1.0f / (float)(CONTROL_SAMPLING_FREQ)};
  transform_data(&state_data, &filter, &calibration, &fsm_state);
  filter.pressure_0 = (state_data.pressure[0] + state_data.pressure[1] + state_data.pressure[2]) / 3;

  init_filter_struct(&filter);
  initialize_matrices(&filter);
  /* For Logging */
  float raw_accel;
  float raw_altitude_AGL;

  /* initialize Orientation State Estimation */
  //  orientation_filter_t orientation_filter = {.t_sampl = 1.0f / (float)(CONTROL_SAMPLING_FREQ)};
  //  init_orientation_filter_struct(&orientation_filter);
  //  initialize_orientation_matrices(&orientation_filter);

#ifdef USE_MEDIAN_FILTER
  /* Arrays for median Filtering */
  median_filter_t filter_data = {0};
#endif

  /* Infinite loop */
  tick_count = osKernelGetTickCount();
  tick_update = osKernelGetTickFreq() / CONTROL_SAMPLING_FREQ;

  while (1) {
    cats_error_e err = CATS_ERR_OK;

    /* Update Flight Phase */
    fsm_state = global_flight_state;

    if (fsm_state.flight_state == INVALID) {
      log_error("Invalid FSM state!");
    }

    /* Reset IMU when we go from moving to IDLE */
    if ((fsm_state.flight_state == IDLE) && (fsm_state.flight_state != old_fsm_enum)) {
      reset_kalman(&filter, average_pressure);
      calibrate_imu(&average_imu, &calibration);
    }
    /* Remove Accel Data when we enter apogee for the KF */
    if ((fsm_state.flight_state == APOGEE) && (fsm_state.flight_state != old_fsm_enum)) {
      float32_t Q_dash[4] = {0, 0, 0, 10.0f};
      memcpy(filter.Q_data, Q_dash, sizeof(Q_dash));
    }

    /* Get Sensor Readings already transformed in the right coordinate Frame */
    transform_data(&state_data, &filter, &calibration, &fsm_state);
    raw_accel = 0;
    raw_altitude_AGL = 0;
    for (int i = 0; i < 3; i++) {
      if (elimination.faulty_imu[i] == 0) {
        raw_accel += state_data.acceleration[i] / (float)(elimination.num_faulty_imus);
      }
      if (elimination.faulty_baro[i] == 0) {
        raw_altitude_AGL += state_data.calculated_AGL[i] / (float)(elimination.num_faulty_baros);
      }
    }

    /* Filter Data */
#ifdef USE_MEDIAN_FILTER
    median_filter(&filter_data, &state_data);
    float filtered_acc = 0;
    float filtered_AGL = 0;
    for (int i = 0; i < 3; i++) {
      if (elimination.faulty_imu[i] == 0) {
        filtered_acc += state_data.acceleration[i] / (float)(elimination.num_faulty_imus);
      }
      if (elimination.faulty_baro[i] == 0) {
        filtered_AGL += state_data.calculated_AGL[i] / (float)(elimination.num_faulty_baros);
      }
    }
    filtered_data_info_t filtered_data_info = {.ts = osKernelGetTickCount(),
                                               .measured_altitude_AGL = raw_altitude_AGL,
                                               .measured_acceleration = raw_accel,
                                               .filtered_acceleration = filtered_acc,
                                               .filtered_altitude_AGL = filtered_AGL};
    record(FILTERED_DATA_INFO, &filtered_data_info);
#endif

    /* Check Sensor Readings */
    err |= check_sensors(&state_data, &elimination);

    /* Write the elimination Data into the global variable */
    global_elimination_data = elimination;

    /* Do the preprocessing on the IMU and BARO for calibration */
    /* Only do if we are in MOVING */
    if (fsm_state.flight_state == MOVING) {
      average_data(rolling_imu, &imu_counter, rolling_pressure, &pressure_counter, &elimination, &average_imu,
                   &average_pressure);
    }

    /* Do a Kalman Step */
    kalman_step(&filter, &state_data, &elimination, fsm_state.flight_state);

    /* write the Data into the global variable */
    global_kf_data.height = (float)filter.x_bar.pData[0];
    global_kf_data.velocity = (float)filter.x_bar.pData[1];
    global_kf_data.acceleration = state_data.acceleration[1];

    /* DO ORIENTATION KALMAN */
    //    orientation_prediction_step(&orientation_filter, &global_imu[1]);
    //    orientation_update_step(&orientation_filter, &global_imu[1]);
    //    log_trace("%ld; %ld; %ld; %ld",
    //              (int32_t)((float)orientation_filter.x_bar_data[0] * 1000),
    //              (int32_t)((float)orientation_filter.x_bar_data[1] * 1000),
    //              (int32_t)((float)orientation_filter.x_bar_data[2] * 1000),
    //              (int32_t)((float)orientation_filter.x_bar_data[3] * 1000));

    uint32_t ts = osKernelGetTickCount();

    /* Log if Sensor is faulty */
    sensor_info_t sensor_info = {.ts = ts,
                                 .faulty_baro[0] = elimination.faulty_baro[0],
                                 .faulty_baro[1] = elimination.faulty_baro[1],
                                 .faulty_baro[2] = elimination.faulty_baro[2],
                                 .faulty_imu[0] = elimination.faulty_imu[0],
                                 .faulty_imu[1] = elimination.faulty_imu[1],
                                 .faulty_imu[2] = elimination.faulty_imu[2]};
    record(SENSOR_INFO, &sensor_info);

    /* Log Covariance Data of KF */
    covariance_info_t cov_info = {.ts = ts, .height_cov = filter.P_bar.pData[1], .velocity_cov = filter.P_bar.pData[5]};
    record(COVARIANCE_INFO, &cov_info);

    /* Log KF outputs */
    flight_info_t flight_info = {.ts = ts,
                                 .height = filter.x_bar.pData[0],
                                 .velocity = filter.x_bar.pData[1],
                                 .acceleration = filtered_data_info.filtered_acceleration + filter.x_bar.pData[2]};
    if (fsm_state.flight_state >= APOGEE) {
      flight_info.height = filter.x_bar.pData[0];
      flight_info.velocity = filter.x_bar.pData[1];
      flight_info.acceleration = filter.x_bar.pData[2];
    }
    record(FLIGHT_INFO, &flight_info);

    log_trace("Height %ld; Velocity %ld; Acceleration %ld; Offset %ld", (int32_t)((float)filter.x_bar.pData[0] * 1000),
              (int32_t)((float)filter.x_bar.pData[1] * 1000),
              (int32_t)(filtered_data_info.filtered_acceleration * 1000),
              (int32_t)((float)filter.x_bar.pData[2] * 1000));

    //            log_trace("Calibrated IMU 1: Z: %ld",
    //            (int32_t)(1000*state_data.acceleration[0]));
    //            log_trace("Calibrated IMU 2: Z: %ld",
    //            (int32_t)(1000*state_data.acceleration[1]));
    //            log_trace("Calibrated IMU 3: Z: %ld",
    //            (int32_t)(1000*state_data.acceleration[2]));
    /* END DEBUGGING */

    old_fsm_enum = fsm_state.flight_state;

    error_handler(err);
    tick_count += tick_update;
    osDelayUntil(tick_count);
  }
}

/** Private Function Definitions **/

inline static float calculate_height(float pressure_initial, float pressure, float temperature) {
  return ((powf(pressure_initial / pressure, (1 / 5.257f)) - 1) * (temperature + 273.15f) / 0.0065f);
}

static void transform_data(state_estimation_data_t *state_data, kalman_filter_t *filter,
                           calibration_data_t *calibration, flight_fsm_t *fsm_state) {
  /* Get Data from the Sensors */
  /* Use calibration step to get the correct acceleration */
  switch (calibration->axis) {
    case 0:
      /* Choose X Axis */
      state_data->acceleration[0] = (float)(global_imu[0].acc_x) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      state_data->acceleration[1] = (float)(global_imu[1].acc_x) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      state_data->acceleration[2] = (float)(global_imu[2].acc_x) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      break;
    case 1:
      /* Choose Y Axis */
      state_data->acceleration[0] = (float)(global_imu[0].acc_y) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      state_data->acceleration[1] = (float)(global_imu[1].acc_y) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      state_data->acceleration[2] = (float)(global_imu[2].acc_y) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      break;
    case 2:
      /* Choose Z Axis */
      state_data->acceleration[0] = (float)(global_imu[0].acc_z) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      state_data->acceleration[1] = (float)(global_imu[1].acc_z) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      state_data->acceleration[2] = (float)(global_imu[2].acc_z) / (1024) * GRAVITY / calibration->angle - GRAVITY;
      break;
    default:
      break;
  }

  state_data->pressure[0] = (float)(global_baro[0].pressure);
  state_data->pressure[1] = (float)(global_baro[1].pressure);
  state_data->pressure[2] = (float)(global_baro[2].pressure);

  state_data->temperature[0] = (float)global_baro[0].temperature / 100.f;
  state_data->temperature[1] = (float)global_baro[1].temperature / 100.f;
  state_data->temperature[2] = (float)global_baro[2].temperature / 100.f;

  /* Add Sensor Noise if asked to */
  if (fsm_state->flight_state == THRUSTING_1) {
#ifdef INCLUDE_NOISE
    float rand_pressure[3] = {0};
    float rand_acc[3] = {0};
    rand_pressure[0] = PRESSURE_NOISE_MAX_AMPL * ((float)rand() - 2147483648 / 2) / (2147483648 / 2);
    rand_pressure[1] = PRESSURE_NOISE_MAX_AMPL * ((float)rand() - 2147483648 / 2) / (2147483648 / 2);
    rand_pressure[2] = PRESSURE_NOISE_MAX_AMPL * ((float)rand() - 2147483648 / 2) / (2147483648 / 2);
    rand_acc[0] = ACC_NOISE_MAX_AMPL * ((float)rand() - 2147483648 / 2) / (2147483648 / 2);
    rand_acc[1] = ACC_NOISE_MAX_AMPL * ((float)rand() - 2147483648 / 2) / (2147483648 / 2);
    rand_acc[2] = ACC_NOISE_MAX_AMPL * ((float)rand() - 2147483648 / 2) / (2147483648 / 2);

    state_data->pressure[0] += rand_pressure[0];
    state_data->pressure[1] += rand_pressure[1];
    state_data->pressure[2] += rand_pressure[2];
    state_data->acceleration[0] += rand_acc[0];
    state_data->acceleration[1] += rand_acc[1];
    state_data->acceleration[2] += rand_acc[2];
#endif
    /* Add Spikes in the Data if asked to */
#ifdef INCLUDE_SPIKES
    float spike = (float)rand() / 2147483648;
    if (spike < SPIKE_THRESHOLD) {
#ifdef SPIKE_BARO
      state_data->pressure[SPIKE_SENSOR_CHOICE] += 10000000;
#endif
#ifdef SPIKE_IMU
      state_data->acceleration[SPIKE_SENSOR_CHOICE] += 10000000;
#endif
    }
#endif
    /* Add Offset to one Sensor if asked to */
#ifdef INCLUDE_OFFSET
#ifdef OFFSET_BARO
    state_data->pressure[OFFSET_SENSOR_CHOICE] += OFFSET_P;
#endif
#ifdef OFFSET_IMU
    state_data->acceleration[OFFSET_SENSOR_CHOICE] += OFFSET_ACC;
#endif
#endif
  }

  state_data->calculated_AGL[0] =
      calculate_height(filter->pressure_0, state_data->pressure[0], state_data->temperature[0]);
  state_data->calculated_AGL[1] =
      calculate_height(filter->pressure_0, state_data->pressure[1], state_data->temperature[1]);
  state_data->calculated_AGL[2] =
      calculate_height(filter->pressure_0, state_data->pressure[2], state_data->temperature[2]);
}

static void average_data(imu_data_t *rolling_imu, uint8_t *imu_counter, int32_t *rolling_pressure,
                         uint8_t *pressure_counter, sensor_elimination_t *elimination, imu_data_t *average_imu,
                         float *average_pressure) {
  imu_data_t average_imu_from_global = {0};
  /* First average the 3 IMU measurements if no IMUs have been eliminated */
  average_imu_from_global.acc_x = 0;
  average_imu_from_global.acc_y = 0;
  average_imu_from_global.acc_z = 0;
  for (int i = 0; i < 3; i++) {
    if (elimination->faulty_imu[i] == 0 && elimination->num_faulty_imus < 3) {
      average_imu_from_global.acc_x += global_imu[i].acc_x / (3 - elimination->num_faulty_imus);
      average_imu_from_global.acc_y += global_imu[i].acc_y / (3 - elimination->num_faulty_imus);
      average_imu_from_global.acc_z += global_imu[i].acc_z / (3 - elimination->num_faulty_imus);
    }
  }

  /* Write this into the rolling IMU array */
  rolling_imu[*imu_counter] = average_imu_from_global;

  /* Average the rolling IMU Array */
  average_imu->acc_x = 0;
  average_imu->acc_y = 0;
  average_imu->acc_z = 0;
  for (int i = 0; i < 10; i++) {
    average_imu->acc_x += rolling_imu[i].acc_x;
    average_imu->acc_y += rolling_imu[i].acc_y;
    average_imu->acc_z += rolling_imu[i].acc_z;
  }
  average_imu->acc_x /= 10;
  average_imu->acc_y /= 10;
  average_imu->acc_z /= 10;

  /* Increase the counter for the rolling IMU array */
  (*imu_counter)++;
  if ((*imu_counter) > 9) {
    (*imu_counter) = 0;
  }

  /* Do the Baro */
  /* First average the 3 Baro measurements if no Baros have been eliminated
   */
  int32_t global_average_pressure = 0;
  for (int i = 0; i < 3; i++) {
    if (elimination->faulty_baro[i] == 0 && elimination->num_faulty_baros < 3)
      global_average_pressure += global_baro[i].pressure / (3 - elimination->num_faulty_baros);
  }

  /* Write this into the rolling Baro array */
  rolling_pressure[*pressure_counter] = global_average_pressure;

  /* Average the rolling IMU Array */
  *average_pressure = 0;
  for (int i = 0; i < 10; i++) {
    *average_pressure += (float)rolling_pressure[i];
  }
  *average_pressure /= 10.0f;

  /* Increase the counter for the rolling Baro array */
  (*pressure_counter)++;
  if ((*pressure_counter) > 9) {
    (*pressure_counter) = 0;
  }
}

#ifdef USE_MEDIAN_FILTER

static void median_filter(median_filter_t *filter_data, state_estimation_data_t *state_data) {
  filter_data->acc_data[0][filter_data->counter] = state_data->acceleration[0];
  filter_data->acc_data[1][filter_data->counter] = state_data->acceleration[1];
  filter_data->acc_data[2][filter_data->counter] = state_data->acceleration[2];
  filter_data->height_AGL_data[0][filter_data->counter] = state_data->calculated_AGL[0];
  filter_data->height_AGL_data[1][filter_data->counter] = state_data->calculated_AGL[1];
  filter_data->height_AGL_data[2][filter_data->counter] = state_data->calculated_AGL[2];
  filter_data->counter++;
  filter_data->counter = filter_data->counter % MEDIAN_FILTER_SIZE;

  state_data->acceleration[0] = median(*(filter_data->acc_data + 0), MEDIAN_FILTER_SIZE);

  state_data->acceleration[1] = median(*(filter_data->acc_data + 1), MEDIAN_FILTER_SIZE);

  state_data->acceleration[2] = median(*(filter_data->acc_data + 2), MEDIAN_FILTER_SIZE);

  state_data->calculated_AGL[0] = median(*(filter_data->height_AGL_data + 0), MEDIAN_FILTER_SIZE);

  state_data->calculated_AGL[1] = median(*(filter_data->height_AGL_data + 1), MEDIAN_FILTER_SIZE);

  state_data->calculated_AGL[2] = median(*(filter_data->height_AGL_data + 2), MEDIAN_FILTER_SIZE);
}

#endif
