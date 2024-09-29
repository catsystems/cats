/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#include "hmi.hpp"

#include <TimeLib.h>
#include <freertos/task.h>
#include <algorithm>

#include "console.hpp"
#include "logging/flightStatistics.hpp"
#include "navigation.hpp"
#include "telemetry/telemetry.hpp"
#include "utils.hpp"

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern Telemetry link1;
extern Telemetry link2;

extern Navigation navigation;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void Hmi::begin() {
  upButton.begin();
  downButton.begin();
  leftButton.begin();
  rightButton.begin();
  centerButton.begin();

  okButton.begin();
  backButton.begin();

  recorder.begin();
  recorder.enable();

  window.begin();
  initialized = true;
  xTaskCreate(update, "task_hmi", 8196, this, 1, nullptr);
}

void Hmi::fsm() {
  switch (state) {
    case MENU:
      menu();
      break;

    case LIVE:
      live();
      break;

    case RECOVERY:
      recovery();
      break;

    case TESTING:
      testing();
      break;

    case DATA:
      data();
      break;

    case SENSORS:
      sensors();
      break;

    case SETTINGS:
      settings();
      break;

    default:
      break;
  }
}

/* MENU */

void Hmi::initMenu() { window.initMenu(menuIndex); }

void Hmi::menu() {
  const uint32_t oldIndex = menuIndex;
  if (rightButton.wasPressed() && (menuIndex % 3) < 2) {
    menuIndex++;
  }

  if (leftButton.wasPressed() && (menuIndex % 3) > 0) {
    menuIndex--;
  }

  if (downButton.wasPressed() && menuIndex < 3) {
    menuIndex += 3;
  }

  if (upButton.wasPressed() && menuIndex > 2) {
    menuIndex -= 3;
  }

  if (menuIndex != oldIndex) {
    window.updateMenu(menuIndex);
  }

  if (okButton.wasPressed() || centerButton.wasPressed()) {
    state = static_cast<State>(menuIndex + 1);
    if (state == LIVE) {
      initLive();
    } else if (state == RECOVERY) {
      initRecovery();
    } else if (state == TESTING) {
      initTesting();
    } else if (state == DATA) {
      initData();
    } else if (state == SENSORS) {
      initSensors();
    } else if (state == SETTINGS) {
      initSettings();
    }
  }
}

/* LIVE */

void Hmi::initLive() { window.initLive(); }

void Hmi::live() {
  bool updated = false;

  if (link1.data.isUpdated() && link1.info.isUpdated()) {
    // We log after LIFTOFF and stop either never (if neverStopLogging == TRUE) or at TOUCHDOWN
    link1Log = (link1.data.state() > 2) && (systemConfig.config.neverStopLogging || link1.data.state() < 7);
    if (link1Log) {
      recorder.record(&link1.data.getRxData(), 1);
    }
    window.updateLive(&link1.data, &link1.info, 0);
    updated = true;
  } else if (link1.info.isUpdated()) {
    window.updateLive(&link1.info, 0);
    updated = true;
  }

  if (link2.data.isUpdated() && link2.info.isUpdated()) {
    // We log after LIFTOFF and stop either never (if neverStopLogging == TRUE) or at TOUCHDOWN
    link2Log = (link2.data.state() > 2) && (systemConfig.config.neverStopLogging || link2.data.state() < 7);
    if (link2Log) {
      recorder.record(&link2.data.getRxData(), 2);
    }
    window.updateLive(&link2.data, &link2.info, 1);
    updated = true;
  } else if (link2.info.isUpdated()) {
    window.updateLive(&link2.info, 1);
    updated = true;
  }

  isLogging = link1Log || link2Log;

  if (updated) {
    window.refresh();
  }

  if (backButton.wasPressed()) {
    state = MENU;
    isLogging = false;
    window.initMenu(menuIndex);
  }
}

/* RECOVERY */

void Hmi::initRecovery() { window.initRecovery(); }

void Hmi::recovery() {
  EarthPoint3D a;
  EarthPoint3D b;

  a = navigation.getPointA();
  b = navigation.getPointB();

  if (navigation.isUpdated()) {
    window.updateRecovery(&navigation);
  }

  if (backButton.wasPressed()) {
    state = MENU;
    window.initMenu(menuIndex);
  }
}

/* TESTING */

void Hmi::initTesting() { window.initTesting(); }

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void Hmi::testing() {
  if (boxWindow) {
    bool exit = false;
    if (okButton.wasPressed()) {
      link1.triggerEvent(testingIndex + 1);
      exit = true;
    }

    if (backButton.wasPressed() || exit) {
      window.initTestingReady();
      window.updateTesting(testingIndex);
      boxWindow = false;
    }
    return;
  }

  if (backButton.wasPressed()) {
    state = MENU;
    if (testingState >= WAIT_FOR_START) {
      link1.exitTesting();
      link2.enable();
    }
    testingState = DISCLAIMER;

    window.initMenu(menuIndex);
  }

  switch (testingState) {
    case DISCLAIMER: {
      if (okButton.wasPressed()) {
        bool connected = false;
        if ((link1.data.getLastUpdateTime() + 1000) > xTaskGetTickCount()) {
          connected = true;
        }

        window.initTestingConfirmed(connected, link1.data.testingMode());
        if (connected) {
          testingState = CAN_START;
        } else {
          testingState = CAN_NOT_START;
        }
      }
    } break;

    case CAN_START: {
      if (okButton.wasPressed()) {
        // Disable Link2
        link2.disable();
        link1.enterTesting();

        window.initTestingWait();

        startTestingTime = xTaskGetTickCount();
        testingState = WAIT_FOR_START;
      }
    } break;

    case WAIT_FOR_START: {
      static uint32_t counter = 0;
      if (link1.data.isUpdated()) {
        // In testing mode state indicates if we sucessfully started the mode
        if ((link1.data.getLastUpdateTime() + 200) > xTaskGetTickCount()) {
          counter++;
          if (link1.data.state() == 1 && counter > 5) {
            window.initTestingReady();
            window.updateTesting(0);
            testingIndex = 0;
            testingState = STARTED;
            counter = 0;
          }
        } else {
          counter = 0;
        }
      }
      if ((startTestingTime + 10000) < xTaskGetTickCount()) {
        link1.disable();
        window.initTestingFailed();
        testingState = FAILED;
        counter = 0;
      }
    } break;

    case CAN_NOT_START:
    case FAILED:
      break;

    case STARTED: {
      const uint32_t oldIndex = testingIndex;
      if (upButton.wasPressed() && (testingIndex % 4) > 0) {
        testingIndex--;
      } else if (downButton.wasPressed() && (testingIndex % 4) < 3) {
        testingIndex++;
      } else if (rightButton.wasPressed() && (testingIndex < 4)) {
        testingIndex += 4;
      } else if (leftButton.wasPressed() && (testingIndex > 3)) {
        testingIndex -= 4;
      }

      if (link1.data.isUpdated()) {
        if (link1.data.state() != 1) {
          testingState = FAILED;
          link1.exitTesting();
          window.initTestingLost();
        }
      }

      if ((link1.data.getLastUpdateTime() + 1000) < xTaskGetTickCount()) {
        testingState = FAILED;
        link1.exitTesting();
        window.initTestingLost();
      }

      if (oldIndex != testingIndex) {
        window.updateTesting(testingIndex);
      }

      if (okButton.wasPressed()) {
        window.initTestingBox(testingIndex);
        boxWindow = true;
      }
    } break;

    default:
      break;
  }
}

/* DATA */

void Hmi::initData() {
  dataFileCount = recorder.getFileCount();

  // The display can show a maximum of 11 entries, no scrolling implemented!
  // It is highly unlikely to get more than 11 flight logs because the flash is quite small
  dataFileCount = std::min<decltype(dataFileCount)>(dataFileCount, 11);

  if (dataFileCount > 0) {
    window.initData(true);
    char fileName[30] = {};
    for (uint8_t i = 0; i < dataFileCount; i++) {
      recorder.getFileNameByIndex(i, fileName);
      if (i == dataIndex) {
        window.dataHighlight(fileName, i, true);
      } else {
        window.listFileName(fileName, i);
      }
    }
  } else {
    window.initData(false);
  }
  window.refresh();
}

void Hmi::data() {
  if (dataFlightStatistic) {
    if (backButton.wasPressed()) {
      dataFlightStatistic = false;
      initData();
    }
  } else {
    if (backButton.wasPressed()) {
      state = MENU;
      window.initMenu(menuIndex);
      dataIndex = 0;
    }
    if (downButton.wasPressed() && dataIndex < (dataFileCount - 1)) {
      dataIndex++;
      char fileName[30] = {};
      if (dataIndex >= 1) {
        recorder.getFileNameByIndex(dataIndex - 1, fileName);
        window.dataHighlight(fileName, dataIndex - 1, false);
      }
      recorder.getFileNameByIndex(dataIndex, fileName);
      window.dataHighlight(fileName, dataIndex, true);
      window.refresh();
    }
    if (upButton.wasPressed() && dataIndex > 0) {
      dataIndex--;
      char fileName[30] = {};
      if (dataIndex < (dataFileCount - 1)) {
        recorder.getFileNameByIndex(dataIndex + 1, fileName);
        window.dataHighlight(fileName, dataIndex + 1, false);
      }
      recorder.getFileNameByIndex(dataIndex, fileName);
      window.dataHighlight(fileName, dataIndex, true);
      window.refresh();
    }

    if (okButton.wasPressed() && (dataFileCount > 0)) {
      FlightStatistics stats1;
      FlightStatistics stats2;
      char fileName[30] = {};
      // NOLINTNEXTLINE(cppcoreguidelines-init-variables) not sure why...
      const char *directory = recorder.getDirectory();
      recorder.getFileNameByIndex(dataIndex, fileName);
      stats1.parseFlightLog(directory, fileName, 1U);
      stats2.parseFlightLog(directory, fileName, 2U);
      dataFlightStatistic = true;
      window.dataShowFlightStatistics(stats1, stats2);
    }
  }
}

/* SENSORS */

void Hmi::initSensors() { window.initSensors(); }

void Hmi::sensors() {
  switch (calibrationState) {
    case IDLE:

      window.updateSensors(&navigation);

      if (okButton.wasPressed()) {
        window.initSensorPrepareCalibrate();
        calibrationState = PREPARE;
      }
      if (backButton.wasPressed()) {
        state = MENU;
        window.initMenu(menuIndex);
      }
      break;
    case PREPARE:
      if (okButton.wasPressed()) {
        window.initSensorCalibrate();
        calibrationState = CALIBRATING;
        navigation.setCalibrationState(navigation.CALIB_ONGOING);
      }
      if (backButton.wasPressed()) {
        calibrationState = IDLE;
        window.initSensors();
      }
      break;
    case CALIBRATING:
      window.updateSensorCalibrate(&navigation);
      if (backButton.wasPressed()) {
        navigation.setCalibrationState(navigation.CALIB_CANCELLED);
        calibrationState = IDLE;
        window.initSensors();
        return;
      }
      if (navigation.getCalibrationState() == navigation.CALIB_CONCLUDED) {
        calibrationState = CONCLUDED;
        window.initSensorCalibrateDone();
      }
      break;
    case CONCLUDED:
      if (okButton.wasPressed() || backButton.wasPressed()) {
        calibrationState = IDLE;
        systemConfig.save();
        window.initSensors();
      }
      break;
    default:
      break;
  }
}

/* SETTINGS */

void Hmi::initSettings() {
  settingSubMenu = 0;
  settingIndex = -1;
  window.initSettings(settingSubMenu);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void Hmi::settings() {
  static bool keyboardActive = false;
  static bool configChanged = false;
  static int32_t i = 0;
  if (keyboardActive) {
    if (rightButton.wasPressed() || rightButton.pressedFor(500)) {
      if (i != 9 && i != 19 && i != 28 && i != 37) {
        i++;
        window.updateKeyboard(keyboardString, i);
      }
    }
    if (leftButton.wasPressed() || leftButton.pressedFor(500)) {
      if (i != -1 && i != 0 && i != 10 && i != 20 && i != 29) {
        i--;
        window.updateKeyboard(keyboardString, i);
      }
    }
    if (downButton.wasPressed() || downButton.pressedFor(500)) {
      if (i == -1) {
        i = 7;
        window.updateKeyboard(keyboardString, i);
      } else if (i < 15) {
        i += 10;
        window.updateKeyboard(keyboardString, i);
      } else if (i < 29) {
        i += 9;
        window.updateKeyboard(keyboardString, i);
      }
    }
    if (upButton.wasPressed() || upButton.pressedFor(500)) {
      if (i > 9) {
        if (i < 25) {
          i -= 10;
          window.updateKeyboard(keyboardString, i);
        } else if (i < 38) {
          i -= 9;
          window.updateKeyboard(keyboardString, i);
        }
      } else {
        i = -1;
        window.updateKeyboard(keyboardString, i);
      }
    }
    if (okButton.wasPressed() || okButton.pressedFor(500)) {
      window.updateKeyboard(keyboardString, i, true);
    }

    if (backButton.wasPressed()) {
      memcpy(static_cast<char *>(settingsTable[settingSubMenu][settingIndex].dataPtr), keyboardString, kMaxPhraseLen);
      keyboardString[kMaxPhraseLen] = '\0';
      window.initSettings(settingSubMenu);
      configChanged = true;
      window.updateSettings(settingIndex);
      keyboardActive = false;
    }

  } else {
    if (settingIndex == -1) {
      if (rightButton.wasPressed() && settingSubMenu < (kSettingPages - 1)) {
        settingSubMenu++;
        window.initSettings(settingSubMenu);
      }

      if (leftButton.wasPressed() && settingSubMenu > 0) {
        settingSubMenu--;
        window.initSettings(settingSubMenu);
      }
    } else {
      void *data_ptr = settingsTable[settingSubMenu][settingIndex].dataPtr;
      const settings_limits_u &cfg = settingsTable[settingSubMenu][settingIndex].config;
      switch (settingsTable[settingSubMenu][settingIndex].type) {
        case NUMBER: {
          if ((rightButton.wasPressed() || rightButton.pressedFor(500)) &&
              *static_cast<int16_t *>(data_ptr) < cfg.minmax.max) {
            (*static_cast<int16_t *>(data_ptr))++;
            configChanged = true;
            window.updateSettings(settingIndex);
          }
          if ((leftButton.wasPressed() || leftButton.pressedFor(500)) &&
              *static_cast<int16_t *>(data_ptr) > cfg.minmax.min) {
            (*static_cast<int16_t *>(data_ptr))--;
            configChanged = true;
            window.updateSettings(settingIndex);
          }
          break;
        }
        case TOGGLE: {
          if (rightButton.wasPressed() && !*static_cast<bool *>(data_ptr)) {
            (*static_cast<bool *>(data_ptr)) = true;
            configChanged = true;
            window.updateSettings(settingIndex);
          }
          if (leftButton.wasPressed() && *static_cast<bool *>(data_ptr)) {
            (*static_cast<bool *>(data_ptr)) = false;
            configChanged = true;
            window.updateSettings(settingIndex);
          }
          break;
        }
        case STRING: {
          if (okButton.wasPressed()) {
            memcpy(keyboardString, static_cast<char *>(data_ptr), kMaxPhraseLen);
            keyboardString[kMaxPhraseLen] = '\0';

            window.initKeyboard(keyboardString, cfg.stringLength);
            keyboardActive = true;
          }
          break;
        }
        case BUTTON: {
          if (okButton.wasPressed()) {
            // If the setting is pointing to the bootloader function, we need to display the bootloader screen
            // first
            if (cfg.fun_ptr == Utils::startBootloader) {
              window.Bootloader();
            }
            if (cfg.fun_ptr != nullptr) {
              cfg.fun_ptr();
            }
          }
          break;
        }
        default: {
          break;
        }
      }
    }

    if (downButton.wasPressed() && settingIndex < settingsTableValueCount[settingSubMenu] - 1) {
      settingIndex++;
      window.updateSettings(settingIndex);
    }

    if (upButton.wasPressed() && settingIndex > -1) {
      settingIndex--;
      window.updateSettings(settingIndex);
    }

    if (backButton.wasPressed()) {
      state = MENU;
      if (configChanged) {
        configChanged = false;
        if (systemConfig.config.receiverMode == SINGLE) {
          // Set both link phrases to the same
          link1.setLinkPhrase(systemConfig.config.linkPhrase1, kMaxPhraseLen);
          link2.setLinkPhrase(systemConfig.config.linkPhrase1, kMaxPhraseLen);
        } else {
          // Use two different link phrases
          link1.setLinkPhrase(systemConfig.config.linkPhrase1, kMaxPhraseLen);
          link2.setLinkPhrase(systemConfig.config.linkPhrase2, kMaxPhraseLen);
        }

        link1.setTestingPhrase(systemConfig.config.testingPhrase, kMaxPhraseLen);
        systemConfig.save();
      }
      window.initMenu(menuIndex);
    }
  }
}

void Hmi::update(void *pvParameter) {
  auto *ref = static_cast<Hmi *>(pvParameter);

  ref->window.logo();

  vTaskDelay(2000);

  ref->window.initBar();
  ref->initMenu();

  uint32_t barUpdate = millis();
  bool timeValid = false;

  while (ref->initialized) {
    TickType_t task_last_tick = xTaskGetTickCount();

    ref->fsm();

    if (link1.data.isUpdated()) {
      // ref->window.updateBar(link1.data.ts());
    }

    if (millis() - barUpdate >= 1000) {
      barUpdate = millis();
      const float voltage = static_cast<float>(analogRead(18)) * 0.00059154929F;
      if (!ref->isLogging) {
        ref->flashFreeMemory = Utils::getFlashMemoryUsage();
      }
      if (link2.time.isUpdated()) {
        setTime(link2.time.hour(), link2.time.minute(), link2.time.second(), 0, 0, 0);
        adjustTime(systemConfig.config.timeZoneOffset * 3600L);
        timeValid = true;
      }
      ref->window.updateBar(voltage, static_cast<bool>(digitalRead(21)), ref->isLogging, link2.location.isValid(),
                            timeValid, ref->flashFreeMemory);
    }

    ref->upButton.read();
    ref->downButton.read();
    ref->leftButton.read();
    ref->rightButton.read();
    ref->centerButton.read();

    ref->okButton.read();
    ref->backButton.read();
    vTaskDelayUntil(&task_last_tick, static_cast<TickType_t>(1000) / 50);
  }
}
