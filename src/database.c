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

#include <string.h>  // strlen()
#include <stdlib.h>  // malloc()
#include <stdio.h>   // printf()
#include <pthread.h> // pthread_mutex_init()

#include "database.h"

/*
 * Checks if metric name already exists in database. If yes, returns a pointer
 * to the the metric. Else returns NULL.
 */

metric_t * get_metric(metrics_database_t * db, char * m_name) {

    metric_t *cur_m = db->first;

    while(cur_m) {
        if (strlen(m_name) == strlen(cur_m->name))
            if (strncmp(cur_m->name, m_name, strlen(m_name)+1) == 0)
                return cur_m;
        cur_m = cur_m->next;
    }

    return NULL;
}

void add_database_metric_point(metrics_database_t * db,
                               metric_t * m,
                               metric_point_t * new_point) {

    if (m->last == NULL) { /* no points for this metric yet */
        m->points = new_point;
    } else {
        m->last->next = new_point;
    }
    m->last = new_point;
    m->nb_points += 1;
}

void add_database_metric(metrics_database_t *db, metric_t *new_metric) {

    if (db->last == NULL) { /* no metric in database yet */
        db->first = new_metric;
    } else {
        db->last->next = new_metric;
    }
    db->last = new_metric;
}

metric_point_t * create_new_metric_point(uint32_t timestamp, double value) {

    metric_point_t *res = calloc(1, sizeof(metric_point_t));
    res->timestamp = timestamp;
    res->value = value;
    res->next = NULL;
    return res;

}

metric_t * create_new_metric(char * name) {
    
    metric_t *res = calloc(1, sizeof(metric_t));
    res->name = malloc(sizeof(char)*strlen(name)+1);
    strncpy(res->name, name, strlen(name)+1);
    res->points = NULL;
    res->next = NULL;
    res->last = NULL;
    res->nb_points = 0;
    if (pthread_mutex_init(&(res->lock), NULL) != 0) {
        printf("\n mutex init failed\n");
    }
    return res;

}

void create_database() {

    conf->db = calloc(1, sizeof(metrics_database_t));
    conf->db->first = NULL;
    conf->db->last = NULL;

}
