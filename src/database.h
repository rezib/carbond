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

#ifndef CARBON_DATABASE_H
#define CARBON_DATABASE_H

#include <stdint.h>
#include "common.h"


metric_t * get_metric(metrics_database_t *, char *);
void add_database_metric_point(metrics_database_t *, metric_t *, metric_point_t *);
void add_database_metric(metrics_database_t *, metric_t *);
metric_point_t * create_new_metric_point(uint32_t, double);
metric_t * create_new_metric(char *);
void create_database(void);

#endif
