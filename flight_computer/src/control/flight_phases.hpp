/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "util/types.hpp"

#include <cstdint>

/* CALIBRATING */
// num iterations, imu action needs to be 0 for at least 10 seconds
inline constexpr uint16_t TIME_THRESHOLD_CALIB_TO_READY = 1000;

// m/s^2, if the IMU measurement is smaller than 0.6 m/s^2 it is not considered as movement for the transition
// CALIBRATING -> READY
inline constexpr float ALLOWED_ACC_ERROR_CALIB = 0.6F;

// dps, if the GYRO measurement is smaller than 10 dps it is not considered as movement for the transition CALIBRATING
// -> READY
inline constexpr float ALLOWED_GYRO_ERROR_CALIB = 10.0F;

/* READY */

// num iterations, if the acceleration is bigger than the threshold for 0.1 s we detect liftoff
inline constexpr uint16_t LIFTOFF_SAFETY_COUNTER = 10;

/* THRUSTING */
// num iterations, acceleration needs to be smaller than 0 for at least 0.1 s for the transition THRUSTING -> COASTING
inline constexpr uint16_t COASTING_SAFETY_COUNTER = 10;

/* COASTING */
// num iterations, velocity needs to be smaller than 0 for at least 0.3 s for the transition COASTING -> DROGUE
inline constexpr uint16_t APOGEE_SAFETY_COUNTER = 30;

/* DROGUE */
// num iterations, height needs to be smaller than user-defined for at least 0.3 s for the transition DROGUE -> MAIN
inline constexpr uint16_t MAIN_SAFETY_COUNTER = 30;
// tick counts [ms]
inline constexpr uint16_t MIN_TICK_COUNTS_BETWEEN_THRUSTING_APOGEE = 1500;

/* MAIN */
// m/s^2, if the IMU measurement is smaller than 0.6 m/s^2 it is not considered as movement for the transition
// MAIN -> TOUCHDWON
inline constexpr float ALLOWED_ACC_ERROR_TD = 0.6F;
// dps, if the GYRO measurement is smaller than 10 dps it is not considered as movement for the transition MAIN
// -> TOUCHDWON
inline constexpr float ALLOWED_GYRO_ERROR_TD = 10.0F;

// num iterations, for at least 3s there can't be imu movement
inline constexpr uint16_t TOUCHDOWN_SAFETY_COUNTER = 300;

/* Function which implements the FSM */
void check_flight_phase(flight_fsm_t *fsm_state, vf32_t acc_data, vf32_t gyro_data, estimation_output_t state_data,
                        const control_settings_t *settings);
