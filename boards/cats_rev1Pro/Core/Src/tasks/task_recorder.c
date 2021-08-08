//
// Created by stoja on 26.12.20.
//

#include "cmsis_os.h"
#include "tasks/task_recorder.h"
#include "util/log.h"
#include "util/types.h"
#include "lfs/lfs_custom.h"
#include "util/recorder.h"
#include "config/cats_config.h"
#include "config/globals.h"
#include "main.h"

#include <stdlib.h>

/** Private Constants **/

static const uint32_t REC_BUFFER_LEN = 256;

/** Private Function Declarations **/

static inline uint_fast8_t get_rec_elem_size(const rec_elem_t *rec_elem);
static inline void write_value(const rec_elem_t *rec_elem, uint8_t *rec_buffer, uint16_t *rec_buffer_idx,
                               uint_fast8_t *rec_elem_size);

/** Exported Function Definitions **/

_Noreturn void task_recorder(__attribute__((unused)) void *argument) {
  uint8_t *rec_buffer = (uint8_t *)calloc(REC_BUFFER_LEN, sizeof(uint8_t));

  uint16_t rec_buffer_idx = 0;
  uint_fast8_t curr_log_elem_size = 0;

  log_debug("Recorder Task Started...\n");

  uint16_t bytes_remaining = 0;
  uint32_t max_elem_count = 0;

  while (1) {
    static uint32_t sync_counter = 0;
    rec_elem_t curr_log_elem;

    /* TODO: check if this should be < or <= */
    while (rec_buffer_idx < REC_BUFFER_LEN) {
      uint32_t curr_elem_count = osMessageQueueGetCount(rec_queue);
      if (max_elem_count < curr_elem_count) {
        max_elem_count = curr_elem_count;
        log_warn("max_queued_elems: %lu", max_elem_count);
      }

      if (global_recorder_status >= REC_WRITE_TO_FLASH) {
        if (osMessageQueueGet(rec_queue, &curr_log_elem, NULL, osWaitForever) == osOK) {
          write_value(&curr_log_elem, rec_buffer, &rec_buffer_idx, &curr_log_elem_size);
        } else {
          log_error("Something wrong with the recording queue!");
        }
      } else if (curr_elem_count > REC_QUEUE_PRE_THRUSTING_LIMIT && global_recorder_status == REC_FILL_QUEUE) {
        /* If the number of elements goes over REC_QUEUE_PRE_THRUSTING_LIMIT we
         * start to empty it. When thrusting is detected we will have around
         * REC_QUEUE_PRE_THRUSTING_LIMIT elements in the queue and in the
         * next loop iteration we will start to write the elements to the flash
         */
        rec_elem_t dummy_log_elem;
        osMessageQueueGet(rec_queue, &dummy_log_elem, NULL, osWaitForever);
      } else {
        /* TODO: see if this is needed */
        osDelay(1);
      }
    }

    /* call LFS function here */
    // w25q_write_page(rec_buffer, page_id * w25q.page_size, 256);
    lfs_file_write(&lfs, &current_flight_file, rec_buffer, 256);

    /* reset log buffer index */
    if (rec_buffer_idx > REC_BUFFER_LEN) {
      bytes_remaining = rec_buffer_idx - REC_BUFFER_LEN;
      rec_buffer_idx = bytes_remaining;
    } else {
      rec_buffer_idx = 0;
    }

    if (rec_buffer_idx > 0) {
      memcpy(rec_buffer, (uint8_t *)(&curr_log_elem) + curr_log_elem_size - bytes_remaining, bytes_remaining);
    }

    if (sync_counter % 16 == 0) {
      /* Flash the LED at certain intervals */
      HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
      lfs_file_sync(&lfs, &current_flight_file);
    }
    ++sync_counter;

    /* TODO: check if there is enough space left on the flash */
  }
}

/** Private Function Definitions **/

static uint_fast8_t get_rec_elem_size(const rec_elem_t *const rec_elem) {
  uint_fast8_t rec_elem_size = sizeof(rec_elem->rec_type);
  switch (rec_elem->rec_type) {
    case IMU0:
    case IMU1:
    case IMU2:
      rec_elem_size += sizeof(rec_elem->u.imu);
      break;
    case BARO0:
    case BARO1:
    case BARO2:
      rec_elem_size += sizeof(rec_elem->u.baro);
      break;
    case MAGNETO:
      rec_elem_size += sizeof(rec_elem->u.magneto_info);
      break;
    case FLIGHT_INFO:
      rec_elem_size += sizeof(rec_elem->u.flight_info);
      break;
    case FILTERED_DATA_INFO:
      rec_elem_size += sizeof(rec_elem->u.filtered_data_info);
      break;
    case FLIGHT_STATE:
      rec_elem_size += sizeof(rec_elem->u.flight_state);
      break;
    case COVARIANCE_INFO:
      rec_elem_size += sizeof(rec_elem->u.covariance_info);
      break;
    case SENSOR_INFO:
      rec_elem_size += sizeof(rec_elem->u.sensor_info);
      break;
    case EVENT_INFO:
      rec_elem_size += sizeof(rec_elem->u.event_info);
      break;
    case ERROR_INFO:
      rec_elem_size += sizeof(rec_elem->u.error_info);
      break;
    default:
      log_fatal("Impossible recorder entry type!");
      break;
  }
  return rec_elem_size;
}

static inline void write_value(const rec_elem_t *const rec_elem, uint8_t *const rec_buffer, uint16_t *rec_buffer_idx,
                               uint_fast8_t *const rec_elem_size) {
  *rec_elem_size = get_rec_elem_size(rec_elem);
  if (*rec_buffer_idx + *rec_elem_size > REC_BUFFER_LEN) {
    memcpy(&(rec_buffer[*rec_buffer_idx]), rec_elem, (REC_BUFFER_LEN - *rec_buffer_idx));
  } else {
    memcpy(&(rec_buffer[*rec_buffer_idx]), rec_elem, *rec_elem_size);
  }
  *rec_buffer_idx += *rec_elem_size;
}

#ifdef FLASH_READ_TEST
uint8_t print_page(uint8_t *rec_buffer, uint8_t print_offset, char prefix, const rec_elem_t *const break_elem) {
  uint8_t bytes_remaining = 0;
  uint32_t i = print_offset;
  rec_elem_t curr_elem;
  if (i > 0) {
    memcpy(((uint8_t *)(break_elem)) + bytes_remaining, &rec_buffer[0], print_offset);
  }
  while (i <= (REC_BUFFER_LEN - sizeof(curr_elem))) {
    curr_elem.rec_type = rec_buffer[i];
    i += sizeof(curr_elem.rec_type);
    log_rawr("%cType: %s, ", prefix, rec_type_map[curr_elem.rec_type]);
    switch (curr_elem.rec_type) {
      case IMU0:
      case IMU1:
      case IMU2:
        memcpy(&(curr_elem.u.imu), &(rec_buffer[i]), sizeof(curr_elem.u.imu));
        i += sizeof(curr_elem.u.imu);
        log_raw("TS: %lu, %d, %d, %d, %d, %d, %d", curr_elem.u.imu.ts, curr_elem.u.imu.gyro_x, curr_elem.u.imu.gyro_y,
                curr_elem.u.imu.gyro_z, curr_elem.u.imu.acc_x, curr_elem.u.imu.acc_y, curr_elem.u.imu.acc_z);
        break;
      case BARO0:
      case BARO1:
      case BARO2:
        memcpy(&(curr_elem.u.baro), &(rec_buffer[i]), sizeof(curr_elem.u.baro));
        i += sizeof(curr_elem.u.baro);
        log_raw("TS: %lu, %ld, %ld", curr_elem.u.baro.ts, curr_elem.u.baro.pressure, curr_elem.u.baro.temperature);
        break;
      case FLIGHT_INFO:
        memcpy(&(curr_elem.u.flight_info), &(rec_buffer[i]), sizeof(curr_elem.u.flight_info));
        i += sizeof(curr_elem.u.flight_info);
        log_raw("TS: %lu, %f, %f, %f", curr_elem.u.flight_info.ts, (double)curr_elem.u.flight_info.height,
                (double)curr_elem.u.flight_info.velocity, (double)curr_elem.u.flight_info.measured_altitude_AGL);
        break;
      case FLIGHT_STATE:
        memcpy(&(curr_elem.u.flight_state), &(rec_buffer[i]), sizeof(curr_elem.u.flight_state));
        i += sizeof(curr_elem.u.flight_state);
        log_raw("TS: %lu, %d", curr_elem.u.flight_state.ts, curr_elem.u.flight_state.flight_state);
        break;
      case COVARIANCE_INFO:
        memcpy(&(curr_elem.u.covariance_info), &(rec_buffer[i]), sizeof(curr_elem.u.covariance_info));
        i += sizeof(curr_elem.u.covariance_info);
        log_raw("TS: %lu, %f, %f", curr_elem.u.covariance_info.ts, (double)curr_elem.u.covariance_info.height_cov,
                (double)curr_elem.u.covariance_info.velocity_cov);
        break;
      case SENSOR_INFO:
        memcpy(&(curr_elem.u.sensor_info), &(rec_buffer[i]), sizeof(curr_elem.u.sensor_info));
        i += sizeof(curr_elem.u.sensor_info);
        log_raw("TS: %lu, %u, %u, %u, %u, %u, %u, %u, %u", curr_elem.u.sensor_info.ts,
                curr_elem.u.sensor_info.faulty_imu[0], curr_elem.u.sensor_info.faulty_imu[1],
                curr_elem.u.sensor_info.faulty_imu[2], curr_elem.u.sensor_info.faulty_baro[0],
                curr_elem.u.sensor_info.faulty_baro[1], curr_elem.u.sensor_info.faulty_baro[2]);
        break;
      default:
        log_fatal("Impossible recorder entry type!");
        break;
    }
  }
  if (REC_BUFFER_LEN - i > 0) {
    bytes_remaining = (REC_BUFFER_LEN - i);
    memcpy(break_elem, &rec_buffer[i], bytes_remaining);
  }
  if (i > REC_BUFFER_LEN) {
    log_fatal("log struct broken, %lu", i);
  }
  return bytes_remaining;
}

static inline void print_elem(const rec_elem_t *const rec_elem, char prefix) {
  char buf[100];
  uint8_t len = sprintf(buf, "%cType: %s, ", prefix, rec_type_map[rec_elem->rec_type]);
  switch (rec_elem->rec_type) {
    case IMU0:
    case IMU1:
    case IMU2:
      sprintf(buf + len, "TS: %lu, %d, %d, %d, %d, %d, %d\n", rec_elem->u.imu.ts, rec_elem->u.imu.gyro_x,
              rec_elem->u.imu.gyro_y, rec_elem->u.imu.gyro_z, rec_elem->u.imu.acc_x, rec_elem->u.imu.acc_y,
              rec_elem->u.imu.acc_z);
      //      sprintf(buf + len, "TS: %lu, %d, %d\n", rec_elem->u.imu.ts,
      //              rec_elem->u.imu.gyro_x, rec_elem->u.imu.acc_x);
      break;
    case BARO0:
    case BARO1:
    case BARO2:
      sprintf(buf + len, "TS: %lu, %ld, %ld\n", rec_elem->u.baro.ts, rec_elem->u.baro.pressure,
              rec_elem->u.baro.temperature);
      break;
    case FLIGHT_INFO:
      // sprintf(buf + len, "TS: %lu\n", rec_elem->u.flight_info.ts);
      sprintf(buf + len, "TS: %lu, %f, %f\n", rec_elem->u.flight_info.ts, (double)rec_elem->u.flight_info.height,
              (double)rec_elem->u.flight_info.velocity);
      break;
    case FLIGHT_STATE:
      sprintf(buf + len, "TS: %lu, %d\n", rec_elem->u.flight_state.ts, rec_elem->u.flight_state.flight_state);
      break;
    case COVARIANCE_INFO:
      sprintf(buf + len, "TS: %lu, %f, %f\n", rec_elem->u.covariance_info.ts,
              (double)rec_elem->u.covariance_info.height_cov, (double)rec_elem->u.covariance_info.velocity_cov);
      break;
    default:
      log_fatal("Impossible recorder entry type!");
      break;
  }
  log_rawr("%s", buf);
}

#endif
