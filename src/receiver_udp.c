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
#include <unistd.h> // sleep()
#include <errno.h>
#include <string.h> // strerror()
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h> // pthread_create()

#include "receiver_udp.h"
#include "common.h"
#include "threads.h"
#include "protocol.h"

/*
 * UDP receiver thread worker.
 * Loop as long as conf->run on UDP socket input.
 */
void * receiver_udp_worker(void * arg) {

    int n = 0; /* used to store message len */
    receiver_udp_args_t *worker_args = (receiver_udp_args_t *) arg;
    //int id_thread = worker_args->id_thread;
    int sockfd = worker_args->sockfd; /* fd on UDP socket */

    socklen_t len;
    struct sockaddr_in cliaddr;
    const uint32_t MAX_UDP_READ = 65535;
    char mesg[MAX_UDP_READ];

    /*
     * Blocks signals (SIGINT, SIGTERM, etc) in this thread so that they are all
     * handled in main thread.
     */
    block_signals();

    for (;conf->run;) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, mesg, MAX_UDP_READ, 0, (struct sockaddr *)&cliaddr, &len);
        if (n == -1) {
            switch(errno) {
                case EAGAIN: /* timeout reached and nothing received */
                    break;
                default:     /* else unmanaged error that deserves to be printed */
                    error("error occured on recvfrom: %s\n", strerror(errno));
            }
        } else {
            debug("received %d bytes", n);
            mesg[n] = '\0';
            protocol_process_metrics_multiline(mesg);
       }
    }

    return NULL;

}

/*
 * Create, initialize with appropriate parameters and returns the socket of the
 * UDP receiver. Returns -1 on error.
 */
static int receiver_udp_init_socket() {

    int sockfd;
    int optval;
    struct timeval tv;

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { 
        error("error on opening socket: %s", strerror(errno));
        return -1;
    }

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0) {
        error("error on setsockopt() SO_REUSEADDR: %s", strerror(errno));
        return -1;
    }

    /* 0.5 sec timeout */
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0) {
        error("error on setsockopt() SO_RCVTIMEO: %s", strerror(errno));
        return -1;
    }

    return sockfd;

}

/*
 * Binds UDP socket to appropriate address:port.
 * Returns 0 on success, -1 on error.
 */
static int receiver_udp_bind_socket(int sockfd) {

    int portno = conf->udp_receiver_port;
    struct sockaddr_in serveraddr;

    /*
     * build the server's Internet address
     */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        perror("ERROR on binding");
        return -1;
    }

    return 0;

}

carbon_thread_t launch_receiver_udp_thread() {

    int sockfd;
    carbon_thread_t thread;
    receiver_udp_args_t *args = calloc(1, sizeof(receiver_udp_args_t));

    debug("creating the UDP socket");

    /* initialize socket with its parameters */
    sockfd = receiver_udp_init_socket();

    /* bind port to start listening */
    receiver_udp_bind_socket(sockfd);

    /* initialize thread parameters */
    args->id_thread = 0;
    args->sockfd = sockfd;

    thread.name = "UDP receiver";

    if (pthread_create(&thread.pthread, NULL, receiver_udp_worker, (void*)args) != 0) {
        error("error on pthread_create: %s\n", strerror(errno));
    }

    return thread;

}
