/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "JC_Button.h"
#include "config.hpp"
#include "logging/recorder.hpp"
#include "window.hpp"

class Hmi {
 public:
  explicit Hmi(const char* dir)
      : recorder(dir),
        upButton(3),
        downButton(4),
        leftButton(2),
        rightButton(5),
        centerButton(1),
        okButton(7),
        backButton(6) {}

  void begin();

 private:
  enum State {
    MENU = 0,
    LIVE = 1,
    RECOVERY = 2,
    TESTING = 3,
    DATA = 4,
    SENSORS = 5,
    SETTINGS = 6,
  };

  enum TestingState {
    DISCLAIMER = 0,
    CAN_START = 1,
    CAN_NOT_START = 2,
    WAIT_FOR_START = 3,
    FAILED = 4,
    STARTED = 5,
  };

  enum CalibrationState {
    IDLE = 0,
    PREPARE = 1,
    CALIBRATING = 2,
    CONCLUDED = 3,
  };

  State state = MENU;

  TestingState testingState = DISCLAIMER;
  uint32_t startTestingTime = 0;
  int16_t testingIndex = 0;

  CalibrationState calibrationState = IDLE;

  Recorder recorder;

  uint16_t oldTimeStamp = 0;

  int16_t settingSubMenu = 0;
  int16_t settingIndex = -1;
  char keyboardString[kMaxPhraseLen + 1] = {};

  static void update(void* pvParameter);

  void fsm();
  void initMenu();
  void menu();
  void initLive();
  void live();
  void initRecovery();
  void recovery();
  void initTesting();
  void testing();
  void initData();
  void data();
  void initSensors();
  void sensors();
  void initSettings();
  void settings();

  bool initialized = false;
  bool isLogging = false;
  bool boxWindow = false;
  bool enableTestMode = false;
  bool triggerTouchdown = false;
  bool isCalibrating = false;

  Button upButton;
  Button downButton;
  Button leftButton;
  Button rightButton;
  Button centerButton;

  Button okButton;
  Button backButton;

  Window window;

  int16_t menuIndex = 0;

  uint8_t dataIndex = 0;
  uint8_t dataFileCount = 0;
  bool dataFlightStatistic = false;

  uint32_t flashFreeMemory = 100;

  bool link1Log = false;
  bool link2Log = false;
};
