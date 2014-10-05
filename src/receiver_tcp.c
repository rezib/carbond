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
#include <pthread.h>
#include <unistd.h> /* sleep() */
#include <errno.h>
#include <string.h> /* strerror() */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "receiver_tcp.h"
#include "threads.h"
#include "common.h"
#include "protocol.h"

void * receiver_tcp_worker(void * arg) {

    int n = 0; /* len of message sent by client */
    receiver_tcp_args_t *worker_args = (receiver_tcp_args_t *) arg;
    //int id_thread = worker_args->id_thread;
    int sockfd = worker_args->sockfd; /* fd on TCP socket */
    int conn; /* TCP connection */

    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(struct sockaddr_in);
    const uint32_t MAX_TCP_READ = 65535;
    char mesg[MAX_TCP_READ];

    /*
     * Blocks signals (SIGINT, SIGTERM, etc) in this thread so that they are all
     * handled in main thread.
     */
    block_signals();

    while(conf->run) {
        /*  Wait for a connection, then accept() it  */
        conn = accept(sockfd, NULL, NULL);
        if ( conn < 0 ) {
            switch(errno) {
                case EAGAIN: /* timeout reached and nothing received */
                    break;
                default:     /* else unmanaged error that deserves to be printed */
                    fprintf(stderr, "error calling accept(): %s\n", strerror(errno));
            }
       } else {

            /*  Retrieve an input line from the connected socket
                then simply write it back to the same socket.     */
            n = recvfrom(conn, mesg, MAX_TCP_READ, 0,(struct sockaddr *)&cliaddr, &clilen);
            
            if (n == -1)
                fprintf(stderr, "error occured on recvfrom: %s\n", strerror(errno));
            else {
                debug("received %d bytes", n);
                mesg[n] = '\0';
                protocol_process_metrics_multiline(mesg);
            }
            //close(conn);
        }
    }

    /* close listening TCP socket */
    close(sockfd);

    return NULL;

}

/*
 * Create, initialize with appropriate parameters and returns the socket of the
 * TCP receiver. Returns -1 on error.
 */
static int receiver_tcp_init_socket() {

    int sockfd;
    int optval;
    struct timeval tv;

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { 
        fprintf(stderr, "error on opening socket: %s", strerror(errno));
        return -1;
    }

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0) {
        fprintf(stderr, "error on setsockopt() SO_REUSEADDR: %s", strerror(errno));
        return -1;
    }

    /* 0.5 sec timeout */
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0) {
        fprintf(stderr, "error on setsockopt() SO_RCVTIMEO: %s", strerror(errno));
        return -1;
    }

    return sockfd;

}

/*
 * Binds TCP socket to appropriate address:port.
 * Returns 0 on success, -1 on error.
 */
static int receiver_tcp_bind_socket(int sockfd) {

    int portno = conf->line_receiver_port;
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
        perror("error on binding");
        return -1;
    }

    if (listen(sockfd, 10) < 0) {
        perror("error on listen");
        return -1;
    }

    return 0;

}

carbon_thread_t launch_receiver_tcp_thread() {

    int sockfd;
    carbon_thread_t thread;;
    receiver_tcp_args_t *args = calloc(1, sizeof(receiver_tcp_args_t));

    debug("creating the TCP socket");

    /* initialize socket with its parameters */
    sockfd = receiver_tcp_init_socket();

    /* bind port to start listening */
    receiver_tcp_bind_socket(sockfd);

    /* initialize thread parameters */
    args->id_thread = 0;
    args->sockfd = sockfd;

    thread.name = "TCP receiver";

    if (pthread_create(&thread.pthread, NULL, receiver_tcp_worker, (void*)args) != 0) {
        fprintf(stderr, "error on pthread_create: %s\n", strerror(errno));
        exit(1);
    }

    return thread;

}
