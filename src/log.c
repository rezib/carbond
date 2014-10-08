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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h> // va_start(), va_end()
#include "common.h"
#include "log.h"

#define LOG_BUF_MAXSIZE 256

static void log_msg(const log_level_t level, const char *fmt, va_list args) {

    char *buf = NULL;
    char *pfx = NULL;

    if(level <= conf->log_level) {

        buf = malloc(LOG_BUF_MAXSIZE);

        switch(level) {
            case LOG_LEVEL_ERROR:
                pfx = "ERROR";
                break;
            case LOG_LEVEL_WARNING:
                pfx = "WARNING";
                break;
            case LOG_LEVEL_INFO:
                pfx = "INFO";
                break;
            case LOG_LEVEL_VERBOSE:
                pfx = "VERBOSE";
                break;
            case LOG_LEVEL_DEBUG:
                pfx = "DEBUG";
                break;
            default:
                pfx = "[*UNKNOWN LOG LEVEL*]";
        }

        vsnprintf(buf, LOG_BUF_MAXSIZE, fmt, args);
        printf("%s: %s\n", pfx, buf);
        free(buf);
    }
}

void info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_msg(LOG_LEVEL_INFO, fmt, ap);
  va_end(ap);
}

void _debug(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_msg(LOG_LEVEL_DEBUG, fmt, ap);
  va_end(ap);
}
