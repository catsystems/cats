#pragma once

#include "config.hpp"
#include "utils.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))

enum settings_type_e {
  STRING = 0,
  TOGGLE = 1,
  NUMBER = 2,
  BUTTON = 3,
};

struct settings_min_max_t {
  int16_t min;
  int16_t max;
};

union settings_limits_u {
  uint32_t stringLength;
  uint32_t lookup;
  settings_min_max_t minmax;
  void (*fun_ptr)();
};

struct device_settings_t {
  const char* name;
  const char* description1;
  const char* description2;
  settings_type_e type;
  settings_limits_u config;

  void* dataPtr;
};

enum lookup_table_index_e {
  TABLE_MODE = 0,
  TABLE_UNIT,
  TABLE_LOGGING,
};

const char* const mode_map[2] = {
    "SINGLE",
    "DUAL",
};

const char* const unit_map[2] = {
    "METRIC",
    "RETARDED",
};

const char* const logging_map[2] = {
    "DOWN",
    "NEVER",
};

struct lookup_table_entry_t {
  const char* const* values;
  const uint8_t value_count;
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOOKUP_TABLE_ENTRY(name) \
  { name, ARRAYLEN(name) }

const lookup_table_entry_t lookup_tables[] = {
    LOOKUP_TABLE_ENTRY(mode_map),
    LOOKUP_TABLE_ENTRY(unit_map),
    LOOKUP_TABLE_ENTRY(logging_map),
};

enum {
  kSettingPages = 2,
};

const char* const settingPageName[kSettingPages] = {"General", "Telemetry"};

// NOLINTNEXTLINE(cppcoreguidelines-interfaces-global-init)
const device_settings_t settingsTable[][4] = {
    {
        {"Time Zone",
         "Set the time offset",
         "",
         NUMBER,
         {.minmax = {.min = -12, .max = 12}},
         &systemConfig.config.timeZoneOffset},
        {"Stop Logging",
         "Down: Stop the log at touchdown",
         "Never: Never stop logging after liftoff",
         TOGGLE,
         {.lookup = TABLE_LOGGING},
         &systemConfig.config.neverStopLogging},
        {
            "Version",
            "Firmware Version: " FIRMWARE_VERSION,
            "",
            BUTTON,
            {.fun_ptr = nullptr},
            nullptr,
        },
        {
            "Start Bootloader",
            "Press A to start the bootloader",
            "Make sure you are connected to a computer",
            BUTTON,
            {.fun_ptr = Utils::startBootloader},
            nullptr,
        },
    },
    {
        {"Mode",
         "Single: Use both receiver to track one rocket",
         "Dual: Use both receivers individually",
         TOGGLE,
         {.lookup = TABLE_MODE},
         &systemConfig.config.receiverMode},
        {"Link Phrase 1",
         "Single Mode: Set phrase for both receivers",
         "Dual Mode: Set phrase for the left receiver",
         STRING,
         {.stringLength = kMaxPhraseLen},
         systemConfig.config.linkPhrase1},
        {"Link Phrase 2",
         "Single Mode: No functionality",
         "Dual Mode: Set phrase for the right receiver",
         STRING,
         {.stringLength = kMaxPhraseLen},
         systemConfig.config.linkPhrase2},
        {"Testing Phrase",
         "Set the phrase for the testing mode",
         "",
         STRING,
         {.stringLength = kMaxPhraseLen},
         systemConfig.config.testingPhrase},
    },
};

const uint16_t settingsTableValueCount[kSettingPages] = {4, 4};
