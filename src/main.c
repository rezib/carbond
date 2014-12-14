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
#include "database.h"
#include "threads.h"
#include "receiver_udp.h"
#include "receiver_tcp.h"
#include "writer.h"
#include "monitoring.h"

/*
 * Initialize global runtime configuration variable
 */
carbon_conf_t *conf = NULL;
/*
 * Initialize global db variable
 */
metrics_database_t *db = NULL;
/*
 * Initialize global threads variable
 */
carbon_threads_t *threads = NULL;
/*
 * Initialize global monitoring db variable
 */
monitoring_metrics_t *monitoring = NULL;

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
 * Copy conf structs members except from orig_conf to dest_conf, except:
 *  - schema
 *  - aggregation
 * Since this function is only used by conf_reload() and the schema and
 * aggregation members are populated by conf_parse_storage_schema_file()
 * and conf_parse_storage_aggregation_file() before the new runtime
 * configuration is used by threads.
 */
void conf_copy(carbon_conf_t *orig_conf, carbon_conf_t *dest_conf) {

    memcpy(dest_conf, orig_conf, sizeof(carbon_conf_t));
    dest_conf->schema = NULL;
    dest_conf->aggregation = NULL;

    /* copy strings properly */
    dest_conf->conf_dir = malloc(sizeof(char)*PATH_MAX);
    memset(dest_conf->conf_dir, 0, sizeof(char)*PATH_MAX);
    strncpy(dest_conf->conf_dir, orig_conf->conf_dir, strlen(orig_conf->conf_dir));

    dest_conf->conf_file = malloc(sizeof(char)*PATH_MAX);
    memset(dest_conf->conf_file, 0, sizeof(char)*PATH_MAX);
    strncpy(dest_conf->conf_file, orig_conf->conf_file, strlen(orig_conf->conf_file));

    dest_conf->storage_dir = malloc(sizeof(char)*PATH_MAX);
    memset(dest_conf->storage_dir, 0, sizeof(char)*PATH_MAX);
    strncpy(dest_conf->storage_dir, orig_conf->storage_dir, strlen(orig_conf->storage_dir));

}

/*
 * Free memory allocated for runtime conf.
 */
void conf_free(carbon_conf_t *c) {

    pattern_retention_t *pret = NULL,
                        *n_pret = NULL;
    retention_t *ret = NULL,
                *n_ret = NULL;
    pattern_aggregation_t *agg = NULL,
                          *n_agg = NULL;

    free(c->conf_dir);
    free(c->conf_file);
    free(c->storage_dir);

    pret = c->schema;

    while(pret) {
        n_pret = pret->next;
        free(pret->str_pattern);
        pret->str_pattern = NULL;
        free(pret->str_retention);
        pret->str_retention = NULL;
        ret = pret->retention_list;
        while(ret) {
            n_ret = ret->next;
            free(ret);
            ret = NULL;
            ret = n_ret;
        }
        pret->retention_list = NULL;
        free(pret);
        pret = NULL;
        pret = n_pret;
    }
    c->schema = NULL;

    agg = c->aggregation;

    while(agg) {
        n_agg = agg->next;
        free(agg->pattern);
        free(agg);
        agg = NULL;
        agg = n_agg;
    }
    c->aggregation = NULL;

    free(c);

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
 * Reload configuration files, then update runtime configuration accordingly.
 *
 * The logic is:
 *  1/ copy current runtime configuration
 *  2/ parse files to overload current runtime configuration
 *  3/ if parsing is OK:
 *    3.1/ pause threads
 *    3.2/ replace current runtime configuration by the new one
 *    3.3/ resume threads
 *
 * Why copying current runtime configuration instead of calling conf_default?
 * Because the default conf could have been overloaded by parameters, and we
 * do not want to parse parameters again at this point.
 */
void conf_reload() {

    carbon_conf_t *new_conf;
    int parse_status = 0;
    new_conf = calloc(1, sizeof(carbon_conf_t));
    conf_copy(conf, new_conf);
    parse_status = conf_parse(new_conf);

    if (parse_status)
        error("parsing new configuration failed");
    else {
        threads_pause_all();
        conf_free(conf);
        conf = new_conf;
        print_conf();
        threads_resume_all();
    }

}

/*
 * SIGHUP signal handler.
 * Reload configuration.
 */
void sighup_handler(int signb) {

    debug("received SIGHUP, reloading configuration");
    conf_reload();
}

/*
 * SIGINT signal handler.
 * Basically set conf->run boolean to false in order to stop threads loops.
 */
void sigint_handler(int signb) {

    debug("received SIGINT, stopping all threads");
    conf->run = false;

}

int main(int argc, char *argv[]) {

    struct sigaction sa_int, sa_hup;
    int status = 0;

    /*
     * Runtime configuration management
     */

    conf = calloc(1, sizeof(carbon_conf_t));
    db = calloc(1, sizeof(metrics_database_t));
    threads = calloc(1, sizeof(carbon_threads_t));
    monitoring = calloc(1, sizeof(monitoring_metrics_t));

    /* load default runtime configuration */
    default_conf();

    parse_args(argc, argv);

    /* parse config */
    status = conf_parse(conf);
    // exit if error encountered during config parsing
    if (status)
        return EXIT_FAILURE;

    print_conf();
    print_storage_schema();

    check_whisper_sizes();

    database_init();

    /*
     *  signals handling
     */

    sa_int.sa_handler = &sigint_handler;
    sigaction(SIGINT, &sa_int, NULL);
    sa_hup.sa_handler = &sighup_handler;
    sigaction(SIGHUP, &sa_hup, NULL);

    /*
     * threads
     */

    /* launch main threads */
    threads->monitoring_thread = launch_monitoring_thread();
    threads->receiver_udp_thread = launch_receiver_udp_thread();
    threads->receiver_tcp_thread = launch_receiver_tcp_thread();
    threads->writer_thread = launch_writer_thread();

    threads_wait_all_stopped();
    debug("all threads terminated properly");

    free(conf);

    return EXIT_SUCCESS;

}
