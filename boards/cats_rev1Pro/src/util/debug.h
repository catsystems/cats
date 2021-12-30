/*
 * CATS Flight Software
 * Copyright (C) 2021 Control and Telemetry Systems
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

/* Debug flag */
#ifdef CATS_DEBUG
#undef CATS_DEBUG
#endif

/* Comment the next line in order to disable debug mode */
#define CATS_DEBUG

#define configUSE_TRACE_FACILITY 0

#if (configUSE_TRACE_FACILITY == 1) && defined(CATS_DEBUG)
#undef CATS_DEBUG
#endif
