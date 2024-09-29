/// Copyright (C) 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "console.hpp"
#include "utils.hpp"

#include <cstring>
#include <string>
#include <vector>

class FlightStatistics {
 public:
  FlightStatistics() = default;

  void parseFlightLog(const char* directory, const char* name, uint8_t linkSource) {
    if (!fatfs.chdir(directory)) {
      console.warning.print("[REC] Open directory failed");
      console.warning.println(directory);
      return;
    }

    if ((linkSource < 1U) || (linkSource > 2U)) {
      console.warning.println("Invalid link source");
      return;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-init-variables) something is wrong with fatfs
    File file = fatfs.open(name);

    uint16_t count = 0;
    char line[128];
    file.open(&fatfs, name, FILE_READ);

    while (file.available()) {
      size_t size = file.readBytesUntil('\n', line, 128);
      if (size > 0 && count > 0) {
        line[size] = '\0';

        std::string tokens[10] = {};
        if (!parseLine(line, tokens)) {
          console.warning.println("Invalid line format");
          continue;
        }

        if (std::stoi(tokens[0]) == linkSource) {
          data.timestamp = std::stoi(tokens[1]);
          data.state = static_cast<flight_state_e>(std::stoi(tokens[2]));
          data.errors = std::stoi(tokens[3]);
          data.lat = std::stoi(tokens[4]);
          data.lon = std::stoi(tokens[5]);
          data.altitude = std::stoi(tokens[6]);
          data.velocity = static_cast<int16_t>(std::stoi(tokens[7]));
          data.voltage = std::stoi(tokens[8]);

          if (stateChanged(data.state)) {
            if (data.state == flight_state_e::LIFTOFF) {
              liftoffTimestamp = data.timestamp;
            }
            if (data.state == flight_state_e::APOGEE) {
              timeToApogeeSeconds = static_cast<float>(data.timestamp - liftoffTimestamp) / 10.0F;
            }
            if (data.state == flight_state_e::MAIN) {
              timeToMainSeconds = static_cast<float>(data.timestamp - liftoffTimestamp) / 10.0F;
              mainDeployAltitude = data.altitude;
            }
          }
          if (data.altitude > maxAltitude) {
            maxAltitude = data.altitude;
          }
          if (data.velocity > maxVelocity) {
            maxVelocity = data.velocity;
          }

          flightTimeRaw = data.timestamp - liftoffTimestamp;

          lastLatitude = data.lat;
          lastLongitude = data.lon;
        }
      }
      count++;
    }
    flightTimeSeconds = static_cast<float>(flightTimeRaw) / 10.0F;
    lastReportedAltitude = data.altitude;
    file.close();
  }

  int32_t getMaxAltitude() const { return maxAltitude; }

  float getTimeToApogee() const { return timeToApogeeSeconds; }

  int32_t getMaxVelocity() const { return maxVelocity; }

  float getLastLatitude() const { return static_cast<float>(lastLatitude) / 10000.0F; }

  float getLastLongitude() const { return static_cast<float>(lastLongitude) / 10000.0F; }

  float getFlightTime() const { return flightTimeSeconds; }

  float getDrogueDescentRate() const {
    return static_cast<float>(maxAltitude - mainDeployAltitude) / (timeToMainSeconds - timeToApogeeSeconds);
  }

  float getMainDescentRate() const {
    return static_cast<float>(mainDeployAltitude - lastReportedAltitude) / (flightTimeSeconds - timeToMainSeconds);
  }

 private:
  enum class flight_state_e : uint8_t {
    NONE = 0,
    READY = 2,
    LIFTOFF = 3,
    BURNOUT = 4,
    APOGEE = 5,
    MAIN = 6,
    TOUCHDOWN = 7,
  };

  struct UnpackedData {
    flight_state_e state;
    uint16_t timestamp;
    uint8_t errors;
    int32_t lat;
    int32_t lon;
    int32_t altitude;
    int16_t velocity;
    uint16_t voltage;
    uint8_t pyro_continuity;
    bool testing_mode;
  };

  static bool parseLine(const char* line, std::string tokens[10]) {
    const char* start = line;
    const char* end = strchr(start, ',');
    int tokenIndex = 0;

    while (end != nullptr && tokenIndex < 10) {
      tokens[tokenIndex] = std::string(start, end - start);
      tokenIndex++;
      start = end + 1;
      end = strchr(start, ',');
    }

    return (tokenIndex == 10);
  }

  bool stateChanged(flight_state_e state) {
    if (state != previousState) {
      previousState = state;
      return true;
    }
    return false;
  }

  UnpackedData data{};

  flight_state_e previousState = flight_state_e::NONE;

  uint32_t liftoffTimestamp = 0;
  uint32_t flightTimeRaw = 0;
  float timeToApogeeSeconds = 0.0F;
  float timeToMainSeconds = 0.0F;
  float flightTimeSeconds = 0.0F;
  int32_t maxAltitude = 0;
  int32_t lastReportedAltitude = 0;
  int32_t maxVelocity = 0;
  int32_t lastLatitude = 0;
  int32_t lastLongitude = 0;
  int32_t mainDeployAltitude = 0;
};
