/// Copyright (C) 2020, 2024 Control and Telemetry Systems GmbH
///
/// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include <Random.hpp>
#include <Sx1280Driver.hpp>

constexpr int32_t kFreqCorrectionMax = static_cast<int32_t>(100'000 / FREQ_STEP);
constexpr int32_t kFreqCorrectionMin = static_cast<int32_t>(-100'000 / FREQ_STEP);

consteval uint32_t FreqHzToRegVal(uint32_t freq) {
  return static_cast<uint32_t>(static_cast<double>(freq) / static_cast<double>(FREQ_STEP));
}

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern volatile uint8_t FHSSptr;
extern uint8_t FHSSsequence[];
extern uint_fast8_t sync_channel;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
extern const uint32_t FHSSfreqs[];
// NOLINTNEXTLINE(clang-diagnostic-c++17-extensions) not sure why this is needed; all source files are compiled as C++20
inline constexpr uint8_t FHSS_SEQUENCE_CNT = 20;  //(256 / FHSS_FREQ_CNT) * FHSS_FREQ_CNT;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t crc);
// The number of frequencies for this regulatory domain
uint32_t FHSSgetChannelCount();

// get the initial frequency, which is also the sync channel
static inline uint32_t GetInitialFreq() { return FHSSfreqs[sync_channel]; }

// Get the current sequence pointer
static inline uint8_t FHSSgetCurrIndex() { return FHSSptr; }

// Set the sequence pointer, used by RX on SYNC
static inline void FHSSsetCurrIndex(const uint8_t value) { FHSSptr = value % FHSS_SEQUENCE_CNT; }

// Advance the pointer to the next hop and return the frequency of that channel
static inline uint32_t FHSSgetNextFreq() {
  FHSSptr = (FHSSptr + 1) % FHSS_SEQUENCE_CNT;
  return FHSSfreqs[FHSSsequence[FHSSptr]];
}

// get the number of entries in the FHSS sequence
static inline uint8_t FHSSgetSequenceCount() { return FHSS_SEQUENCE_CNT; }
