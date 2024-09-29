/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SharpMem.h>

#include "logging/flightStatistics.hpp"
#include "navigation.hpp"
#include "settings.hpp"
#include "telemetry/telemetryData.hpp"

inline constexpr uint16_t BLACK = 0;
inline constexpr uint16_t WHITE = 1;

inline constexpr uint8_t SHARP_SCK = 36;
inline constexpr uint8_t SHARP_MOSI = 35;
inline constexpr uint8_t SHARP_SS = 34;

struct topBarData {
  time_t time;
  uint32_t storage;
  bool saving;
  bool locationLock;
  bool usbDetection;
};

class Window {
 public:
  Window() : display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 400, 240) {}
  void begin();
  void logo();
  void Bootloader();

  void initBar();
  void updateBar(float batteryVoltage, bool usb = false, bool logging = false, bool location = false, bool time = false,
                 uint32_t free_memory = 100);

  void initMenu(int16_t index);
  void updateMenu(int16_t index);

  void initLive();
  void updateLive(TelemetryInfo *info, int16_t index);
  void updateLive(TelemetryData *data, TelemetryInfo *info, int16_t index);
  void updateLive(TelemetryData *data, int16_t index);

  void initRecovery();
  void updateRecovery(Navigation *navigation);

  void initTesting();
  void initTestingConfirmed(bool connected, bool testingEnabled);
  void initTestingFailed();
  void initTestingWait();
  void initTestingReady();
  void initTestingLost();
  void updateTesting(int16_t index);
  void initTestingBox(int16_t index);

  void initData(bool fileAvailable);

  void initSensors();
  void initSensorPrepareCalibrate();
  void initSensorCalibrate();
  void updateSensorCalibrate(Navigation *navigation);
  void initSensorCalibrateDone();
  void updateSensors(Navigation *navigation);

  void initSettings(int16_t submenuIdx);
  void updateSettings(int16_t index);
  void drawSettingsTriangles(int16_t submenuIdx, int16_t color);

  void initBox(const char *text);

  void initKeyboard(char *text, uint32_t maxLength = 0);
  void updateKeyboard(char *text, int32_t keyHighlight, bool keyPressed = false);

  void listFileName(const char *fileName, uint16_t index, uint16_t color = BLACK);
  void dataHighlight(const char *fileName, uint8_t index, bool highlight);
  void dataShowFlightStatistics(FlightStatistics &stats1, FlightStatistics &stats2);

  void refresh() { display.refresh(); }

  static constexpr uint8_t kShiftIdx = 29;
  static constexpr uint8_t kUnderscoreIdx = 37;

 private:
  void updateLiveData(TelemetryData *data, int16_t index, uint16_t color);
  void updateLiveInfo(TelemetryInfo *info, int16_t index, uint16_t color);
  void drawCentreString(const char *buf, int16_t x, int16_t y);
  void drawCentreString(String &buf, int16_t x, int16_t y);

  void addSettingEntry(uint32_t settingIndex, const device_settings_t *setting, uint16_t color = BLACK);
  void highlightSetting(int16_t index, uint16_t color);

  void highlightKeyboardKey(int32_t key, uint16_t color);
  void updateKeyboardText(char *text, uint16_t color);

  void dataShowFlightStatisticsSide(FlightStatistics &stats, uint16_t index);

  void clearMainScreen();

  Adafruit_SharpMem display;

  bool connected[2]{};
  uint32_t lastTeleData[2]{};
  uint32_t dataAge[2]{};
  topBarData barData{};
  TelemetryData teleData[2]{};
  TelemetryInfo infoData[2]{};

  int16_t oldSettingsIndex{0};
  int16_t subMenuSettingIndex{0};

  bool upperCase{false};
  int32_t oldKey{0};
  uint32_t keyboardTextMaxLength{0};

  const char *eventName[9] = {"Ready", "Liftoff", "Burnout", "Apogee", "Main", "Touchdown", "Custom 1", "Custom 2"};
};
