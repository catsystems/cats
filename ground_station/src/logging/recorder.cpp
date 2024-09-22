/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#include "recorder.hpp"

bool Recorder::begin() {
  int32_t number = 0;

  if (!fatfs.chdir(directory)) {
    console.warning.print("[REC] Open directory failed");
    console.warning.println(directory);
    fatfs.mkdir(&directory[1]);
    if (!fatfs.chdir(directory)) {
      console.warning.println("[REC] Open directory failed");
      return false;
    }
  }

  do {
    snprintf(fileName, 30, "log_%03ld.csv", number);
    number++;
  } while (fatfs.exists(fileName));

  queue = xQueueCreate(64, sizeof(RecorderElement));
  xTaskCreate(recordTask, "task_recorder", 4096, this, 1, nullptr);
  initialized = true;
  return initialized;
}

void Recorder::createFile() {
  file = fatfs.open(fileName, FILE_WRITE);
  if (!file) {
    console.warning.println("[REC] Open file failed");
    return;
  }
  fileCreated = true;
  file.println(
      "link,ts[deciseconds],state,errors,lat[deg/10000],lon[deg/10000],altitude[m],velocity[m/"
      "s],battery[decivolts],pyro1,pyro2");
}

void Recorder::recordTask(void *pvParameter) {
  auto *ref = static_cast<Recorder *>(pvParameter);
  char line[128];
  uint32_t count = 0;
  RecorderElement element{};
  while (ref->initialized) {
    if (xQueueReceive(ref->queue, &element, portMAX_DELAY) == pdPASS) {
      if (!ref->fileCreated) {
        ref->createFile();
      }
      const auto &data = element.data;
      const auto pyro1_continuity = static_cast<bool>(data.pyro_continuity & 0x01U);
      const auto pyro2_continuity = static_cast<bool>(data.pyro_continuity & 0x02U);
      snprintf(line, 128, "%hu,%d,%d,%d,%d,%d,%d,%d,%d,%hu,%hu", element.source, data.timestamp, data.state,
               data.errors, data.lat, data.lon, data.altitude, data.velocity, data.voltage,
               static_cast<uint8_t>(pyro1_continuity), static_cast<uint8_t>(pyro2_continuity));
      ref->file.println(line);
      count++;

      if (count == 10) {
        count = 0;
        ref->file.sync();
      }
    }
  }
  vTaskDelete(nullptr);
}

uint8_t Recorder::getFileCount() {
  if (!fatfs.chdir(directory)) {
    console.warning.print("[REC] Open directory failed");
    console.warning.println(directory);
    return 0;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-init-variables) something is wrong with fatfs
  File file = fatfs.open("/logs/");

  uint8_t count = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables) something is wrong with fatfs
  File entry = file.openNextFile();
  while (entry) {
    count++;
    entry.close();
    entry = file.openNextFile();
  }
  file.close();
  return count;
}

bool Recorder::getFileNameByIndex(uint8_t index, char *name) const {
  if (!fatfs.chdir(directory)) {
    console.warning.print("[REC] Open directory failed");
    console.warning.println(directory);
    return false;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-init-variables) something is wrong with fatfs
  File file = fatfs.open("/logs/");

  uint8_t count = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables) something is wrong with fatfs
  File entry = file.openNextFile();
  while (entry) {
    if (count == index) {
      entry.getName(name, 30);
      entry.close();
      file.close();
      return true;
    }
    count++;
    entry.close();
    entry = file.openNextFile();
  }
  file.close();
  return false;
}
