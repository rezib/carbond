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

#include "writer.h"
#include "threads.h"

/*
 * Lock the metric, call whisper function to write all points in cache, and
 * finally empty and unlock the metric.
 */
void write_metric(struct metric * m) {


    metric_point_t *mt_p = m->points,
                   *mt_p_next = NULL;

    // LOCK METRIC
    pthread_mutex_lock(&(m->lock));

    while(mt_p) {
        mt_p_next = mt_p->next;
        whisper_write_value(m, mt_p->timestamp, mt_p->value);
        free(mt_p);
        mt_p = mt_p_next; // jump to next metric_point_t
    }

    /* empty metric_t */
    m->nb_points = 0;
    m->points = NULL;
    m->last = NULL;

    // UNLOCK METRIC
    pthread_mutex_unlock(&(m->lock));

}

/*
 * Search the metric with the most points in cache among the DB
 * and return a pointer to it.
 */
metric_t * find_largest_metric() {

    metric_t *cur_m = db->first,
             *max_m = NULL;

    uint32_t max_nb_points = 0;

    while(cur_m) {

        if (cur_m->nb_points > max_nb_points) {
            max_m = cur_m;
            max_nb_points = cur_m->nb_points;
        }
        cur_m = cur_m->next;
    }

    return max_m;
}

void * writer_thread(void * thread_args) {

    struct writer_thread_args * w_thd_args = (struct writer_thread_args *) thread_args;
    carbon_thread_t *me = w_thd_args->thread;
    metric_t *max_m = NULL;
    /*
     * Blocks signals (SIGINT, SIGTERM, etc) in this thread so that they are all
     * handled in main thread.
     */
    block_signals();

    debug("thread %u is running", w_thd_args->id_thread);

    thread_run_lock(me);

    for(;conf->run;) {

        if(thread_must_pause(me)) {
            thread_pause_and_wait_run_signal(me);
        }

        max_m = find_largest_metric();

        if (max_m) {
            debug("largest metric: %s nb_points: %u", max_m->name, max_m->nb_points);
            /* write metric on disk */
            write_metric(max_m);
        } else {
            /* empty database */
            sleep(1);
        }
    }

    return NULL;
}

carbon_thread_t *launch_writer_thread() {

    carbon_thread_t *thread;
    struct writer_thread_args * w_thd_args = NULL;

    thread = calloc(1, sizeof(carbon_thread_t));
    thread_init(thread, "writer");

    w_thd_args = (struct writer_thread_args *) malloc(sizeof(struct writer_thread_args));
    w_thd_args->id_thread = 0;
    w_thd_args->thread = thread;

    if (pthread_create(&(thread->pthread), NULL, writer_thread, (void*)w_thd_args) != 0) {
        error("error on pthread_create: %s\n", strerror(errno));
        exit(1);
    }

    return thread;

}
