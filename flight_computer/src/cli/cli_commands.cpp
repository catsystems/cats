/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later
///
/// Additional notice:
/// This file was adapted from Betaflight (https://github.com/betaflight/betaflight),
/// released under GNU General Public License v3.0.

#include "cli/cli_commands.hpp"

#ifdef CATS_DEV
#include "tasks/task_simulator.hpp"
#endif

#include "cli/cli.hpp"
#include "config/cats_config.hpp"
#include "config/globals.hpp"
#include "drivers/w25q.hpp"
#include "flash/lfs_custom.hpp"
#include "flash/reader.hpp"
#include "main.hpp"
#include "tasks/task_state_est.hpp"
#include "util/actions.hpp"
#include "util/battery.hpp"
#include "util/enum_str_maps.hpp"
#include "util/log.h"

#include <strings.h>

#include <cstdlib>
#include <cstring>

/** CLI command function declarations **/
static void cli_cmd_help(const char *cmd_name, char *args);

static void cli_cmd_reboot(const char *cmd_name, char *args);
static void cli_cmd_bl(const char *cmd_name, char *args);
static void cli_cmd_save(const char *cmd_name, char *args);

static void cli_cmd_get(const char *cmd_name, char *args);
static void cli_cmd_set(const char *cmd_name, char *args);
static void cli_cmd_config(const char *cmd_name, char *args);
static void cli_cmd_defaults(const char *cmd_name, char *args);
static void cli_cmd_dump(const char *cmd_name, char *args);

static void cli_cmd_status(const char *cmd_name, char *args);
static void cli_cmd_version(const char *cmd_name, char *args);

static void cli_cmd_log_enable(const char *cmd_name, char *args);

static void cli_cmd_ls(const char *cmd_name, char *args);
static void cli_cmd_cd(const char *cmd_name, char *args);
static void cli_cmd_rm(const char *cmd_name, char *args);
static void cli_cmd_rec_info(const char *cmd_name, char *args);

static void cli_cmd_dump_flight(const char *cmd_name, char *args);
static void cli_cmd_parse_flight(const char *cmd_name, char *args);
static void cli_cmd_print_stats(const char *cmd_name, char *args);

static void cli_cmd_lfs_format(const char *cmd_name, char *args);
static void cli_cmd_erase_flash(const char *cmd_name, char *args);

static void cli_cmd_flash_write(const char *cmd_name, char *args);
static void cli_cmd_flash_stop(const char *cmd_name, char *args);
static void cli_cmd_flash_test(const char *cmd_name, char *args);

#ifdef CATS_DEV
static void cli_cmd_start_simulation(const char *cmd_name, char *args);
#endif

/* List of CLI commands; should be sorted in alphabetical order. */
const clicmd_t cmd_table[] = {
    CLI_COMMAND_DEF("bl", "reset into bootloader", nullptr, cli_cmd_bl),
    CLI_COMMAND_DEF("cd", "change current working directory", nullptr, cli_cmd_cd),
    CLI_COMMAND_DEF("config", "print the flight config", nullptr, cli_cmd_config),
    CLI_COMMAND_DEF("defaults", "reset to defaults and reboot", nullptr, cli_cmd_defaults),
    CLI_COMMAND_DEF("dump", "Dump configuration", nullptr, cli_cmd_dump),
    CLI_COMMAND_DEF("flash_erase", "erase the flash", nullptr, cli_cmd_erase_flash),
    CLI_COMMAND_DEF("flash_test", "test the flash", nullptr, cli_cmd_flash_test),
    CLI_COMMAND_DEF("flash_start_write", "set recorder state to REC_WRITE_TO_FLASH", nullptr, cli_cmd_flash_write),
    CLI_COMMAND_DEF("flash_stop_write", "set recorder state to REC_FILL_QUEUE", nullptr, cli_cmd_flash_stop),
    CLI_COMMAND_DEF("flight_dump", "print a specific flight", "<flight_number>", cli_cmd_dump_flight),
    CLI_COMMAND_DEF("flight_parse", "print a specific flight", "<flight_number>", cli_cmd_parse_flight),
    CLI_COMMAND_DEF("get", "get variable value", "[cmd_name]", cli_cmd_get),
    CLI_COMMAND_DEF("help", "display command help", "[search string]", cli_cmd_help),
    CLI_COMMAND_DEF("lfs_format", "reformat lfs", nullptr, cli_cmd_lfs_format),
    CLI_COMMAND_DEF("log_enable", "enable the logging output", nullptr, cli_cmd_log_enable),
    CLI_COMMAND_DEF("ls", "list all files in current working directory", nullptr, cli_cmd_ls),
    CLI_COMMAND_DEF("reboot", "reboot without saving", nullptr, cli_cmd_reboot),
    CLI_COMMAND_DEF("rec_info", "get the info about flash", nullptr, cli_cmd_rec_info),
    CLI_COMMAND_DEF("rm", "remove a file", "<file_name>", cli_cmd_rm),
    CLI_COMMAND_DEF("save", "save configuration", nullptr, cli_cmd_save),
    CLI_COMMAND_DEF("set", "change setting", "[<cmd_name>=<value>]", cli_cmd_set),
#ifdef CATS_DEV
    CLI_COMMAND_DEF("sim", "start a simulation flight", "<sim_tag>", cli_cmd_start_simulation),
#endif
    CLI_COMMAND_DEF("stats", "print flight stats", "<flight_number>", cli_cmd_print_stats),
    CLI_COMMAND_DEF("status", "show status", nullptr, cli_cmd_status),
    CLI_COMMAND_DEF("version", "show version", nullptr, cli_cmd_version),
};

const size_t NUM_CLI_COMMANDS = sizeof cmd_table / sizeof cmd_table[0];

static const char *const emptyName = "-";

/** Helper function declarations **/

static void print_control_config();

static void print_action_config();

static void print_timer_config();

static void cli_set_var(const cli_value_t *var, uint32_t value);

static void fill_buf(uint8_t *buf, size_t buf_sz);

/** CLI command function definitions **/

static void cli_cmd_help(const char *cmd_name, char *args) {
  bool any_matches = false;

  // NOLINTNEXTLINE(modernize-loop-convert)
  for (uint32_t i = 0; i < ARRAYLEN(cmd_table); i++) {
    bool print_entry = false;
    if (is_empty(args)) {
      print_entry = true;
    } else {
      if ((strstr(cmd_table[i].name, args) != nullptr) || (strstr(cmd_table[i].description, args) != nullptr)) {
        print_entry = true;
      }
    }

    if (print_entry) {
      any_matches = true;
      cli_print(cmd_table[i].name);
      if (cmd_table[i].description != nullptr) {
        cli_printf(" - %s", cmd_table[i].description);
      }
      if (cmd_table[i].args != nullptr) {
        cli_printf("\r\n\t%s", cmd_table[i].args);
      }
      cli_print_linefeed();
    }
  }
  if (!is_empty(args) && !any_matches) {
    cli_print_error_linef(cmd_name, "NO MATCHES FOR '%s'", args);
  }
}

static void cli_cmd_reboot(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) { NVIC_SystemReset(); }

static void cli_cmd_bl(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, BOOTLOADER_MAGIC_PATTERN);
  __disable_irq();
  NVIC_SystemReset();
}

static void cli_cmd_save(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  if (!cc_save()) {
    cli_print_line("Saving unsuccessful, trying force save...");
    if (!cc_format_save()) {
      cli_print_line("Force save failed!");
      return;
    }
  }
  cli_print_line("Successfully written to flash");
}

static void cli_cmd_get(const char *cmd_name, char *args) {
  const cli_value_t *val{nullptr};
  int matched_commands = 0;

  for (uint32_t i = 0; i < value_table_entry_count; i++) {
    if (strstr(value_table[i].name, args) != nullptr) {
      val = &value_table[i];
      if (matched_commands > 0) {
        cli_print_linefeed();
      }
      cli_printf("%s = ", value_table[i].name);
      cli_print_var(cmd_name, &global_cats_config, val, false);
      cli_print_linefeed();
      cli_print_var_range(val);
      // cliPrintVarDefault(cmd_name, val);

      matched_commands++;
    }
  }

  if (matched_commands == 0) {
    cli_print_error_linef(cmd_name, "INVALID NAME");
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void cli_cmd_set(const char *cmd_name, char *args) {
  const uint32_t arg_len = strlen(args);
  char *eqptr = nullptr;

  if (arg_len == 0 || (arg_len == 1 && args[0] == '*')) {
    cli_print_line("Current settings: ");

    // when arg_len is 1 (when * is passed as argument), it will print min/max values as well, for gui
    print_cats_config(cmd_name, &global_cats_config, arg_len > 0);

  } else if ((eqptr = strstr(args, "=")) != nullptr) {  // NOLINT(bugprone-assignment-in-if-condition)
    // has equals

    const uint8_t variable_name_length = get_word_length(args, eqptr);

    // skip the '=' and any ' ' characters
    eqptr++;
    eqptr = skip_space(eqptr);

    const uint16_t index = cli_get_setting_index(args, variable_name_length);
    if (index >= value_table_entry_count) {
      cli_print_error_linef(cmd_name, "INVALID NAME");
      return;
    }
    const cli_value_t *val = &value_table[index];

    bool value_changed = false;

    switch (val->type & VALUE_MODE_MASK) {
      case MODE_DIRECT: {
        if ((val->type & VALUE_TYPE_MASK) == VAR_UINT32) {
          const uint32_t value = strtoul(eqptr, nullptr, 10);

          if (value <= val->config.u32_max) {
            cli_set_var(val, value);
            value_changed = true;
          }
        } else {
          const int value = atoi(eqptr);

          int min = 0;
          int max = 0;
          get_min_max(val, &min, &max);

          if (value >= min && value <= max) {
            cli_set_var(val, value);
            value_changed = true;
          }
        }
      }

      break;
      case MODE_LOOKUP:
      case MODE_BITSET: {
        int tableIndex = 0;
        if ((val->type & VALUE_MODE_MASK) == MODE_BITSET) {
          tableIndex = TABLE_EVENTS;
        } else {
          tableIndex = val->config.lookup.table_index;
        }
        const EnumToStrMap &tableEntry = lookup_tables[tableIndex];
        bool matched = false;
        for (uint32_t tableValueIndex = 0; tableValueIndex < tableEntry.size() && !matched; tableValueIndex++) {
          matched = tableEntry[tableValueIndex] && strcasecmp(tableEntry[tableValueIndex], eqptr) == 0;

          if (matched) {
            cli_set_var(val, tableValueIndex);
            value_changed = true;
          }
        }
      } break;
      case MODE_ARRAY: {
        const uint8_t array_length = val->config.array.length;
        char *valPtr = eqptr;

        int i = 0;
        while (i < array_length && valPtr != nullptr) {
          // skip spaces
          valPtr = skip_space(valPtr);

          const void *var_ptr = get_cats_config_member_ptr(&global_cats_config, val);

          // process substring starting at valPtr
          // note: no need to copy substrings for atoi()
          //       it stops at the first character that cannot be converted...
          // NOLINTBEGIN(google-readability-casting)
          switch (val->type & VALUE_TYPE_MASK) {
            default:
            case VAR_UINT8: {
              // fetch data pointer
              uint8_t *data = (uint8_t *)var_ptr + i;
              // store value
              *data = static_cast<uint8_t>(atoi(static_cast<const char *>(valPtr)));
            }

            break;
            case VAR_INT8: {
              // fetch data pointer
              int8_t *data = (int8_t *)var_ptr + i;
              // store value
              *data = static_cast<int8_t>(atoi(static_cast<const char *>(valPtr)));
            }

            break;
            case VAR_UINT16: {
              // fetch data pointer
              uint16_t *data = (uint16_t *)var_ptr + i;
              // store value
              *data = static_cast<uint16_t>(atoi(static_cast<const char *>(valPtr)));
            }

            break;
            case VAR_INT16: {
              // fetch data pointer
              int16_t *data = (int16_t *)var_ptr + i;
              // store value
              *data = static_cast<int16_t>(atoi(static_cast<const char *>(valPtr)));
            }

            break;
            case VAR_UINT32: {
              // fetch data pointer
              uint32_t *data = (uint32_t *)var_ptr + i;
              // store value
              *data = static_cast<uint32_t>(strtoul(static_cast<const char *>(valPtr), nullptr, 10));
            }

            break;
          }
          // NOLINTEND(google-readability-casting)

          // find next comma (or end of string)
          valPtr = strchr(valPtr, ',') + 1;

          i++;
        }
      }
        // mark as changed
        value_changed = true;

        break;
      case MODE_STRING: {
        char *val_ptr = eqptr;
        val_ptr = skip_space(val_ptr);
        char *var_ptr = static_cast<char *>(get_cats_config_member_ptr(&global_cats_config, val));
        const unsigned int len = strlen(val_ptr);
        const uint8_t min = val->config.string.min_length;
        const uint8_t max = val->config.string.max_length;
        const bool updatable = ((val->config.string.flags & STRING_FLAGS_WRITEONCE) == 0 || strlen(var_ptr) == 0 ||
                                strncmp(val_ptr, var_ptr, len) == 0);

        if (updatable && len > 0 && len <= max) {
          memset(var_ptr, 0, max);
          if ((len >= min) && (strncmp(val_ptr, emptyName, len) != 0)) {
            strncpy(var_ptr, val_ptr, len);
          }
          value_changed = true;
        } else {
          cli_print_error_linef(cmd_name, "STRING MUST BE %hu..%hu CHARACTERS OR '-' FOR EMPTY", min, max);
        }
      } break;
    }

    if (value_changed) {
      cli_printf("%s set to ", val->name);
      cli_print_var(cmd_name, &global_cats_config, val, false);
      if (val->cb != nullptr) {
        val->cb(val);
      }
      global_cats_config.is_set_by_user = true;
    } else {
      cli_print_error_linef(cmd_name, "INVALID VALUE");
      cli_print_var_range(val);
    }

    return;
  } else {
    // no equals, check for matching variables.
    cli_cmd_get(cmd_name, args);
  }
}

static void cli_cmd_config(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  print_control_config();
  print_action_config();
  print_timer_config();
}

static void cli_cmd_defaults(const char *cmd_name [[maybe_unused]], char *args) {
  bool use_default_outputs = true;
  if (strcmp(args, "--no-outputs") == 0) {
    use_default_outputs = false;
  }
  cc_defaults(use_default_outputs, true);
  cli_print_linef("Reset to default values%s", use_default_outputs ? "" : " [no outputs]");
}

static void cli_cmd_dump(const char *cmd_name, char *args) {
  const uint32_t len = strlen(args);
  cli_print_linef("#Configuration dump");

  print_cats_config(cmd_name, &global_cats_config, len > 0);

  cli_printf("#End of configuration dump");
}

static void cli_cmd_status(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  cli_printf("System time: %lu ticks\n", osKernelGetTickCount());
  auto new_enum = static_cast<flight_fsm_e>(osEventFlagsWait(fsm_flag_id, 0xFF, osFlagsNoClear, 0));
  if (new_enum > TOUCHDOWN || new_enum < CALIBRATING) {
    new_enum = INVALID;
  }
  cli_printf("State:       %s\n", GetStr(new_enum, fsm_map));
  cli_printf("Voltage:     %.2fV\n", static_cast<double>(battery_voltage()));
  cli_printf("h: %.2fm, v: %.2fm/s, a: %.2fm/s^2",
             static_cast<double>(task::global_state_estimation->GetEstimationOutput().height),
             static_cast<double>(task::global_state_estimation->GetEstimationOutput().velocity),
             static_cast<double>(task::global_state_estimation->GetEstimationOutput().acceleration));

#ifdef CATS_DEV
  if (strcmp(args, "--heap") == 0) {
    HeapStats_t heap_stats = {};
    vPortGetHeapStats(&heap_stats);
    cli_print_linef("\nHeap stats");
    cli_print_linef("  Available heap space: %u B", heap_stats.xAvailableHeapSpaceInBytes);
    cli_print_linef("  Largest free block size: %u B", heap_stats.xSizeOfLargestFreeBlockInBytes);
    cli_print_linef("  Smallest free block size: %u B", heap_stats.xSizeOfSmallestFreeBlockInBytes);
    cli_print_linef("  Number of free blocks: %u", heap_stats.xNumberOfFreeBlocks);
    cli_print_linef("  Minimum free bytes remaining during program lifetime: %u B",
                    heap_stats.xMinimumEverFreeBytesRemaining);
    cli_print_linef("  Number of successful allocations: %u", heap_stats.xNumberOfSuccessfulAllocations);
    cli_print_linef("  Number of successful frees: %u", heap_stats.xNumberOfSuccessfulFrees);
  }
#endif
}

static void cli_cmd_version(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  cli_printf("Board: %s\n", board_name);
  cli_printf("Code version: %s\n", code_version);
  cli_printf("Telemetry Code version: %s\n", telemetry_code_version);
}

static void cli_cmd_log_enable(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) { log_enable(); }

static void cli_cmd_ls(const char *cmd_name [[maybe_unused]], char *args) {
  if (args == nullptr) {
    lfs_ls(cwd);
  } else {
    const uint32_t full_path_len = strlen(cwd) + 1 + strlen(args);
    if (full_path_len > LFS_NAME_MAX) {
      cli_print_line("File path too long!");
      return;
    }
    char *full_path = static_cast<char *>(pvPortMalloc(full_path_len + 1));
    strcpy(full_path, cwd);
    strcat(full_path, "/");
    strcat(full_path, args);
    lfs_ls(full_path);
    vPortFree(full_path);
  }
}

static void cli_cmd_cd(const char *cmd_name [[maybe_unused]], char *args) {
  /* TODO - check if a directory actually exists */
  if (args == nullptr || strcmp(args, "/") == 0) {
    strncpy(cwd, "/", sizeof(cwd));
  } else if (strcmp(args, "..") == 0) {
    /* Return one lvl back by clearing everything after the last path separator. */
    const char *last_path_sep = strrchr(cwd, '/');
    if (last_path_sep != nullptr) {
      const uint32_t last_path_sep_loc = last_path_sep - cwd;
      cwd[last_path_sep_loc + 1] = '\0';
    }
  } else if (strcmp(args, ".") != 0) {
    if (args[0] == '/') {
      /* absolute path */
      const uint32_t full_path_len = strlen(args);
      if (full_path_len > LFS_NAME_MAX) {
        cli_print_line("Path too long!");
        return;
      }
      char *tmp_path = static_cast<char *>(pvPortMalloc(full_path_len + 1));
      strcpy(tmp_path, args);
      if (lfs_obj_type(tmp_path) != LFS_TYPE_DIR) {
        cli_print_linef("Cannot go to '%s': not a directory!", tmp_path);
        vPortFree(tmp_path);
        return;
      }
      strncpy(cwd, args, sizeof(cwd));
      vPortFree(tmp_path);
    } else {
      /* relative path */
      const uint32_t full_path_len = strlen(cwd) + 1 + strlen(args);
      if (full_path_len > LFS_NAME_MAX) {
        cli_print_line("Path too long!");
        return;
      }
      char *tmp_path = static_cast<char *>(pvPortMalloc(full_path_len + 1));
      strcpy(tmp_path, args);
      if (lfs_obj_type(tmp_path) != LFS_TYPE_DIR) {
        cli_print_linef("Cannot go to '%s': not a directory!", tmp_path);
        vPortFree(tmp_path);
        return;
      }
      strncat(cwd, args, sizeof(cwd) - strlen(cwd) - 1);
      vPortFree(tmp_path);
    }
  }
}

static void cli_cmd_rm(const char *cmd_name [[maybe_unused]], char *args) {
  if (args != nullptr) {
    /* +1 for the path separator (/) */
    const uint32_t full_path_len = strlen(cwd) + 1 + strlen(args);
    if (full_path_len > LFS_NAME_MAX) {
      cli_print_line("File path too long!");
      return;
    }
    /* +1 for the null terminator */
    char *full_path = static_cast<char *>(pvPortMalloc(full_path_len + 1));
    strcpy(full_path, cwd);
    strcat(full_path, "/");
    strcat(full_path, args);

    if (lfs_obj_type(full_path) != LFS_TYPE_REG) {
      cli_print_linef("Cannot remove '%s': not a file!", full_path);
      vPortFree(full_path);
      return;
    }

    const int32_t rm_err = lfs_remove(&lfs, full_path);
    if (rm_err < 0) {
      cli_print_linef("Removal of file '%s' failed with %ld", full_path, rm_err);
    }
    cli_printf("File '%s' removed!", args);
    vPortFree(full_path);
  } else {
    cli_print_line("Argument not provided!");
  }
}

static void cli_cmd_rec_info(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  const lfs_ssize_t curr_sz_blocks = lfs_fs_size(&lfs);
  const int32_t num_flights = lfs_cnt("/flights", LFS_TYPE_REG);
  const int32_t num_stats = lfs_cnt("/stats", LFS_TYPE_REG);

  if ((curr_sz_blocks < 0) || (num_flights < 0) || (num_stats < 0)) {
    cli_print_line("Error while accessing recorder info.");
    return;
  }

  const lfs_size_t block_size_kb = get_lfs_cfg()->block_size / 1024;
  const lfs_size_t curr_sz_kb = curr_sz_blocks * block_size_kb;
  const lfs_size_t total_sz_kb = block_size_kb * get_lfs_cfg()->block_count;
  const double percentage_used =
      static_cast<double>(curr_sz_kb) / static_cast<double>(static_cast<lfs_ssize_t>(total_sz_kb) * 100);
  cli_print_linef("Space:\n  Total: %lu KB\n   Used: %lu KB (%.2f%%)\n   Free: %lu KB (%.2f%%)", total_sz_kb,
                  curr_sz_kb, percentage_used, total_sz_kb - curr_sz_kb, 100 - percentage_used);

  cli_print_linef("Number of flight logs: %ld", num_flights);
  cli_print_linef("Number of stats logs: %ld", num_stats);
}

/**
 * Parse the log index argument string and return it as a number.
 *
 * The function supports tail indexing: -1, -2, -3..., where -1 is the last log, -2 the one before it, etc.
 *
 *
 * @param log_idx_arg
 * @return
 */
static int32_t get_flight_idx(const char *log_idx_arg) {
  if (log_idx_arg == nullptr) {
    cli_print_line("\nArgument not provided!");
    return -1;
  }

  char *endptr = nullptr;
  uint32_t flight_idx = strtoul(log_idx_arg, &endptr, 10);

  if (log_idx_arg == endptr) {
    cli_print_linef("\nInvalid argument: %s.", log_idx_arg);
    return -1;
  }

  /* Check for tail indexing */
  if (flight_idx < 0) {
    /* Convert to "normal" index */
    flight_idx = flight_counter + 1 + flight_idx;
  }

  if (flight_idx <= 0) {
    cli_print_linef("\nInvalid flight: %s.", log_idx_arg);
    return -1;
  }

  if (flight_idx > flight_counter) {
    cli_print_linef("\nFlight %lu doesn't exist", flight_idx);
    cli_print_linef("Number of recorded flights: %lu", flight_counter);
    return -1;
  }

  return static_cast<int32_t>(flight_idx);
}

static void cli_cmd_dump_flight(const char *cmd_name [[maybe_unused]], char *args) {
  const int32_t flight_idx_or_err = get_flight_idx(args);

  if (flight_idx_or_err > 0) {
    cli_print_linefeed();
    reader::dump_recording(flight_idx_or_err);
  }
}

/* flight_parse <flight_idx> [--filter <RECORDER TYPE>...] */
static void cli_cmd_parse_flight(const char *cmd_name [[maybe_unused]], char *args) {
  char *ptr = strtok(args, " ");

  const int32_t flight_idx_or_err = get_flight_idx(ptr);
  auto filter_mask = static_cast<rec_entry_type_e>(0);

  if (flight_idx_or_err < 0) {
    return;
  }

  /* Read filter command */
  ptr = strtok(nullptr, " ");
  if (ptr != nullptr) {
    if (strcmp(ptr, "--filter") == 0) {
      /*Read filter types */
      while (ptr != nullptr) {
        if (strcmp(ptr, "IMU") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | IMU);
        }
        if (strcmp(ptr, "BARO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | BARO);
        }
        if (strcmp(ptr, "FLIGHT_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | FLIGHT_INFO);
        }
        if (strcmp(ptr, "ORIENTATION_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | ORIENTATION_INFO);
        }
        if (strcmp(ptr, "FILTERED_DATA_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | FILTERED_DATA_INFO);
        }
        if (strcmp(ptr, "FLIGHT_STATE") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | FLIGHT_STATE);
        }
        if (strcmp(ptr, "EVENT_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | EVENT_INFO);
        }
        if (strcmp(ptr, "ERROR_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | ERROR_INFO);
        }
        if (strcmp(ptr, "GNSS_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | GNSS_INFO);
        }
        if (strcmp(ptr, "VOLTAGE_INFO") == 0) {
          filter_mask = static_cast<rec_entry_type_e>(filter_mask | VOLTAGE_INFO);
        }
        ptr = strtok(nullptr, " ");
      }
    } else {
      cli_print_linef("\nBad option: %s!", ptr);
    }
  } else {
    filter_mask = static_cast<rec_entry_type_e>(UINT32_MAX);
  }

  reader::parse_recording(flight_idx_or_err, filter_mask);
}

static void cli_cmd_print_stats(const char *cmd_name [[maybe_unused]], char *args) {
  const int32_t flight_idx_or_err = get_flight_idx(args);

  if (flight_idx_or_err > 0) {
    cli_print_linefeed();
    reader::print_stats_and_cfg(flight_idx_or_err);
  }
}

static void cli_cmd_lfs_format(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  cli_print_line("\nTrying LFS format");
  lfs_format(&lfs, get_lfs_cfg());
  const int err = lfs_mount(&lfs, get_lfs_cfg());
  if (err != 0) {
    cli_print_linef("LFS mounting failed with error %d!", err);
  } else {
    cli_print_line("Mounting successful!");
    flight_counter = 0;
    /* create the flights directory */
    lfs_mkdir(&lfs, "flights");
    lfs_mkdir(&lfs, "stats");
    lfs_mkdir(&lfs, "configs");

    strncpy(cwd, "/", sizeof(cwd));
  }
}

static void cli_cmd_erase_flash(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  cli_print_line("\nErasing the flash, this might take a while...");
  w25q_chip_erase();
  cli_print_line("Flash erased!");
  cli_print_line("Mounting LFS");

  const int err = lfs_mount(&lfs, get_lfs_cfg());
  if (err == 0) {
    cli_print_line("LFS mounted successfully!");
  } else {
    cli_print_linef("LFS mounting failed with error %d!", err);
    cli_print_line("Trying LFS format");
    lfs_format(&lfs, get_lfs_cfg());
    const int err2 = lfs_mount(&lfs, get_lfs_cfg());
    if (err2 != 0) {
      cli_print_linef("LFS mounting failed again with error %d!", err2);
      return;
    }
    cli_print_line("Mounting successful!");
  }
  flight_counter = 0;
  /* create the flights directory */
  lfs_mkdir(&lfs, "flights");
  lfs_mkdir(&lfs, "stats");
  lfs_mkdir(&lfs, "configs");

  strncpy(cwd, "/", sizeof(cwd));
}

static void cli_cmd_flash_write(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  cli_print_line("\nSetting recorder state to REC_WRITE_TO_FLASH");
  set_recorder_state(REC_WRITE_TO_FLASH);
}

static void cli_cmd_flash_stop(const char *cmd_name [[maybe_unused]], char *args [[maybe_unused]]) {
  cli_print_line("\nSetting recorder state to REC_FILL_QUEUE");
  set_recorder_state(REC_FILL_QUEUE);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void cli_cmd_flash_test(const char *cmd_name [[maybe_unused]], char *args) {
  uint8_t write_buf[256] = {};
  uint8_t read_buf[256] = {};
  fill_buf(write_buf, 256);
  // w25q_chip_erase();
  if (strcmp(args, "full") == 0) {
    cli_print_line("\nStep 1: Erasing the chip sector by sector...");
    for (uint32_t i = 0; i < w25q.sector_count; ++i) {
      if (i % 100 == 0) {
        cli_print_linef("%lu / %lu sectors erased...", i, w25q.sector_count);
      }
      const w25q_status_e sector_erase_status = w25q_sector_erase(i);
      if (sector_erase_status != W25Q_OK) {
        cli_print_linef("Sector erase error encountered at sector %lu; status %d", i, sector_erase_status);
        osDelay(5000);
      }
    }

    cli_print_line("Step 2: Sequential write test");
    for (uint32_t i = 0; i < w25q.page_count; ++i) {
      if (i % 100 == 0) {
        cli_print_linef("%lu / %lu pages written...", i, w25q.page_count);
      }
      const w25q_status_e write_status = w25qxx_write_page(write_buf, i, 0, 256);
      if (write_status != W25Q_OK) {
        cli_print_linef("Write error encountered at page %lu; status %d", i, write_status);
        osDelay(5000);
      }
    }

    cli_print_line("Step 3: Sequential read test");
    for (uint32_t i = 0; i < w25q.page_count; ++i) {
      memset(read_buf, 0, 256);
      if (i % 100 == 0) {
        cli_print_linef("%lu / %lu pages read...", i, w25q.page_count);
      }
      const w25q_status_e read_status = w25qxx_read_page(read_buf, i, 0, 256);
      if (read_status != W25Q_OK) {
        cli_print_linef("Read error encountered at page %lu; status %d", i, read_status);
        osDelay(1);
      }
      if (memcmp(write_buf, read_buf, 256) != 0) {
        cli_print_linef("Buffer mismatch at page %lu", i);
        osDelay(1);
      }
    }

    cli_print_line("\nStep 4: Erasing the chip sector by sector...");
    for (uint32_t i = 0; i < w25q.sector_count; ++i) {
      if (i % 100 == 0) {
        cli_print_linef("%lu / %lu sectors erased...", i, w25q.sector_count);
      }
      const w25q_status_e sector_erase_status = w25q_sector_erase(i);
      if (sector_erase_status != W25Q_OK) {
        cli_print_linef("Sector erase error encountered at sector %lu; status %d", i, sector_erase_status);
        osDelay(5000);
      }
    }
  } else {
    char *endptr = nullptr;
    const uint32_t sector_idx = strtoul(args, &endptr, 10);
    if (args != endptr) {
      if (sector_idx >= w25q.sector_count) {
        cli_print_linef("Sector %lu not found!", sector_idx);
        return;
      }

      cli_print_linef("\nStep 1: Erasing sector %lu", sector_idx);
      w25q_status_e sector_erase_status = w25q_sector_erase(sector_idx);
      if (sector_erase_status != W25Q_OK) {
        cli_print_linef("Sector erase error encountered at sector %lu; status %d", sector_idx, sector_erase_status);
        osDelay(5000);
      }

      const uint32_t start_page_idx = w25q_sector_to_page(sector_idx);
      const uint32_t pages_per_sector = w25q.sector_size / w25q.page_size;
      const uint32_t end_page_idx = start_page_idx + pages_per_sector - 1;
      cli_print_linef("Step 2: Sequential write test (start_page: %lu, end_page: %lu)", start_page_idx, end_page_idx);
      for (uint32_t i = start_page_idx; i <= end_page_idx; ++i) {
        if (i % 4 == 0) {
          cli_print_linef("%lu / %lu pages written...", i - start_page_idx, pages_per_sector);
        }
        const w25q_status_e write_status = w25qxx_write_page(write_buf, i, 0, 256);
        if (write_status != W25Q_OK) {
          cli_print_linef("Write error encountered at page %lu; status %d", i, write_status);
          osDelay(5000);
        }
      }

      cli_print_linef("Step 3: Sequential read test (start_page: %lu, end_page: %lu)", start_page_idx, end_page_idx);
      for (uint32_t i = start_page_idx; i <= end_page_idx; ++i) {
        memset(read_buf, 0, 256);
        if (i % 4 == 0) {
          cli_print_linef("%lu / %lu pages read...", i - start_page_idx, pages_per_sector);
        }
        const w25q_status_e read_status = w25qxx_read_page(read_buf, i, 0, 256);
        if (read_status != W25Q_OK) {
          cli_print_linef("Read error encountered at page %lu; status %d", i, read_status);
          osDelay(1);
        }
        if (memcmp(write_buf, read_buf, 256) != 0) {
          cli_print_linef("Buffer mismatch at page %lu", i);
          osDelay(1);
        }
      }

      cli_print_linef("\nStep 4: Erasing sector %lu...", sector_idx);
      sector_erase_status = w25q_sector_erase(sector_idx);
      if (sector_erase_status != W25Q_OK) {
        cli_print_linef("Sector erase error encountered at sector %lu; status %d", sector_idx, sector_erase_status);
        osDelay(5000);
      }
    }
  }
  cli_print_line("Test complete!");
}

#ifdef CATS_DEV
static void cli_cmd_start_simulation(const char *cmd_name [[maybe_unused]], char *args) { start_simulation(args); }
#endif

/**  Helper function definitions **/

static void print_control_config() {
  cli_print_line("\n * CONTROL SETTINGS *\n");

  cli_printf("  Liftoff Acc. Threshold: %u m/s^2\n", global_cats_config.control_settings.liftoff_acc_threshold);
  cli_printf("  Main Altitude:          %u m\n", global_cats_config.control_settings.main_altitude);
}

static void print_action_config() {
  const EnumToStrMap &p_event_table = lookup_tables[TABLE_EVENTS];
  const EnumToStrMap &p_action_table = lookup_tables[TABLE_ACTIONS];

  cli_printf("\n * ACTION CONFIGURATION *\n");
  config_action_t action{};
  for (int i = 0; i < NUM_EVENTS; i++) {
    const auto ev = static_cast<cats_event_e>(i);
    const int nr_actions = cc_get_num_actions(ev);
    if (nr_actions > 0) {
      cli_printf("\n%s\n", GetStr(ev, p_event_table));
      cli_printf("   Number of Actions: %d\n", nr_actions);
      for (int j = 0; j < nr_actions; j++) {
        cc_get_action(ev, j, &action);
        cli_printf("     %s - %d\n", p_action_table[action.action_idx], action.arg);
      }
    }
  }
}

static void print_timer_config() {
  const EnumToStrMap &p_event_table = lookup_tables[TABLE_EVENTS];

  cli_printf("\n\n * TIMER CONFIGURATION *\n");
  for (int i = 0; i < NUM_TIMERS; i++) {
    if (global_cats_config.timers[i].duration > 0) {
      cli_printf("\nTIMER %d\n", i + 1);
      cli_printf("  Start:    %s\n",
                 GetStr(static_cast<cats_event_e>(global_cats_config.timers[i].start_event), p_event_table));
      cli_printf("  Trigger:  %s\n",
                 GetStr(static_cast<cats_event_e>(global_cats_config.timers[i].trigger_event), p_event_table));
      cli_printf("  Duration: %lu ms\n", global_cats_config.timers[i].duration);
    }
  }
}

// TODO: The casts in this function can be fixed with:
// *(reinterpret_cast<uint8_t*>(const_cast<void*>(ptr))) = static_cast<uint8_t>(value);
// But it should be tested.
// NOLINTBEGIN(google-readability-casting)
static void cli_set_var(const cli_value_t *var, const uint32_t value) {
  const void *ptr = get_cats_config_member_ptr(&global_cats_config, var);

  uint32_t work_value = 0;
  uint32_t mask = 0;

  if ((var->type & VALUE_MODE_MASK) == MODE_BITSET) {
    switch (var->type & VALUE_TYPE_MASK) {
      case VAR_UINT8:
        mask = (1U << var->config.bitpos) & 0xffU;
        if (value > 0) {
          work_value = *(uint8_t *)ptr | mask;
        } else {
          work_value = *(uint8_t *)ptr & ~mask;
        }
        *(uint8_t *)ptr = work_value;
        break;

      case VAR_UINT16:
        mask = (1U << var->config.bitpos) & 0xffffU;
        if (value > 0) {
          work_value = *(uint16_t *)ptr | mask;
        } else {
          work_value = *(uint16_t *)ptr & ~mask;
        }
        *(uint16_t *)ptr = work_value;
        break;

      case VAR_UINT32:
        mask = 1U << var->config.bitpos;
        if (value > 0) {
          work_value = *(uint32_t *)ptr | mask;
        } else {
          work_value = *(uint32_t *)ptr & ~mask;
        }
        *(uint32_t *)ptr = work_value;
        break;
    }
  } else {
    switch (var->type & VALUE_TYPE_MASK) {
      case VAR_UINT8:
        *(uint8_t *)ptr = value;
        break;

      case VAR_INT8:
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        *(int8_t *)ptr = value;
        break;

      case VAR_UINT16:
        *(uint16_t *)ptr = value;
        break;

      case VAR_INT16:
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        *(int16_t *)ptr = value;
        break;

      case VAR_UINT32:
        *(uint32_t *)ptr = value;
        break;
    }
  }
}
// NOLINTEND(google-readability-casting)

static void fill_buf(uint8_t *buf, size_t buf_sz) {
  for (uint32_t i = 0; i < buf_sz / 2; ++i) {
    buf[i] = i * 2;
    buf[buf_sz - i - 1] = i * 2 + 1;
  }
}
