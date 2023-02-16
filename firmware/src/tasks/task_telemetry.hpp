/*
 * CATS Flight Software
 * Copyright (C) 2023 Control and Telemetry Systems
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

#include "task.hpp"
#include "task_state_est.hpp"

namespace task {

class Telemetry final : public Task<Telemetry, 1024> {
 public:
  explicit Telemetry(const StateEstimation* task_state_estimation)
      : m_testing_enabled{global_cats_config.enable_testing_mode}, m_task_state_estimation{task_state_estimation} {}

 private:
  [[noreturn]] void Run() noexcept override;

  struct packed_tx_msg_t {
    uint8_t state : 3;
    uint16_t timestamp : 15;
    uint8_t errors : 6;
    int32_t lat : 22;
    int32_t lon : 22;
    int32_t altitude : 17;
    int16_t velocity : 10;
    uint16_t voltage : 8;
    uint16_t pyro_continuity : 2;
    bool testing_on : 1;
    // fill up to 16 bytes
    uint8_t : 0;  // sent
    uint8_t d1;   // dummy
  } __attribute__((packed));

  struct packed_rx_msg_t {
    /* Header used to check if the packet is used for arming */
    uint8_t header;
    /* Testing passcode, only if this matches with the configured passcode is the packet accepted. */
    uint32_t passcode;
    /* Event which needs to be triggered */
    uint8_t event;
    /* If this bit is set to one, the flight computer arms itself for testing; only then can events be triggered */
    bool enable_testing_telemetry;
    uint32_t dummy1;
    uint32_t dummy2;
  } __attribute__((packed));

  uint32_t m_test_phrase_crc = 0;
  /* used to notify the groundstation that the flight computer is in testing mode */
  bool m_testing_enabled;
  /* used to notify the groundstation if the testing is armed */
  bool m_testing_armed = false;
  /* used to check if the current groundstation event was already triggered and that we are waiting for the
   * groundstation to reset the event */
  bool m_event_reset = false;
  /* timeout to check if data is received from the groundstation for the testing mode */
  uint32_t m_testing_timeout = 0;
  static constexpr uint32_t RX_PACKET_HEADER{0x72}; /* Random Header */

  void PackTxMessage(uint32_t ts, gnss_data_t* gnss, packed_tx_msg_t* tx_payload,
                     estimation_output_t estimation_data) const noexcept;
  void ParseRxMessage(packed_rx_msg_t* rx_payload) noexcept;
  bool Parse(uint8_t op_code, const uint8_t* buffer, uint32_t length, gnss_data_t* gnss) noexcept;
  static void SendLinkPhrase(uint8_t* phrase, uint32_t length) noexcept;
  void SendSettings(uint8_t command, uint8_t value) const noexcept;
  void SendEnable() const noexcept;
  void SendDisable() const noexcept;
  void SendTxPayload(uint8_t* payload, uint32_t length) const noexcept;
  [[nodiscard]] bool CheckValidOpCode(uint8_t op_code) const noexcept;

  const StateEstimation* m_task_state_estimation;

  float32_t m_amplifier_temperature{0.0F};

  static constexpr float32_t k_amplifier_hot_limit{60.F};
};

}  // namespace task
