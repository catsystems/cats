/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "telemetry/telemetryData.hpp"
#include "utils.hpp"

struct RecorderElement {
  packedRXMessage data;
  uint8_t source;  // Link 1 or Link 2
};

class Recorder {
 public:
  explicit Recorder(const char* directory) : directory(directory) {}
  bool begin();

  void enable() { enabled = true; }

  void disable() { enabled = false; }

  void record(packedRXMessage* data, uint8_t link_source) {
    if (enabled) {
      RecorderElement rec_elem = {*data, link_source};
      xQueueSend(queue, &rec_elem, 0);
    }
  }

  uint8_t getFileCount();

  bool getFileNameByIndex(uint8_t index, char* name) const;

  const char* getDirectory() const { return directory; }

 private:
  bool initialized = false;
  bool enabled = false;
  bool fileCreated = false;

  const char* directory;

  char fileName[30] = {};

  QueueHandle_t queue{nullptr};
  File file{};

  void createFile();

  static void recordTask(void* pvParameter);
};
