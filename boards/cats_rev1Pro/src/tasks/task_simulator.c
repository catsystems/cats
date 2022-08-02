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

#include "tasks/task_simulator.h"
#include "config/globals.h"
#include "target.h"
#include "util/log.h"
#include "util/task_util.h"

#include <stdlib.h>

/** Private Constants **/
/* DataPoints Acceleration Rocket*/
timestamp_t acc_time_array[5] = {0, 9000000, 9000000, 9000000, 9000000};
float32_t acc_array[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

/* DataPoints Pressure Rocket*/
timestamp_t pressure_time_array[10] = {0,       9000000, 9000000, 9000000, 9000000,
                                       9000000, 9000000, 9000000, 9000000, 9000000};
float32_t pressure_array[10] = {98000.0f, 98000.0f, 98000.0f, 98000.0f, 98000.0f,
                                98000.0f, 98000.0f, 98000.0f, 98000.0f, 98000.0f};

/* DataPoints Acceleration Rocket*/
timestamp_t acc_rocket_time_array[3] = {20000, 21000, 9000000};
float32_t acc_rocket_array[3] = {1.0f, 10.0f, 0.0f};

/* DataPoints Pressure Rocket*/
timestamp_t pressure_rocket_time_array[8] = {0, 20000, 23000, 26000, 28000, 48000, 70000, 9000000};
float32_t pressure_rocket_array[8] = {98000.0f, 98000.0f, 96000.0f, 94600.0f, 94000.0f, 96500.0f, 98000.0f, 98000.0f};

/* DataPoints Acceleration Hop*/
timestamp_t acc_hop_time_array[3] = {15000, 15500, 9000000};
float32_t acc_hop_array[3] = {1.0f, 4.0f, 0.0f};

/* DataPoints Pressure */
timestamp_t pressure_hop_time_array[5] = {0, 15500, 17000, 20000, 9000000};
float32_t pressure_hop_array[5] = {98000.0f, 98000.0f, 96500.0f, 98000.0f, 98000.0f};

SET_TASK_PARAMS(task_simulator, 512)

/** Private Function Declarations **/
int32_t linear_interpol(float32_t time, float32_t LB_time, float32_t UB_time, float32_t LB_val, float32_t UB_val);
int32_t rand_bounds(int32_t lower_b, int32_t upper_b);
void init_simulation_data(cats_sim_choice_e sim_choice);

/** Exported Function Definitions **/

/**
 * @brief Function implementing the task_state_est thread.
 * @param argument: Simulation Choice
 * @retval None
 */
[[noreturn]] void task_simulator(void *args) {
  cats_sim_config_t sim_config = *(cats_sim_config_t *)args;
  /* Free pointer to sim_config from the simulation_start function */
  vPortFree(args);
  /* Change when the timestamp is reached */
  int8_t index_acc = 0;
  /* Linear interpolation */
  int8_t index_press = 0;
  imu_data_t sim_imu_data[NUM_IMU] = {};

  log_mode_e prev_log_mode = log_get_mode();
  log_set_mode(LOG_MODE_SIM);

  /* RNG Init with known seed */
  srand(sim_config.noise_seed);

  uint32_t tick_count = osKernelGetTickCount();
  uint32_t tick_update = osKernelGetTickFreq() / CONTROL_SAMPLING_FREQ;

  /* initialise time */
  timestamp_t sim_start = osKernelGetTickCount();
  timestamp_t time_since_start = 0;

  init_simulation_data(sim_config.sim_choice);

  while (1) {
    time_since_start = osKernelGetTickCount() - sim_start;

    /* Check if we need to use new acceleration datapoint */
    if (time_since_start > acc_time_array[index_acc]) {
      index_acc++;
    }

    /* Compute wanted acceleration */
    switch (sim_config.sim_axis) {
      case 0:
        for (int i = 0; i < NUM_IMU; i++) {
          sim_imu_data[i].acc.x = (int16_t)(1024 * acc_array[index_acc]) + (int16_t)rand_bounds(-10, 10);
          sim_imu_data[i].acc.y = (int16_t)rand_bounds(-10, 10);
          sim_imu_data[i].acc.z = (int16_t)rand_bounds(-10, 10);
        }
        break;
      case 1:
        for (int i = 0; i < NUM_IMU; i++) {
          sim_imu_data[i].acc.x = (int16_t)rand_bounds(-10, 10);
          sim_imu_data[i].acc.y = (int16_t)(1024 * acc_array[index_acc]) + (int16_t)rand_bounds(-10, 10);
          sim_imu_data[i].acc.z = (int16_t)rand_bounds(-10, 10);
        }
        break;
      case 2:
        for (int i = 0; i < NUM_IMU; i++) {
          sim_imu_data[i].acc.x = (int16_t)rand_bounds(-10, 10);
          sim_imu_data[i].acc.y = (int16_t)rand_bounds(-10, 10);
          sim_imu_data[i].acc.z = (int16_t)(1024 * acc_array[index_acc]) + (int16_t)rand_bounds(-10, 10);
        }
        break;
    }

    /* Write into global imu sim variable */
    for (int i = 0; i < NUM_IMU; i++) {
      memcpy(&global_imu_sim[i].acc, &sim_imu_data[i].acc, sizeof(vi16_t));
    }

    /* Check if we need to use new barometer datapoint */
    if (time_since_start > pressure_time_array[index_press + 1]) {
      index_press++;
    }
    /* Do linear interpolation for pressure measurement */
    float32_t pressure = linear_interpol((float32_t)time_since_start, (float32_t)pressure_time_array[index_press],
                                         (float32_t)pressure_time_array[index_press + 1], pressure_array[index_press],
                                         pressure_array[index_press + 1]);

    /* Write into global pressure sim variable */
    for (int i = 0; i < NUM_BARO; i++) {
      global_baro_sim[i].pressure = pressure + rand_bounds(-25, 25);
    }



    if (global_flight_state.flight_state == TOUCHDOWN) {
      log_raw("Simulation Successful.");
      log_set_mode(prev_log_mode);
      osThreadExit();
    }

    tick_count += tick_update;
    osDelayUntil(tick_count);
  }
}

/** Private Function Definitions **/

void init_simulation_data(cats_sim_choice_e sim_choice) {
  if (sim_choice == SIM_HOP) {
    memcpy(&acc_time_array[0], &acc_hop_time_array[0], 3 * sizeof(timestamp_t));
    memcpy(&acc_array[0], &acc_hop_array[0], 3 * sizeof(float32_t));
    memcpy(&pressure_time_array[0], &pressure_hop_time_array[0], 4 * sizeof(timestamp_t));
    memcpy(&pressure_array[0], &pressure_hop_array[0], 4 * sizeof(float32_t));
  } else if (sim_choice == SIM_300M) {
    memcpy(&acc_time_array[0], &acc_rocket_time_array[0], 3 * sizeof(timestamp_t));
    memcpy(&acc_array[0], &acc_rocket_array[0], 3 * sizeof(float32_t));
    memcpy(&pressure_time_array[0], &pressure_rocket_time_array[0], 8 * sizeof(timestamp_t));
    memcpy(&pressure_array[0], &pressure_rocket_array[0], 8 * sizeof(float32_t));
  }
}

int32_t linear_interpol(float32_t time, float32_t LB_time, float32_t UB_time, float32_t LB_val, float32_t UB_val) {
  return (int32_t)(((time - LB_time) / (UB_time - LB_time)) * (UB_val - LB_val) + LB_val);
}

int32_t rand_bounds(int32_t lower_b, int32_t upper_b) { return rand() % (upper_b - lower_b) - lower_b; }

void start_simulation(char *args) {
  static osThreadId_t task_simulator_id = NULL;

  if (task_simulator_id != NULL) {
    log_raw("Simulation already started.");
    return;
  }

  cats_sim_config_t *sim_config;
  sim_config = (cats_sim_config_t *)pvPortMalloc(sizeof(cats_sim_config_t));
  sim_config->noise_seed = 1;
  sim_config->sim_axis = 0;
  sim_config->sim_choice = SIM_HOP;
  char *token = strtok(args, " ");

  while (token != NULL) {
    if (strcmp(token, "--hop") == 0) {
      sim_config->sim_choice = SIM_HOP;
    }
    if (strcmp(token, "--300m") == 0) {
      sim_config->sim_choice = SIM_300M;
    }
    if (strcmp(token, "--x") == 0) {
      sim_config->sim_axis = 0;
    }
    if (strcmp(token, "--y") == 0) {
      sim_config->sim_axis = 1;
    }
    if (strcmp(token, "--z") == 0) {
      sim_config->sim_axis = 2;
    }
    if (strcmp(token, "--ns1") == 0) {
      sim_config->noise_seed = 1;
    }
    if (strcmp(token, "--ns10") == 0) {
      sim_config->noise_seed = 10;
    }
    if (strcmp(token, "--ns69") == 0) {
      sim_config->noise_seed = 69;
    }
    token = strtok(NULL, " ");
  }
  simulation_started = true;
  log_info("Starting simulation, enable log (Ctrl + L) to see simulation outputs...");
  task_simulator_id = osThreadNew(task_simulator, (void *)sim_config, &task_simulator_attributes);
}
