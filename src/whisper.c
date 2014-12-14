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
#include <stdint.h>    // fixed width integers
#include <fcntl.h>     // for O_RDONLY
#include <stdio.h>     // *printf
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h> // SEEK_SET
#include <unistd.h>    // lseek()
#include <arpa/inet.h> // ntohl()
#include <string.h>    // strerror()
#include <inttypes.h>  // PRIu64, etc
#include <sys/types.h> // stat()
#include <sys/stat.h>  // stat()
#include <assert.h>
#include <pcre.h>
#include <pthread.h>   // pthread_mutex_[un]lock()
#include "common.h"
#include "whisper.h"


static inline void hton_whisper_metadata(whisper_metadata_t *wsp_md) {

    uint32_t tmp_value = 0,
            *tmp_value_p = NULL;

    tmp_value = wsp_md->aggregation_type;
    wsp_md->aggregation_type = htonl(tmp_value);
    tmp_value = wsp_md->max_retention;
    wsp_md->max_retention = htonl(tmp_value);
    tmp_value = wsp_md->archive_count;
    wsp_md->archive_count = htonl(tmp_value);

    tmp_value_p = (uint32_t *)&wsp_md->x_files_factor;
    tmp_value = htonl(*tmp_value_p);
    tmp_value_p = &tmp_value;
    wsp_md->x_files_factor = *(float *)tmp_value_p;

}

static inline void ntoh_whisper_metadata(whisper_metadata_t *wsp_md) {

    uint32_t tmp_value = 0,
            *tmp_value_p = NULL;

    tmp_value = wsp_md->aggregation_type;
    wsp_md->aggregation_type = ntohl(tmp_value);
    tmp_value = wsp_md->max_retention;
    wsp_md->max_retention = ntohl(tmp_value);
    tmp_value = wsp_md->archive_count;
    wsp_md->archive_count = ntohl(tmp_value);

    tmp_value_p = (uint32_t *)&wsp_md->x_files_factor;
    tmp_value = ntohl(*tmp_value_p);
    tmp_value_p = &tmp_value;
    wsp_md->x_files_factor = *(float *)tmp_value_p;

}

static inline void hton_archive_info(archive_info_t *arch_info) {

    uint32_t tmp_value = 0;

    tmp_value = arch_info->offset;
    arch_info->offset = htonl(tmp_value);
    tmp_value = arch_info->seconds_per_point;
    arch_info->seconds_per_point = htonl(tmp_value);
    tmp_value = arch_info->points;
    arch_info->points = htonl(tmp_value);

}

static inline void ntoh_archive_info(archive_info_t *arch_info) {

    uint32_t tmp_value = 0;

    tmp_value = arch_info->offset;
    arch_info->offset = ntohl(tmp_value);
    tmp_value = arch_info->seconds_per_point;
    arch_info->seconds_per_point = ntohl(tmp_value);
    tmp_value = arch_info->points;
    arch_info->points = ntohl(tmp_value);

}

static inline void hton_archive_point(archive_point_t *arch_pt) {

    uint32_t tmp_value = 0;
    uint64_t tmp_value64 = 0,
            *tmp_value64_p = NULL;

    tmp_value = arch_pt->timestamp;
    arch_pt->timestamp = htonl(tmp_value);

    // Use temporary variable to avoid strict-aliasing warning raised by this code:
    // tmp_value64 = htobe64(*(uint64_t *)&arch_pt->value);
    // arch_pt->value = *(double *)&tmp_value64;
    tmp_value64_p = (uint64_t *)&arch_pt->value;
    tmp_value64 = htobe64(*tmp_value64_p);
    tmp_value64_p = &tmp_value64;
    arch_pt->value = *(double *)tmp_value64_p;

}

static inline void ntoh_archive_point(archive_point_t *arch_pt) {

    uint32_t tmp_value = 0;
    uint64_t tmp_value64 = 0,
            *tmp_value64_p = NULL;

    tmp_value = arch_pt->timestamp;
    arch_pt->timestamp = ntohl(tmp_value);

    tmp_value64_p = (uint64_t *)&arch_pt->value;
    tmp_value64 = be64toh(*tmp_value64_p);
    tmp_value64_p = &tmp_value64;
    arch_pt->value = *(double *)tmp_value64_p;

}

/*
 * Returns the total retention time in seconds of an archive in bytes
 */

static inline uint64_t archive_retention(archive_info_t *arch_info) {

    return arch_info->seconds_per_point * arch_info->points;

}

/*
 * Returns the size of an archive in bytes
 */

static inline uint64_t archive_size(archive_info_t *arch_info) {

    return arch_info->points * WHISPER_POINT_SIZE;

}

/*
 * Returns the last offset of an archive
 */

static inline uint32_t archive_offset_end(archive_info_t *arch_info) {

    return arch_info->offset + archive_size(arch_info);

}

/*
 * Seek to specific offset in a whisper file
 * Returns 1 if failure
 *         0 if success
 */

static inline int whisper_seek(int whisper_fd, int offset) {

    if (lseek(whisper_fd, offset, SEEK_SET) < 0) {
        error("error while seeking file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
 * Seek to beginning of whisper file and read archive_info_t.
 * Returns NULL if any error is encountered.
 */

static whisper_metadata_t * whisper_read_metadata(int whisper_fd) {

    whisper_metadata_t *wsp_md = NULL;

    if (whisper_seek(whisper_fd, 0))
        return NULL;

    wsp_md = calloc(1, WHISPER_HEADER_SIZE);

    if(read(whisper_fd, wsp_md, WHISPER_HEADER_SIZE) < 0) {
        error("error while reading file: %s\n", strerror(errno));
        free(wsp_md);
        wsp_md = NULL;
        return NULL;
    }

    ntoh_whisper_metadata(wsp_md);

    return wsp_md;

}

/*
 * Seek to archive header position in file and read archive_info_t.
 * Returns NULL if any error is encountered.
 */

static archive_info_t * whisper_read_archive_info(int whisper_fd, int archive_id) {

    archive_info_t *arch_info = NULL;

    int seek_point = WHISPER_HEADER_SIZE + archive_id * WHISPER_ARCHIVE_SIZE;

    if (whisper_seek(whisper_fd, seek_point))
        return NULL;

    arch_info = calloc(1, WHISPER_ARCHIVE_SIZE);

    if (read(whisper_fd, arch_info, WHISPER_ARCHIVE_SIZE) < 0) {
        error("error while reading file: %s\n", strerror(errno));
        free(arch_info);
        return NULL;
    }

    ntoh_archive_info(arch_info);

    return arch_info;

}

/*
 * Read archive point w/o seeking
 */

static archive_point_t * whisper_read_archive_point(int whisper_fd) {

    archive_point_t *arch_pt = calloc(1, WHISPER_POINT_SIZE);

    if (read(whisper_fd, &(arch_pt->timestamp), sizeof(uint32_t)) < 0) {
        error("error while reading file: %s\n", strerror(errno));
        return NULL;
    }

    if (read(whisper_fd, &(arch_pt->value), sizeof(double)) < 0) {
        error("error while reading file: %s\n", strerror(errno));
        return NULL;
    }

    ntoh_archive_point(arch_pt);

    return arch_pt;

}

/*
 * Tests if pattern matches str. Returns:
 *   - 0 if matches
 *   - 1 if not
 *   - 2 on regex compilation error
 */
static int whisper_pattern_match(const char *pattern, const char *str) {

    pcre * re = NULL;
    int erroffset = -1;
    int rc = -1; // result of pcre_exec()
    const char *errmsg;
    const int OVECCOUNT = 30; // according to pcresample, should be a multiple of 3
    int ovector[OVECCOUNT];

    re = pcre_compile(pattern, 0, &errmsg, &erroffset, NULL);

    /* Compilation failed: print the error message and exit */
    if (re == NULL) {
        error("whisper: PCRE compilation failed at offset %d: %s\n", erroffset, errmsg);
        return 2;
    }

    rc = pcre_exec(re, NULL, str, strlen(str), 0, 0, ovector, OVECCOUNT);

    if (rc >= 0) {
       debug("metric %s matches %s", str, pattern);
       pcre_free(re);
       return 0;
    }

    else if (rc != PCRE_ERROR_NOMATCH) {
       error("whisper: matching error %d\n", rc);
       pcre_free(re);
       return 2;
    }

    return 1;
}

/*
 * Find the pattern_retention_t in list conf->conf with pattern that
 * matches metric name and returns its retention_t list.
 * Returns NULL if not found or on error.
 */
static retention_t * whisper_find_retention(const metric_t *metric) {

    const pattern_retention_t *cur_patret = conf->schema;
    int match = 0;

    while(cur_patret) {

        match = whisper_pattern_match(cur_patret->str_pattern, metric->name);

        if(match == 0) return cur_patret->retention_list; // match
        else if (match == 2) return NULL; // pattern compilation error, abort.

        cur_patret = cur_patret->next;
    }

    // None found!
    return NULL;

}

/*
 * Find the pattern_aggregation_t in list conf->aggregation with pattern that
 * matches metric name.
 * Returns NULL if not found or on error.
 */
static pattern_aggregation_t * whisper_find_aggregation(const metric_t *metric) {

    pattern_aggregation_t *agg = conf->aggregation;
    int match = 0;

    while(agg) {

        match = whisper_pattern_match(agg->pattern, metric->name);

        if(match == 0) {
            debug("pattern aggregation found for metric %s: pattern: %s, "
                  "xff: %f, method: %d", metric->name, agg->pattern, agg->xff,
                  agg->method);
            return agg;
        } else if (match == 2)
            return NULL; // pattern compilation error, abort.

        agg = agg->next;

    }

    // None found!
    debug("pattern aggregation not found for metric %s", metric->name);
    return NULL;

}

/*
 * creates directory if not exists
 */
static void create_dir(char * dir_name) {

    int stat_value = 0;
    struct stat s;

    stat_value = stat(dir_name, &s);

    if(-1 == stat_value) {
        if(ENOENT == errno) {
            debug("create database directory: %s", dir_name);
            mkdir(dir_name, S_IRWXU | S_IRWXG); /* 770 */
        } else {
            error("error during stat: %s\n", strerror(errno));
            exit(1);
        }
    } else {
        if(!S_ISDIR(s.st_mode)) { /* exists but not a directory */
            error("%s exists but is not a directory!\n", dir_name);
            exit(1);
        }
    }

}

static char * whisper_metric_filename(const metric_t *metric) {

    char metric_name_cpy[METRIC_NAME_MAX_LEN];
    char *metric_substr_next,
         *metric_substr_last,
         *saveptr;
    char *tmp_dir_name = malloc(sizeof(char)*PATH_MAX);

    char *res_filename = malloc(sizeof(char)*PATH_MAX);
    memset(res_filename, 0, sizeof(char)*PATH_MAX);

    strncpy(tmp_dir_name, conf->storage_dir, strlen(conf->storage_dir)+1);
    /* TODO: handle relative path with:
    if (realpath(".", tmp_dir_name) == NULL) {
        error("error during realpath(): %s\n", strerror(errno));
    }
    */

    strncpy(metric_name_cpy, metric->name, METRIC_NAME_MAX_LEN);
    metric_substr_next = strtok_r(metric_name_cpy, ".", &saveptr);
    metric_substr_last = metric_substr_next;

    while(metric_substr_next) {

        strncat(tmp_dir_name, "/", 1);
        strncat(tmp_dir_name, metric_substr_last, strlen(metric_substr_last));

        metric_substr_next = strtok_r(NULL, ".", &saveptr);

        if(metric_substr_next == NULL) { /* last is the filename */
            strncpy(res_filename, tmp_dir_name, strlen(tmp_dir_name)); 
            strncat(res_filename, ".wsp", 4);
        }

        metric_substr_last = metric_substr_next;
    }

    free(tmp_dir_name);

    return res_filename;

}

static void whisper_create_dirs(const metric_t *metric) {

    char metric_name_cpy[METRIC_NAME_MAX_LEN];
    char * metric_substr_next, * metric_substr_last, * saveptr;
    char * tmp_dir_name = malloc(sizeof(char) * PATH_MAX);

    strncpy(tmp_dir_name, conf->storage_dir, strlen(conf->storage_dir)+1);
    /* TODO: handle relative path with:
    if (realpath(".", tmp_dir_name) == NULL) {
        error("error during realpath(): %s\n", strerror(errno));
    }
    */

    create_dir(tmp_dir_name);

    strncpy(metric_name_cpy, metric->name, METRIC_NAME_MAX_LEN);
    metric_substr_next = strtok_r(metric_name_cpy, ".", &saveptr);
    metric_substr_last = metric_substr_next;

    while(metric_substr_next) {

        strncat(tmp_dir_name, "/", 1);
        strncat(tmp_dir_name, metric_substr_last, strlen(metric_substr_last));

        metric_substr_next = strtok_r(NULL, ".", &saveptr);

        if(metric_substr_next != NULL) {
            /* there is a non-null string after last, last is a dir */
            create_dir(tmp_dir_name);
        }

        metric_substr_last = metric_substr_next;
    }

    free(tmp_dir_name);

}
static uint32_t whisper_get_archive_offset(const retention_t *retention_list,
                                           const int id_ret) {

    const retention_t *cur_ret = NULL;
    int loop_id = 0; 
    uint32_t offset = WHISPER_HEADER_SIZE;

    for (cur_ret=retention_list; cur_ret; cur_ret=cur_ret->next)
        offset += WHISPER_ARCHIVE_SIZE;

    for (cur_ret=retention_list, loop_id=0; loop_id<id_ret; loop_id++, cur_ret=cur_ret->next) {
        offset += (cur_ret->time_to_store / cur_ret->time_per_point)
                  * WHISPER_POINT_SIZE;
    }

    debug("offset of arch %d: %" PRIu32 "", id_ret, offset);

    return offset;
}

/*
 * Returns the timestamp of the first in the list of point in the higher
 * precision archive to aggregate for the point at timestamp in lower precision
 * archive.
 *
 * Ex. with sampling of lower in 4 times bigger than higher, timestamp is on l2:
 *
 *                                 v timestamp given in parameter
 * lower :           l1 |          l2
 * higher:  h0 h1 h2 h3 | h4 h5 h6 h7
 *                        ^ timestamp returned
 * 
 */
uint32_t whisper_higher_archive_timestamp_start(uint32_t timestamp, 
                                                archive_info_t *higher,
                                                archive_info_t *lower) {

    return timestamp 
           - lower->seconds_per_point
           + higher->seconds_per_point;
}

static int whisper_create_file(const metric_t *metric) {

    retention_t *ret = whisper_find_retention(metric);
    retention_t *cur_ret = NULL;

    int whisper_fd = -1;
    int id_ret = 0;
    uint32_t nb_points = 0;
    size_t sizeof_arch = 0;

    whisper_metadata_t new_wsp_md;
    archive_info_t wsp_cur_arch;
    archive_point_t *empty_arch = NULL;

    pattern_aggregation_t *agg = NULL;

    uint32_t nb_arch = 0;
    uint32_t max_retention = 0;
    char *filename = NULL;

    filename = whisper_metric_filename(metric);
    whisper_create_dirs(metric);

    // count nb of archs
    for(cur_ret=ret; cur_ret; cur_ret=cur_ret->next, nb_arch++)
        if (cur_ret->time_to_store  > max_retention)
            max_retention = cur_ret->time_to_store;

    agg = whisper_find_aggregation(metric);
    // return here if pattern_aggregation_t not found
    if(!agg) return -1;

    new_wsp_md.aggregation_type = agg->method;
    new_wsp_md.max_retention = max_retention;
    new_wsp_md.x_files_factor = agg->xff;
    new_wsp_md.archive_count = nb_arch;

    debug("creating file %s", filename);
    whisper_fd = open(filename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if(whisper_fd == -1) {
        error("failed to open file: %s\n", strerror(errno));
        return -1;
    }

    hton_whisper_metadata(&new_wsp_md);

    debug("writing whisper headers in file");
    if (write(whisper_fd, &new_wsp_md, WHISPER_HEADER_SIZE) < WHISPER_HEADER_SIZE) {
        error("error while writing file: %s\n", strerror(errno));
        return -1;
    }

    // restart from beginning of retentions list to write archive headers
    cur_ret = ret;
    while(cur_ret) {

        nb_points = cur_ret->time_to_store / cur_ret->time_per_point;
        wsp_cur_arch.offset = whisper_get_archive_offset(ret, id_ret);
        wsp_cur_arch.seconds_per_point = cur_ret->time_per_point;
        wsp_cur_arch.points = nb_points;

        hton_archive_info(&wsp_cur_arch);
        debug("writing archive %d header in file", id_ret);
        if (write(whisper_fd, &wsp_cur_arch, WHISPER_ARCHIVE_SIZE) < WHISPER_ARCHIVE_SIZE) {
            error("error while writing file: %s\n", strerror(errno));
            return -1;
        }

        id_ret++;
        cur_ret = cur_ret->next;

    }

    // restart from beginning of retentions list to write empty archive points
    cur_ret = ret;
    while(cur_ret) {

        nb_points = cur_ret->time_to_store / cur_ret->time_per_point;
        sizeof_arch = nb_points * WHISPER_POINT_SIZE;
        empty_arch = calloc(nb_points, WHISPER_POINT_SIZE);

        debug("whisper: writing empty archive in file (%" PRIu32 " points, %lu bytes)",
               nb_points, sizeof_arch);

        if (write(whisper_fd, empty_arch, sizeof_arch) < sizeof_arch) {
            error("error while writing file: %s\n", strerror(errno));
            return -1;
        }

        free(empty_arch);
        cur_ret = cur_ret->next;

    }

    free(filename);

    return whisper_fd;

}

static double whisper_aggregate_values(double *aggregated_values,
                                       uint32_t nb_known_points,
                                       uint32_t aggregation_type) {

    double value = 0.0;
    int i = 0;

    switch(aggregation_type) {

        case AGG_TYPE_AVERAGE:
            for(i=0; i<nb_known_points; i++) value += aggregated_values[i];
            value /= nb_known_points;
            break;
        case AGG_TYPE_SUM:
            for(i=0; i<nb_known_points; i++) value += aggregated_values[i];
            break;
        default:
            debug("unhandled aggregation type %" PRIu32 "", aggregation_type);

    }

    return value;
}

/*
 * Write point in archive of whisper_fd at proper offset according to timestamp.
 */
int whisper_write_point(int whisper_fd, archive_info_t *archive, uint32_t timestamp, archive_point_t point) {

    archive_point_t *first_arch_pt = NULL;
    uint32_t time_distance = 0;
    // offset of lower precision archive
    uint32_t write_offset = 0;

    // goto first point of archive
    if (whisper_seek(whisper_fd, archive->offset))
        return 1;

    // read first archive point
    first_arch_pt = whisper_read_archive_point(whisper_fd);

    debug("timestamp of first point: %" PRIu32 "", first_arch_pt->timestamp);
    // check if first update
    if(first_arch_pt->timestamp == 0) {
        write_offset = archive->offset;
        debug("whisper: computed write offset: %" PRIu32 " (0)", write_offset);
    }
    else {
        // calculate distance in number of metrics points
        time_distance = (timestamp - first_arch_pt->timestamp) / archive->seconds_per_point;
        write_offset = archive->offset + (time_distance * WHISPER_POINT_SIZE % archive_size(archive));
        debug("whisper: computed write offset: %" PRIu32 " (%" PRIu32 ")",
              write_offset,
              (write_offset - archive->offset)/WHISPER_POINT_SIZE);
    }

    // goto write offset
    if (lseek(whisper_fd, write_offset , SEEK_SET) < 0) {
        error("error while seeking file: %s\n", strerror(errno));
        return 1;
    }

    // write point    
    if (write(whisper_fd, &point, WHISPER_POINT_SIZE) < WHISPER_POINT_SIZE) {
        error("error while writing file: %s\n", strerror(errno));
        return 1;
    }

    // update internal monitoring data
    pthread_mutex_lock(&(monitoring->mutex_points));
    monitoring->points++;
    pthread_mutex_unlock(&(monitoring->mutex_points));

    return 0;

}

static int whisper_write_propagate(int whisper_fd, uint32_t timestamp,
                                   whisper_metadata_t *wsp_md,
                                   archive_info_t *wsp_arch_higher,
                                   archive_info_t *wsp_arch_lower) {

    archive_point_t *first_higher_point = NULL,
                    *tmp_point = NULL;
    // for later usage
    //archive_point_t * first_lower_point = NULL;
    int rd_len = -1,
        rd_len2 = -1,
        higher_point_id = -1;
    double new_value = 0.0;
    uint32_t points_distance = 0;
    uint32_t higher_start_offset = 0,
             higher_end_offset = 0;
    uint32_t nb_higher_points = 0,
             nb_known_points = 0,
             cur_timestamp = 0;


    void * rd_buf = malloc(archive_size(wsp_arch_higher));
    double agregated_values[wsp_arch_higher->points];

    archive_point_t new_arch_pt;

    memset(rd_buf, 0, archive_size(wsp_arch_higher));

    /* determine read interval in higher precision archive */

    // goto first point of higher precision archive to get its timestamp
    whisper_seek(whisper_fd, wsp_arch_higher->offset);
    first_higher_point = whisper_read_archive_point(whisper_fd);

    // calculate the number of points between the first and the one with good timestamp
    points_distance = (whisper_higher_archive_timestamp_start(timestamp, wsp_arch_higher, wsp_arch_lower)
                       - first_higher_point->timestamp )
                      / wsp_arch_higher->seconds_per_point;

    higher_start_offset = wsp_arch_higher->offset +
                          ((points_distance * WHISPER_POINT_SIZE)
                           % archive_size(wsp_arch_higher));

    nb_higher_points = wsp_arch_lower->seconds_per_point /
                       wsp_arch_higher->seconds_per_point;

    higher_end_offset = (higher_start_offset +
                         nb_higher_points * WHISPER_POINT_SIZE)
                        % archive_size(wsp_arch_higher);

    if (higher_start_offset < higher_end_offset) {
        whisper_seek(whisper_fd, higher_start_offset);
        rd_len = higher_end_offset - higher_start_offset;
        debug("reading from %d->%d", higher_start_offset, higher_end_offset);
        read(whisper_fd, rd_buf, higher_end_offset - higher_start_offset);
    }
    else {
        whisper_seek(whisper_fd, higher_start_offset);
        rd_len = archive_offset_end(wsp_arch_higher) - higher_start_offset;
        debug("reading from %d->%d + %d->%d",
              higher_start_offset,
              archive_offset_end(wsp_arch_higher),
              wsp_arch_higher->offset,
              higher_end_offset);
        if (read(whisper_fd, rd_buf, rd_len) != rd_len) {
            error("error while reading file: %s\n", strerror(errno));
        }
        whisper_seek(whisper_fd, wsp_arch_higher->offset);
        rd_len2 = wsp_arch_higher->offset - higher_end_offset;
        if (read(whisper_fd, rd_buf+rd_len, rd_len2) != rd_len2) {
            error("error while reading file: %s\n", strerror(errno));
        }
        rd_len += rd_len2;
    }

    nb_known_points = 0;
    cur_timestamp = whisper_higher_archive_timestamp_start(timestamp, wsp_arch_higher, wsp_arch_lower);

    for(higher_point_id=0; higher_point_id < nb_higher_points; higher_point_id++) {
        tmp_point = ((archive_point_t *)rd_buf) + higher_point_id;
        ntoh_archive_point(tmp_point);

        if (tmp_point->timestamp == cur_timestamp)
            agregated_values[nb_known_points++] = tmp_point->value;
        
        cur_timestamp += wsp_arch_higher->seconds_per_point;
    }

    // print agregated values
    /*
    printf("agregated_values: ");
    for(higher_point_id=0; higher_point_id < nb_known_values; higher_point_id++) {
        printf("%f ", agregated_values[higher_point_id]);
    }
    printf("\n");
    */

    if (nb_known_points / nb_higher_points >= wsp_md->x_files_factor) {

        new_value = whisper_aggregate_values(agregated_values, 
                                             nb_known_points,
                                             wsp_md->aggregation_type);
        // write new value
        debug("write value %f with timestamp %" PRIu32 "", new_value, timestamp);
        new_arch_pt.timestamp = timestamp;
        new_arch_pt.value = new_value;
        hton_archive_point(&new_arch_pt);

        whisper_write_point(whisper_fd, wsp_arch_lower, timestamp, new_arch_pt);

    }
    else
        debug("known values (%" PRIu32 ") below xff", nb_known_points);

    free(first_higher_point);
    free(rd_buf);

    return EXIT_SUCCESS;

}

int whisper_write_value(const metric_t * metric,
                        uint32_t timestamp, double value) {

    int whisper_fd = -1;
    int archive_id = 0;
    uint32_t aligned_timestamp = 0;
    bool end_loop = false; // flag for propagation loop

    whisper_metadata_t *wsp_md = NULL;
    archive_info_t *wsp_arch = NULL;
    archive_info_t *wsp_arch_higher = NULL, // for propagation
                   *wsp_arch_lower = NULL;
    archive_point_t *first_arch_pt = NULL;
    archive_point_t new_arch_pt;
    char *filename = whisper_metric_filename(metric);

    debug("whisper: opening file %s", filename);
    whisper_fd = open(filename, O_RDWR);

    if (whisper_fd < 0) {
        switch(errno) {
            case ENOENT:
                whisper_fd = whisper_create_file(metric);
                break;
            default:
                error("error while opening file: %s\n", strerror(errno));
                return EXIT_FAILURE;
        }
    }

    wsp_md = whisper_read_metadata(whisper_fd);

    if (wsp_md == NULL)
        return EXIT_FAILURE;

    /* TODO: check timestamp < wsp max retention of the highest precision archive */
    wsp_arch = whisper_read_archive_info(whisper_fd, 0);

    if (wsp_arch == NULL)
        return EXIT_FAILURE;

    /*
     * Align timestamp to archive sampling rate.
     * for example, with:
     *   - timestamp: 188
     *   - seconds_per_point: 60
     * arch_pt.timestamp = 188 - (188 % 60)
     *                   = 188 - 8
     *                   = 180
     */
    aligned_timestamp = timestamp - (timestamp % wsp_arch->seconds_per_point);
    new_arch_pt.timestamp = aligned_timestamp;
    new_arch_pt.value = value;
    //printf("whisper: before %#010" PRIx32" %#018" PRIx64 "\n", new_arch_pt.timestamp, *(int64_t *)&new_arch_pt.value);
    hton_archive_point(&new_arch_pt);
    //printf("whisper: after  %#010" PRIx32" %#018" PRIx64 "\n", new_arch_pt.timestamp, *(int64_t *)&new_arch_pt.value);

    whisper_write_point(whisper_fd, wsp_arch, aligned_timestamp, new_arch_pt);

    // propogation to lower precision archives
    wsp_arch_higher = wsp_arch;
    end_loop = false;

    for(archive_id=1; !end_loop && archive_id < wsp_md->archive_count; archive_id++) {
        wsp_arch_lower = whisper_read_archive_info(whisper_fd, archive_id);

        assert(wsp_arch_lower->seconds_per_point != 0);

        // only if timestamp can be divided by lower archive seconds per point
        if (aligned_timestamp % wsp_arch_lower->seconds_per_point == 0) {
            debug("propagate to archive %d", archive_id);
            whisper_write_propagate(whisper_fd, aligned_timestamp, wsp_md, wsp_arch_higher, wsp_arch_lower);
        }
        else {
            debug("end loop at archive %d", archive_id);
            end_loop = true;
        }
        if(wsp_arch_higher != wsp_arch)
            free(wsp_arch_higher);
        wsp_arch_higher = wsp_arch_lower;

    }

    free(wsp_arch_lower);
    free(wsp_md);
    free(wsp_arch);
    free(first_arch_pt);
    free(filename);

    close(whisper_fd);

    debug("end writing value %f at timestamp %" PRIu32 " (%" PRIu32 ")",
           value, timestamp, aligned_timestamp);

    return EXIT_SUCCESS;

}

void whisper_print_file(const char * filename) {

    int whisper_fd = -1;
    int sk_res = -1,
        archive_id = 0,
        point_id = 0;
    uint32_t loop_offset = 0;
    whisper_metadata_t *wsp_md = NULL;
    archive_info_t *arch_info = NULL;
    archive_point_t *arch_pt = NULL;

    whisper_fd = open(filename, O_RDONLY);
    wsp_md = whisper_read_metadata(whisper_fd);

    printf("whisper file: %s\n", filename);
    printf("  aggregation_type: %" PRIu32 " max_retention: %" PRIu32 "  x_files_factor: %f archive_count: %" PRIu32 "\n",
           wsp_md->aggregation_type,
           wsp_md->max_retention,
           wsp_md->x_files_factor,
           wsp_md->archive_count);

    /*
     * now read all archives info
     */
    for(archive_id=0; archive_id < wsp_md->archive_count; archive_id++) {

        arch_info = whisper_read_archive_info(whisper_fd, archive_id);

        printf("  archive %d: offset: %" PRIu32 " seconds_per_point: %" PRIu32 " points: %" PRIu32 "\n",
               archive_id, arch_info->offset, arch_info->seconds_per_point, arch_info->points);

        /*
         * get all the data of the archive
         */

        loop_offset = arch_info->offset;

        sk_res = lseek(whisper_fd, loop_offset , SEEK_SET);
        if (sk_res < 0) {
            error("error while seeking file: %s\n", strerror(errno));
        }

        for(point_id=0; point_id<arch_info->points; point_id++) {

            arch_pt = whisper_read_archive_point(whisper_fd);

            printf("  point %-5d: offset: %-5" PRIu32 " timestamp: %" PRIu32 " value: %f",
                   point_id, loop_offset, arch_pt->timestamp, arch_pt->value);

            loop_offset += WHISPER_POINT_SIZE;

        }
    }

    close(whisper_fd);

}

void check_whisper_sizes() {

    debug("checking whisper data structures sizes");
    assert(sizeof(whisper_metadata_t) == WHISPER_HEADER_SIZE);
    assert(sizeof(archive_info_t) == WHISPER_ARCHIVE_SIZE);
    assert(sizeof(archive_point_t) == WHISPER_POINT_SIZE);

}
