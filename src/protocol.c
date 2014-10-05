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
#include <strings.h>
#include <string.h>
#include "protocol.h"
#include "database.h" // manage database recors

void protocol_process_metric_line(char *metric_line) {

    char *metric_name = malloc(sizeof(char) * METRIC_NAME_MAX_LEN);
    double value = 0.0;
    uint32_t timestamp = 0;
    metric_t *related_metric = NULL;
    metric_point_t *metric_point = NULL;

    sscanf(metric_line, "%s %lf %u", metric_name, &value, &timestamp);

    //printf("parsed metric:%s timestamp:%u value:%f\n", metric_name, timestamp, value);

    related_metric = get_metric(conf->db, metric_name);

    if(related_metric == NULL) { /* metric does not exist yet */
        related_metric = create_new_metric(metric_name);
        add_database_metric(conf->db, related_metric);
    }
    
    metric_point = create_new_metric_point(timestamp, value);
    add_database_metric_point(conf->db, related_metric, metric_point);

    free(metric_name);
}

void protocol_process_metrics_multiline(char *metrics_multiline) {

    char *cur_line = NULL;
    char *saveptr = NULL;

    cur_line = strtok_r(metrics_multiline, "\n", &saveptr);
    while(cur_line) {
        protocol_process_metric_line(cur_line);
        cur_line = strtok_r(NULL, "\n", &saveptr);
    }
}

