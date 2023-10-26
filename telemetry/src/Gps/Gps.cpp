/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#include "Gps.hpp"
#include "Main.hpp"

#include <cstdio>

constexpr static uint8_t ublox_request_115200_baud[] = {
    0xb5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xd0, 0x08, 0x00, 0x00, 0x00, 0xc2, 0x01, 0x00, 0x07,
    0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc4, 0x96, 0xb5, 0x62, 0x06, 0x00, 0x01, 0x00, 0x01, 0x08, 0x22};
constexpr static uint8_t ublox_request_5Hz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8,
                                                0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern UART_HandleTypeDef huart1;

void gpsSetup() {
  uint8_t command[20];

  // Check hardware version
  if (HAL_GPIO_ReadPin(HARDWARE_ID_GPIO_Port, HARDWARE_ID_Pin) == GPIO_PIN_SET) {
    // Flight computer
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) safe to cast as transmitted data is not changed
    HAL_UART_Transmit(&huart1, const_cast<uint8_t *>(ublox_request_115200_baud), sizeof(ublox_request_115200_baud),
                      100);
  } else {
    // Groundstation
    /* Request UART speed of 115200 */
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) fine to cast since char is 8 bits on this platform
    snprintf(reinterpret_cast<char *>(command), 15, "$PCAS01,5*19\r\n");
    HAL_UART_Transmit(&huart1, command, 14, 100);
  }

  HAL_Delay(200);

  /* Change bus speed to 115200 */
  USART1->CR1 &= ~(USART_CR1_UE);
  USART1->BRR = 417;  // Set baud to 115200
  USART1->CR1 |= USART_CR1_UE;

  HAL_Delay(200);

  // Check hardware version
  if (HAL_GPIO_ReadPin(HARDWARE_ID_GPIO_Port, HARDWARE_ID_Pin) == GPIO_PIN_SET) {
    // Flight computer
    /* Request 5 Hz mode */
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) safe to cast as transmitted data is not changed
    HAL_UART_Transmit(&huart1, const_cast<uint8_t *>(ublox_request_5Hz), sizeof(ublox_request_5Hz), 100);
    /* Request airbourne, not working yet */
    // HAL_UART_Transmit(&huart1, ublox_request_airbourne,
    // sizeof(ublox_request_airbourne), 100);
  } else {
    // Groundstation
    /* Request 10Hz update rate */
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) fine to cast since char is 8 bits on this platform
    snprintf(reinterpret_cast<char *>(command), 17, "$PCAS02,100*1E\r\n");
    HAL_UART_Transmit(&huart1, command, 16, 100);
  }
}
