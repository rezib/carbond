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

#ifndef CARBON_RECEIVER_UDP_H
#define CARBON_RECEIVER_UDP_H

#include "threads.h"  // carbon_thread_t type

struct receiver_udp_args_s {
    int id_thread;
    int sockfd;
};

typedef struct receiver_udp_args_s receiver_udp_args_t;

void * receiver_udp_worker(void *);
carbon_thread_t * launch_receiver_udp_thread();

#endif
