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

#ifndef CARBON_THREADS_H
#define CARBON_THREADS_H

struct carbon_thread_s {
    pthread_t pthread;
    char *name;
};

typedef struct carbon_thread_s carbon_thread_t;

/* carbon threads */

struct carbon_threads_s {
    carbon_thread_t *receiver_udp_thread;
    carbon_thread_t *receiver_tcp_thread;
    carbon_thread_t *writer_thread;
    carbon_thread_t *monitoring_thread;
};

typedef struct carbon_threads_s carbon_threads_t;

extern carbon_threads_t *threads;

void block_signals();
void thread_init(carbon_thread_t *, char *);
void threads_wait_all_stopped();

#endif
