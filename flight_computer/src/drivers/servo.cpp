/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#include "servo.hpp"

namespace driver {

void Servo::SetPosition(uint16_t ticks) {
  if (ticks > 1000) {
    ticks = 1000U;
  }

  // Get the output depth
  const auto depth = static_cast<float32_t>(m_pwm_channel.GetPwmDepth());

  // Servos operate between 2.5% and 12.5% duty cycle -> 10% usage of the pwm depth
  const float32_t single_tick = (depth * 0.1F) / 1000.0F;
  const auto output = static_cast<uint32_t>(single_tick * static_cast<float32_t>(ticks) + 0.025F * depth);
  m_pwm_channel.SetDutyCycleTicks(output);
}

}  // namespace driver
