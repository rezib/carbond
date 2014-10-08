/*
 * Copyright (C) 2014 - Rémi Palancher <remi@rezib.org>
 *
 * This file is part of carbond, an implementation in C of Graphite
 * carbon daemon.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef CARBOND_LOG_H
#define CARBOND_LOG_H

#include "log.h"

#define debug(M, ...) \
    if (conf->tracing) \
        _debug("%s:%s:%d: " M "", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    else \
        _debug("" M "", ##__VA_ARGS__)


typedef enum {
  LOG_LEVEL_QUIET = 0,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_INFO,
  LOG_LEVEL_VERBOSE,
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_END
} log_level_t;

void info(const char *, ...);
void _debug(const char *, ...);

#endif
