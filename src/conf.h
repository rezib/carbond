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

#ifndef CONF_H
#define CONF_H

#include "common.h"

#define READ_BUF_MAX  300 /* size of read buffer on config files */
#define PATTERN_MAX   100 /* max size of pattern string in storage schema */
#define RETENTION_MAX 100 /* max size of retention def string in storage schema */

/* utilities */

void trim_string(char *);
int string_starts_with(char *, char *);
uint32_t str_to_seconds(char *);

/* storage schema */

int conf_parse_storage_schema_file(carbon_conf_t *);
void print_storage_schema();

/* storage aggregation */
int conf_parse_storage_aggregation_file(carbon_conf_t *);

/* main configuration file */

char * get_conf_value(const char *);
int conf_parse_carbon_file(carbon_conf_t *);

#endif
