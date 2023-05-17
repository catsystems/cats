/// CATS Flight Software
/// Copyright (C) 2022 Control and Telemetry Systems
///
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>

typedef enum {
  TX = 0,
  RX = 1,
} transmission_direction_e;

#define ESC_CHAR 0xFF

typedef enum {
  UNIDIRECTIONAL = 0,
  BIDIRECTIONAL = 1,
} transmission_mode_e;

typedef enum {
  connected,
  tentative,
  disconnected,
} connectionState_e;

typedef struct modulation_settings_s {
  uint8_t bw;
  uint8_t sf;
  uint8_t cr;
  uint32_t interval;
  uint8_t PreambleLen;
  uint8_t PayloadLength;
} modulation_settings_t;

#define CMD_DIRECTION   0x10
#define CMD_PA_GAIN     0x11
#define CMD_POWER_LEVEL 0x12
#define CMD_MODE        0x13
#define CMD_MODE_INDEX  0x14

#define CMD_LINK_PHRASE 0x15

#define CMD_ENABLE  0x20
#define CMD_DISBALE 0x21

#define CMD_TX   0x30
#define CMD_RX   0x31
#define CMD_INFO 0x32

#define CMD_GNSS_LOC  0x40
#define CMD_GNSS_TIME 0x41
#define CMD_GNSS_INFO 0x42

#define CMD_TEMP_INFO 0x50

#define CMD_VERSION_INFO 0x60

#define CMD_BOOTLOADER 0x80
