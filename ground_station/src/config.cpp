
/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.hpp"

#include "console.hpp"
#include "systemParser.hpp"

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
SystemParser systemParser{};
Config systemConfig{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void Config::save() {
  systemParser.setTestingPhrase(config.testingPhrase);
  systemParser.setLinkPhrase1(config.linkPhrase1);
  systemParser.setLinkPhrase2(config.linkPhrase2);
  systemParser.setTelemetryMode(static_cast<bool>(config.receiverMode));
  systemParser.setNeverStopLoggingFlag(config.neverStopLogging);
  systemParser.setUnitSystem(config.unitSystem);
  systemParser.setTimeZone(config.timeZoneOffset);
  systemParser.setMagCalib(config.mag_calib);
  systemParser.saveFile("/config.json");
}

void Config::load() {
  systemParser.loadFile("/config.json");
  bool mode{false};
  bool stop{false};
  if (!systemParser.getTestingPhrase(config.testingPhrase)) {
    strncpy(config.testingPhrase, "", 1);
    console.warning.println("Testing phrase loading failed");
  }

  if (!systemParser.getLinkPhrase1(config.linkPhrase1)) {
    strncpy(config.linkPhrase1, "", 1);
    console.warning.println("Link phrase 1 loading failed");
  }

  if (!systemParser.getLinkPhrase2(config.linkPhrase2)) {
    strncpy(config.linkPhrase2, "", 1);
    console.warning.println("Link phrase 2 loading failed");
  }

  if (!systemParser.getTelemetryMode(mode)) {
    mode = false;
    console.warning.println("Telemetry mode loading failed");
  }

  if (!systemParser.getNeverStopLoggingFlag(stop)) {
    config.neverStopLogging = false;
    console.warning.println("Logging flag loading failed");
  }

  if (!systemParser.getTimeZone(config.timeZoneOffset)) {
    config.timeZoneOffset = 0;
    console.warning.println("Timezone loading failed");
  }

  if (!systemParser.getMagCalib(config.mag_calib)) {
    config.mag_calib.mag_offset_x = 0;
    config.mag_calib.mag_offset_y = 0;
    config.mag_calib.mag_offset_z = 0;

    config.mag_calib.mag_scale_x = 1000;
    config.mag_calib.mag_scale_y = 1000;
    config.mag_calib.mag_scale_z = 1000;
    console.warning.println("Mag Calibration loading failed");
  }
  if (!systemParser.getUnitSystem(config.unitSystem)) {
    config.unitSystem = UnitSystem::kMetric;
  }

  config.neverStopLogging = stop;
  config.receiverMode = static_cast<ReceiverTelemetryMode_e>(mode);
}
