/*
 * reader.h
 *
 *  Created on: 2 Mar 2021
 *      Author: Luca
 */

#pragma once

#include "util/types.h"
#include "cmsis_os.h"

void dump_recording(uint16_t number);
void parse_recording(uint16_t number);
void erase_recordings();
