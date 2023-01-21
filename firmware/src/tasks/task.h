/*
 * CATS Flight Software
 * Copyright (C) 2023 Control and Telemetry Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <array>
#include <cstdint>
#include <typeinfo>
#include <utility>
#include "config/globals.h"
#include "util/log.h"

#include "cmsis_os.h"

namespace task {

template <typename T, uint32_t STACK_SZ>
class Task {
 protected:
  /* Protected constructor */
  Task() = default;

  virtual void Run() noexcept = 0;
  flight_fsm_e m_fsm_enum = INVALID;

  /* Update FSM enum */
  bool GetNewFsmEnum() {
    auto new_enum = static_cast<flight_fsm_e>(osEventFlagsWait(fsm_flag_id, 0xFF, osFlagsNoClear, 0));

    /* If this happens, there is an error on the Event Flag.*/
    if (new_enum > TOUCHDOWN || new_enum < MOVING) {
      return false;
    }

    if (new_enum == this->m_fsm_enum) {
      return false;
    } else {
      this->m_fsm_enum = new_enum;
      return true;
    }
  }

 private:
  std::array<uint32_t, STACK_SZ> m_task_buffer{};
  StaticTask_t m_task_control_block{};

  // clang-format off
  const osThreadAttr_t m_task_attributes = {
      // TODO: This is not a good name
      .name = typeid(T).name(),
      .cb_mem = &m_task_control_block,
      .cb_size = sizeof(m_task_control_block),
      .stack_mem = m_task_buffer.data(),
      .stack_size = m_task_buffer.size(),
      .priority = (osPriority_t)osPriorityNormal,
  };
  // clang-format on

  static constexpr void RunWrapper(void* task_ptr) noexcept { static_cast<T*>(task_ptr)->Run(); }

 public:
  /* Deleted move constructor & move assignment operator */
  Task(Task&&) = delete;
  Task& operator=(Task&&) = delete;

  /* Deleted copy constructor & assignment operator */
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  /* Deleted new & delete operators */
  static void* operator new(std::size_t size) = delete;
  static void* operator new[](std::size_t size) = delete;
  static void operator delete(void* ptr) = delete;
  static void operator delete[](void* ptr) = delete;

  template <typename... Args>
  static T& GetInstance(Args&&... args) noexcept {
    /* Static local variable */
    static T instance(std::forward<Args>(args)...);
    return instance;
  }

  static constexpr void Start() noexcept {
    auto& task = T::GetInstance();
    osThreadNew(RunWrapper, &task, &task.m_task_attributes);
  }
};

}  // namespace task
