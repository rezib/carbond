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

#ifndef CARBON_COMMON_H
#define CARBON_COMMON_H

#include <linux/limits.h> /* PATH_MAX */
#include <stdint.h>
#include <stdbool.h>

#include "log.h"

#ifndef SYSCONFDIR
#    define SYSCONFDIR "/etc/carbon"
#endif
#ifndef LOCALSTATEDIR
#    define LOCALSTATEDIR "/var/lib/carbon"
#endif

#define METRIC_NAME_MAX_LEN 100
    
/* storage schema */

struct retention {
    uint32_t time_per_point;
    uint32_t time_to_store;
    struct retention * next;
};

typedef struct retention retention_t;

struct pattern_retention {
    char * str_pattern;
    char * str_retention;
    struct retention * retention_list;
    struct pattern_retention * next;
};

typedef struct pattern_retention pattern_retention_t;

/* storage aggregation */

enum aggregation_type_e {
    AGG_TYPE_UNDEF,
    AGG_TYPE_AVERAGE,
    AGG_TYPE_SUM,
    AGG_TYPE_LAST,
    AGG_TYPE_MAX,
    AGG_TYPE_MIN
};

typedef enum aggregation_type_e aggregation_type_t;

struct pattern_aggregation_s {
    char *pattern;
    float xff;
    aggregation_type_t method;
    struct pattern_aggregation_s * next;
};

typedef struct pattern_aggregation_s pattern_aggregation_t;

/* database */

struct metrics_database {
    struct metric *first;
    struct metric *last;
};

typedef struct metrics_database metrics_database_t;

struct metric_point {
    uint32_t timestamp;
    double value;
    struct metric_point *next;
};

typedef struct metric_point metric_point_t;

struct metric {
    char * name;
    uint32_t nb_points;
    struct metric_point *points;
    struct metric_point *last;
    struct metric *next;
    pthread_mutex_t lock;
};

typedef struct metric metric_t;

/* carbon runtime parameters */

struct carbon_conf_s {
    pattern_retention_t *schema;
    pattern_aggregation_t *aggregation;
    metrics_database_t *db;
    char *conf_dir;
    char *conf_file;
    char *storage_dir;
    int line_receiver_port;
    int udp_receiver_port;
    bool run; /* should the app keeps running or stop? */
    /* flag to print file, function and line number in debug() */
    bool tracing;
    log_level_t log_level;
};

typedef struct carbon_conf_s carbon_conf_t;

struct monitoring_metrics_s {
    uint32_t points;
    pthread_mutex_t mutex_points;
};

typedef struct monitoring_metrics_s monitoring_metrics_t;

/*
 *  global conf variable
 */
extern carbon_conf_t *conf;

extern monitoring_metrics_t *monitoring;

#endif
