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

#ifndef _WHISPER_H
#define _WHISPER_H

#define WHISPER_HEADER_SIZE 16
#define WHISPER_ARCHIVE_SIZE 12
#define WHISPER_POINT_SIZE 12

struct whisper_metadata_s {
    uint32_t aggregation_type;
    uint32_t max_retention;
    float x_files_factor;
    uint32_t archive_count;
};

typedef struct whisper_metadata_s whisper_metadata_t;

struct archive_info_s {
    uint32_t offset;
    uint32_t seconds_per_point;
    uint32_t points;
};

typedef struct archive_info_s archive_info_t;

struct archive_point_s {
    uint32_t timestamp;
    double value;
} __attribute__ ((packed)); // avoid padding

typedef struct archive_point_s archive_point_t;

int whisper_write_value(const metric_t *, uint32_t, double);
void check_whisper_sizes();

#endif
