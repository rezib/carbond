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
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "threads.h"
#include "common.h" // debug()

/*
 * Block SIGINT; other threads created
 * will inherit a copy of the signal mask.
 */
void block_signals() {

    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        error("error on pthread_sigmask(): %s", strerror(errno));
    }

}

/*
 * initialize carbon_thread_t members
 */
void thread_init(carbon_thread_t *thread, char *name) {

    thread->name = name;
    thread->must_pause = false;

    if (pthread_mutex_init(&(thread->run_lock), NULL) != 0) {
        error("thread %s run_lock mutex init failed", thread->name);
    }

    if (pthread_cond_init (&(thread->can_run), NULL) != 0) {
        error("thread %s can_run cond init failed", thread->name);
    }

}

/*
 * only called by threads_pause_all()
 */
static void thread_order_pause(carbon_thread_t *thread) {

    debug("ordered pause for thread %s", thread->name);
    thread->must_pause = true;

}

void thread_run_lock(carbon_thread_t *thread) {

    pthread_mutex_lock(&(thread->run_lock));

}

/*
 * only called by threads_pause_all()
 */
static void thread_wait_paused(carbon_thread_t *thread) {

    thread_run_lock(thread);

}

/*
 * only called by threads_resume_all()
 */
static void thread_resume(carbon_thread_t *thread) {

    debug("resuming thread %s", thread->name);
    thread->must_pause = false;
    pthread_cond_signal(&(thread->can_run));
    pthread_mutex_unlock(&(thread->run_lock));

}

bool thread_must_pause(carbon_thread_t *thread) {

    return thread->must_pause;

}

void thread_pause_and_wait_run_signal(carbon_thread_t *thread) {

    debug("thread %s waiting for run cond", thread->name);
    pthread_cond_wait(&(thread->can_run),&(thread->run_lock));
    debug("thread %s pause end", thread->name);

}

/*
 * Wait for a thread to stop
 */
static void thread_wait_stopped(carbon_thread_t *thread) {

    pthread_join(thread->pthread, NULL);
    debug("%s thread stopped.", thread->name);

}

/*
 * Wait for all threads to stop
 */
void threads_wait_all_stopped() {

    thread_wait_stopped(threads->receiver_udp_thread);
    thread_wait_stopped(threads->receiver_tcp_thread);
    thread_wait_stopped(threads->writer_thread);
    thread_wait_stopped(threads->monitoring_thread);
    debug("all threads are stopped");

}

/*
 * Order all threads to pause and wait for them to be paused. Returns once all
 * threads are paused.
 */
void threads_pause_all() {

    debug("pausing all threads");
    thread_order_pause(threads->receiver_udp_thread);
    thread_order_pause(threads->receiver_tcp_thread);
    thread_order_pause(threads->writer_thread);
    thread_order_pause(threads->monitoring_thread);

    thread_wait_paused(threads->receiver_udp_thread);
    thread_wait_paused(threads->receiver_tcp_thread);
    thread_wait_paused(threads->writer_thread);
    thread_wait_paused(threads->monitoring_thread);

}

/*
 * Unpause all threads.
 */
void threads_resume_all() {

    debug("resuming all threads");
    thread_resume(threads->receiver_udp_thread);
    thread_resume(threads->receiver_tcp_thread);
    thread_resume(threads->writer_thread);
    thread_resume(threads->monitoring_thread);

}
