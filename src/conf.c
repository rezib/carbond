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
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "conf.h"

/*
 * Remove spaces/tabs/\r/\n from the string given in parameter
 */
void trim_string(char * string) {

    size_t s_res = 0, s_cur = 0, s_end = strlen(string);

    for (s_cur = 0; s_cur < s_end; s_cur ++) {
        if (string[s_cur] != ' ' && string[s_cur] != '\t'
            && string[s_cur] !='\n' && string[s_cur] != '\r') {
            string[s_res] = string[s_cur];
            s_res++;
        }
    }

    string[s_res] = '\0';

}

/*
 * returns true if string starts with sub-string beginning
 */
int string_starts_with(char * string, char * beginning) {

   if (strlen(string) > strlen(beginning))
       return (strncmp(string, beginning, strlen(beginning)) == 0);
   else
       return 0;
}

/*
 * convert strings like "90d" into nb of seconds
 * TODO: unit tests!
 */
uint32_t str_to_seconds(char * str_duration) {

    size_t last_char_idx = strlen(str_duration) -1 ;
    uint32_t multiplier = 1; /* determined by the unit at the end of the string */
    uint32_t unit_found = 0;
    uint32_t res = 0;

    switch (str_duration[last_char_idx]) {

        case 'w':
            multiplier = 86400*7;
            unit_found = 1;
            break;
        case 'd':
            multiplier = 86400;
            unit_found = 1;
            break;
        case 'h':
            multiplier = 3600;
            unit_found = 1;
            break;
        case 's':
            unit_found = 1;
        default:
            multiplier = 1;

    }

    if (unit_found)
        str_duration[last_char_idx] = '\0';

    res = strtoul(str_duration, NULL, 10);
    res *= multiplier;

    return res;

}

static retention_t * parse_retention_item_str(char *str_retention_item, char **saveptr) {

    char *time_per_point_str = NULL,
         *time_to_store_str = NULL;
    retention_t *retention_res = calloc(1, sizeof(retention_t));

    //printf("str_retention_item %s\n", str_retention_item);

    time_per_point_str = strtok_r(str_retention_item, ":", saveptr);
    retention_res->time_per_point = str_to_seconds(time_per_point_str);

    time_to_store_str = strtok_r(NULL, ":", saveptr);
    retention_res->time_to_store = str_to_seconds(time_to_store_str);

    retention_res->next = NULL;

    //printf("time_per_point: %lu time_to_store: %lu\n", time_per_point, time_to_store);

    return retention_res;

}

static retention_t * parse_retention_list_str(char *str_retention_list) {

    char *str_retention_item = NULL;
    char *pair_list = "pair_list",
         *pair_item = "pair_item";
    retention_t *retention_list = NULL,
                *cur_retention = NULL;
    char *tmp_str_retention_list = malloc(sizeof(char)*strlen(str_retention_list)+1);
    // keep a copy for free() since strtok() changes the position of the pointer
    char *tmp_str_save = tmp_str_retention_list;
 
    strncpy(tmp_str_retention_list, str_retention_list, strlen(str_retention_list)+1);

    str_retention_item = strtok_r(tmp_str_retention_list, ",", &pair_list);

    if (str_retention_item) {

        retention_list = parse_retention_item_str(str_retention_item, &pair_item);
        cur_retention = retention_list;

        while(str_retention_item) {

            str_retention_item = strtok_r(NULL, ",", &pair_list);

            if (str_retention_item) {

                cur_retention->next = parse_retention_item_str(str_retention_item, &pair_item);
                cur_retention = cur_retention->next;

            }
        }
    }

    free(tmp_str_save);

    return retention_list;

}

/*
 * Add new pattern_aggregation_t with values given in parameters at the end of
 * list conf->aggregation
 */
void conf_add_pattern_aggregation(char *pattern, float xff, aggregation_type_t method) {

   pattern_aggregation_t *agg = NULL;

   debug("adding new pattern aggregation: %s, %f, %d", pattern, xff, method);

   if (conf->aggregation) {
       // go to last pattern_aggregation_t
       agg = conf->aggregation;
       while(agg->next) agg = agg->next;
       agg->next = calloc(1, sizeof(pattern_aggregation_t));
       agg = agg->next;
   } else {
       conf->aggregation = calloc(1, sizeof(pattern_aggregation_t));
       agg = conf->aggregation;
   }

   // copy pattern string
   agg->pattern = malloc(sizeof(char)*strlen(pattern+1));
   memset(agg->pattern, 0, strlen(pattern)+1);
   strncpy(agg->pattern, pattern, strlen(pattern)+1);

   agg->xff = xff;
   agg->method = method; 
   agg->next = NULL;

}

/*
 * Returns the aggregation_type_t correspond to method name method_s
 */
aggregation_type_t conf_get_aggregation_method(const char *method_s) {

    if(strncmp(method_s, "average", 7) == 0)
        return AGG_TYPE_AVERAGE;
    else if(strncmp(method_s, "sum", 3) == 0)
        return AGG_TYPE_SUM;
    else if (strncmp(method_s, "last", 4) == 0)
        return AGG_TYPE_LAST;
    else if (strncmp(method_s, "max", 3) == 0)
        return AGG_TYPE_MAX;
    else if (strncmp(method_s, "min", 3) == 0)
        return AGG_TYPE_MIN;
    else
        return AGG_TYPE_UNDEF;

}

/*
 * Parses storage-aggregation.conf file in configuration directory and set
 * memory structures accordingly. Returns 0 on success, 1 on error.
 */
int conf_parse_storage_aggregation_file () {

    const char *filename = "storage-aggregation.conf";
    char filepath[PATH_MAX]; // abs path to file
    FILE *fh = NULL;

    char buffer[READ_BUF_MAX];
    bool pattern_found = false;

    char pattern[READ_BUF_MAX],
         xff_s[READ_BUF_MAX],
         method_s[READ_BUF_MAX];
    float xff = 0.0;
    aggregation_type_t method = AGG_TYPE_UNDEF;

    /*
     * make filepath contain the full absolute path to storage aggregation
     * configuration file
     */
    strncpy(filepath, conf->conf_dir, strlen(conf->conf_dir));
    // if path doesn't end with '/' add it.
    if (filepath[strlen(filepath)] != '/')
        strncat(filepath, "/", 1);
    strncat(filepath, filename, strlen(filename));

    debug("opening storage aggregation file %s", filepath);
    fh = fopen(filepath, "r");

    if (!fh) {
        error("error while opening file %s: %s\n",
              filepath, strerror(errno));
        return 1;
    }

    while (!feof(fh)) {

        if (fgets(buffer, READ_BUF_MAX-2, fh) == NULL)
            break;

        trim_string(buffer);

        if (!pattern_found) { 
            if (string_starts_with(buffer, "pattern=")) {
                pattern_found = true;
                sscanf(buffer,"pattern=%s", pattern);
            }
        } else {
            if (string_starts_with(buffer, "xFilesFactor=")) {
                sscanf(buffer,"xFilesFactor=%s", xff_s);
                xff = strtof(xff_s, NULL);
            } else if(string_starts_with(buffer, "aggregationMethod=")) {
                sscanf(buffer,"aggregationMethod=%s", method_s);
                method = conf_get_aggregation_method(method_s);
                pattern_found = false;
            }

            /* At this point, if pattern_found is false, all parameters have
             * been found for new pattern_aggregation_t
             */
            if(!pattern_found) {
                conf_add_pattern_aggregation(pattern, xff, method);
            }

        }

    }

    fclose(fh);

    return 0;

}

/*
 * Parses storage-schemas.conf file in configuration directory and set
 * memory structures accordingly. Returns 0 on success, 1 on error.
 */
int conf_parse_storage_schema_file() {

    const char *filename = "storage-schemas.conf";
    char filepath[PATH_MAX];
    FILE *fh = NULL;
    char buffer[READ_BUF_MAX];

    bool pattern_found = false;
    pattern_retention_t *cur_pattern_retention = NULL,
                        *first = NULL;

    // initialize allocated str
    memset(filepath, 0, sizeof(char)*PATH_MAX);

    /*
     * make str_sch_filepath contain the full absolute path to storage schema
     * configuration file
     */
    strncpy(filepath, conf->conf_dir, strlen(conf->conf_dir));
    if (filepath[strlen(filepath)] != '/')
        strncat(filepath, "/", 1);
    strncat(filepath, filename, strlen(filename));

    debug("opening storage schema file %s", filepath);
    fh = fopen(filepath, "r");

    if (!fh) {
        error("error while opening file %s: %s\n",
              filepath, strerror(errno));
        return 1;
    }

    while (!feof(fh)) {
        if (fgets(buffer, READ_BUF_MAX-2, fh) == NULL)
            break;

        trim_string(buffer);

        if (!pattern_found) { 
            if (string_starts_with(buffer, "pattern=")) {
                pattern_found = true;

                if (!first) {
                    first = calloc(1, sizeof(pattern_retention_t));
                    cur_pattern_retention = first;
                    conf->schema = first;
                } else {
                    cur_pattern_retention->next = calloc(1, sizeof(pattern_retention_t));
                    cur_pattern_retention = cur_pattern_retention->next;
                }
                cur_pattern_retention->str_pattern = malloc(sizeof(char) * PATTERN_MAX);
                cur_pattern_retention->next = NULL;
                sscanf(buffer,"pattern=%s", cur_pattern_retention->str_pattern);
            }
        } else {
            if (string_starts_with(buffer, "retentions=")) {
                pattern_found = false;
                cur_pattern_retention->str_retention = malloc(sizeof(char) * RETENTION_MAX);
                sscanf(buffer,"retentions=%s", cur_pattern_retention->str_retention);
                cur_pattern_retention->retention_list = parse_retention_list_str(cur_pattern_retention->str_retention);
            }
        }
    }

    fclose(fh);

    return 0;

}

/*
 * Parses carbon.conf file and conf members accordingly.
 * Returns 0 on success, 1 on error.
 */
int conf_parse_carbon_file() {

    FILE *conf_fh = NULL;
    char *read_buffer = malloc(sizeof(char) * READ_BUF_MAX);
    char *cnf_key = NULL,
         *cnf_val = NULL,
         *saveptr = NULL;

    memset(read_buffer, 0, sizeof(char)*READ_BUF_MAX);

    conf_fh = fopen(conf->conf_file, "r");

    if (!conf_fh) {
        error("error while opening file %s: %s\n", conf->conf_file, strerror(errno));
        return 1;
    }

    while (!feof(conf_fh)) {
        if (fgets(read_buffer, READ_BUF_MAX-2, conf_fh) == NULL)
            break;

        trim_string(read_buffer);
        cnf_key = strtok_r(read_buffer,"=", &saveptr);

        if (cnf_key)
            cnf_val = strtok_r(NULL, "=", &saveptr);

        if (cnf_key && cnf_val) {

            if (strncmp(cnf_key, "CONF_DIR", 8) == 0) {
                free(conf->conf_dir);
                conf->conf_dir = malloc(sizeof(char)*PATH_MAX);
                memset(conf->conf_dir, 0, sizeof(char)*PATH_MAX);
                strncpy(conf->conf_dir, cnf_val, strlen(cnf_val));
            }

            else if (strncmp(cnf_key, "STORAGE_DIR", 11) == 0) {
                free(conf->storage_dir);
                conf->storage_dir = malloc(sizeof(char)*PATH_MAX);
                memset(conf->storage_dir, 0, sizeof(char)*PATH_MAX);
                strncpy(conf->storage_dir, cnf_val, strlen(cnf_val));
            }

            else if (strncmp(cnf_key, "LINE_RECEIVER_PORT", 18) == 0) {
                errno = 0;
                conf->line_receiver_port = strtol(cnf_val, NULL, 10);
                if (errno)
                    switch(errno) {
                        case EINVAL:
                        case ERANGE:
                            error("problem while setting LINE_RECEIVER_PORT: %s\n", strerror(errno));
                            return 1;
                    }
            }

            else if (strncmp(cnf_key, "UDP_RECEIVER_PORT", 17) == 0) {
                errno = 0;
                conf->udp_receiver_port = strtol(cnf_val, NULL, 10);
                if (errno)
                    switch(errno) {
                        case EINVAL:
                        case ERANGE:
                            error("problem while setting UDP_RECEIVER_PORT: %s\n", strerror(errno));
                            return 1;
                    }
            }

            else {
                error("conf: unknown key in configuration file: %s\n", cnf_key);
                return 1;
            }

        }
        cnf_key = cnf_val = NULL;
    }

    fclose(conf_fh);
    free(read_buffer);

    return 0;

}

/*
 * prints to stdout the linked list of pattern_retention_t with all their
 * retention_t members
 */
void print_storage_schema() {

    const pattern_retention_t * cur_pt = conf->schema;
    const retention_t * cur_rt = NULL;

    while(cur_pt) {

        debug("pattern: %s retention: %s",
               cur_pt->str_pattern,
               cur_pt->str_retention);

        cur_rt = cur_pt->retention_list;

        while(cur_rt) {

            debug("  time_per_point: %u time_to_store: %u",
                   cur_rt->time_per_point,
                   cur_rt->time_to_store);

            cur_rt = cur_rt->next; // next retention_t
        }

        cur_pt = cur_pt->next; // next pattern_retention_t
    }

}
