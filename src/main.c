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

#include <stdlib.h> /* malloc */
#include <getopt.h>
#include <signal.h>

#include "common.h"
#include "conf.h"
#include "threads.h"
#include "receiver_udp.h"
#include "receiver_tcp.h"
#include "database.h"
#include "writer.h"

/*
 * Initialize global runtime configuration variable
 */
carbon_conf_t *conf = NULL;

void print_usage() {

    printf("usage: carbon [-h] [-d] [-c CONF]\n"
           "-h, --help         Print this help\n"
           "-d, --debug        Debug mode\n"
           "-c, --conf CONF    Specify an alternative configuration file\n"
           "                     (default: %%sysconfdir/carbon/carbon.conf)\n\n");

}

/*
 * Set default configuration in conf global variable
 */
void default_conf() {

    const char * sysconfdir = SYSCONFDIR;
    const char * localstatedir = LOCALSTATEDIR;
    const char * default_conf_filename = "/carbon.conf";

    conf->run = true;

    /* log level */
    conf->tracing = false;
    conf->log_level = LOG_LEVEL_INFO;

    /* config directory path */
    conf->conf_dir = malloc(sizeof(char)*PATH_MAX);
    memset(conf->conf_dir, 0, sizeof(char)*PATH_MAX);
    strncpy(conf->conf_dir, sysconfdir, strlen(sysconfdir));

    /* config file path */
    conf->conf_file = malloc(sizeof(char)*PATH_MAX);
    memset(conf->conf_file, 0, sizeof(char)*PATH_MAX);
    strncpy(conf->conf_file, sysconfdir, strlen(sysconfdir));
    strncat(conf->conf_file, default_conf_filename, strlen(default_conf_filename));

    /* storage directory path */
    conf->storage_dir = malloc(sizeof(char)*PATH_MAX);
    memset(conf->storage_dir, 0, sizeof(char)*PATH_MAX);
    strncpy(conf->storage_dir, localstatedir, strlen(localstatedir));

    /* default listened TCP/UDP ports */
    conf->line_receiver_port = 2003;
    conf->udp_receiver_port = 2003;

    conf->schema = NULL;
    conf->aggregation = NULL;
}

/*
 * Prints the runtime configuration parameters to stdout
 */
void print_conf() {

    debug("runtime configuration:");
    debug("  conf_dir: %s", conf->conf_dir);
    debug("  conf_file: %s", conf->conf_file);
    debug("  storage_dir: %s", conf->storage_dir);
    debug("  tracing: %d", conf->tracing);
    debug("  log_level: %d", conf->log_level);
    debug("  line_receiver_port: %d", conf->line_receiver_port);
    debug("  udp_receiver_port: %d", conf->udp_receiver_port);

}

/*
 * Parse args and set conf accordingly
 */
void parse_args(int argc, char * argv[]) {

    int opt = 0, long_index = 0;
    const char * opt_string = "hdc:";

    static const struct option long_opts[] = {
        { "help", no_argument, NULL, 'h' },
        { "debug", no_argument, NULL, 'd' },
        { "conf", required_argument, NULL, 'c' }
    };

    opt = getopt_long(argc, argv, opt_string,
                      (const struct option *)&long_opts,
                      &long_index);

    while (opt != -1) {

        switch(opt) {

            case 'h':
                print_usage();
                exit(0);
                break;
            case 'c':
                free(conf->conf_file);
                conf->conf_file = malloc(sizeof(char) * PATH_MAX);
                strncpy(conf->conf_file, optarg, strlen(optarg));
                break;
            case 'd':
                conf->tracing = true;
                conf->log_level = LOG_LEVEL_DEBUG;
                break;
            default: /* never gets here */
                break;

        }

        opt = getopt_long(argc, argv, opt_string,
                          (const struct option *)&long_opts,
                          &long_index);

    }

}

/*
 * SIGINT signal handler.
 * Basically set conf->run boolean to false in order to stop threads loops.
 */
void sigint_handler() {

    debug("received SIGINT, stopping all threads");
    conf->run = false;

}

int main(int argc, char *argv[]) {

    carbon_thread_t receiver_udp_thread,
                    receiver_tcp_thread,
                    writer_thread;

    struct sigaction sa;
    int status = 0;

    /*
     * Runtime configuration management
     */

    conf = calloc(1, sizeof(carbon_conf_t));

    /* load default runtime configuration */
    default_conf();

    parse_args(argc, argv);

    /* parse config files */
    status = conf_parse_carbon_file();
    if (status) {
        error("error while parsing carbon configuration file\n");
        return EXIT_FAILURE;
    }

    status = conf_parse_storage_schema_file();
    if (status) {
        error("error while parsing storage schema file\n");
        return EXIT_FAILURE;
    }

    status = conf_parse_storage_aggregation_file();
    if (status) {
        error("error while parsing storage aggregation file\n");
        return EXIT_FAILURE;
    }

    print_conf();
    print_storage_schema();

    check_whisper_sizes();

    create_database();

    /*
     *  signals handling
     */
    sa.sa_handler = &sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    /*
     * threads
     */

    /* launch main threads */
    receiver_udp_thread = launch_receiver_udp_thread();
    receiver_tcp_thread = launch_receiver_tcp_thread();
    writer_thread = launch_writer_thread();

    /* wait until end */
    wait_thread(receiver_udp_thread);
    wait_thread(receiver_tcp_thread);
    wait_thread(writer_thread);

    debug("all threads terminated properly");

    free(conf);

    return EXIT_SUCCESS;

}
