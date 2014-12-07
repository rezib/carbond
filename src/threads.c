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
