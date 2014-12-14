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
#include <errno.h>
#include <string.h> // strerror()
#include <pthread.h> // pthread_create()
#include <time.h>

#include "monitoring.h"
#include "database.h"
#include "common.h"

/*
 * Add a metric point in global db. It eventually creates the metric in db if
 * it did not exist.
 */
static void update_monitoring_metric(const char *name,
                                     const uint32_t timestamp,
                                     const double value) {

    metric_t *metric = NULL;
    metric_point_t *point = NULL;

    metric = get_metric(db, name);
    if(metric == NULL) { /* metric does not exist yet */
        metric = create_new_metric(name);
        add_database_metric(db, metric);
    }

    point = create_new_metric_point(timestamp, value);
    add_database_metric_point(db, metric, point);

}

/*
 * Updates all metrics of the monitoring metrics structure, locking them
 * one by one to avoid concurrent updates and to avoid big locks.
 */
static void update_monitoring_metrics() {

    uint32_t timestamp;

    // get current timestamp
    timestamp = (uint32_t)time(NULL);

    pthread_mutex_lock(&(monitoring->mutex_points));
    update_monitoring_metric("carbond.points", timestamp, (double)monitoring->points);
    monitoring->points = 0;
    pthread_mutex_unlock(&(monitoring->mutex_points));

}

/*
 * Monitoring thread worker.
 * Loop as long as conf->run.
 */
void * monitoring_worker(void * arg) {

    monitoring_args_t *worker_args = (monitoring_args_t *) arg;
    //int id_thread = worker_args->id_thread;
    carbon_thread_t *me = worker_args->thread;
    struct timespec end, wait;

    /*
     * Blocks signals (SIGINT, SIGTERM, etc) in this thread so that they are all
     * handled in main thread.
     */
    block_signals();

    thread_run_lock(me);

    // init all mutexes
    if (pthread_mutex_init(&(monitoring->mutex_points), NULL) != 0) {
        error("monitoring mutex_points init failed");
    }

    for (;conf->run;) {

        if(thread_must_pause(me)) {
            thread_pause_and_wait_run_signal(me);
        }

        update_monitoring_metrics();

        if ((clock_gettime(CLOCK_REALTIME, &end) != 0)) {
            error("error getting monitoring end time: %s", strerror(errno));
        }

        // wait until the end of current second
        wait.tv_sec = 0;
        wait.tv_nsec = 1000000000-end.tv_nsec;

        debug("end nsec: %d", end.tv_nsec);

        nanosleep(&wait, NULL); // TODO: check return code
    }

    return NULL;

}

carbon_thread_t * launch_monitoring_thread() {

    carbon_thread_t *thread;
    monitoring_args_t *args = calloc(1, sizeof(monitoring_args_t));

    thread = calloc(1, sizeof(carbon_thread_t));

    /* initialize thread parameters */
    args->id_thread = 0;
    args->thread = thread;

    thread_init(thread, "monitoring");

    if (pthread_create(&(thread->pthread), NULL, monitoring_worker, (void*)args) != 0) {
        error("error on pthread_create: %s\n", strerror(errno));
    }

    return thread;

}
