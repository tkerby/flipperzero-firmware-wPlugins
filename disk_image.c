/* 
 * This file is part of the 8-bit ATARI FDD Emulator for Flipper Zero 
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "disk_image.h"

#include "app_common.h"

#define ATR_FILE_HEADER_SIZE 16

struct DiskImage {
    // Path to the ATR file
    FuriString* path;
    // ATR file header
    uint8_t header[ATR_FILE_HEADER_SIZE];
    // File handle
    File* file;
    // Disk size in bytes
    size_t disk_size;
    // Sector size in bytes
    size_t sector_size;
    // Disk configuration
    DiskGeometry geometry;
};

static bool guess_disk_geometry(DiskGeometry* geom, size_t disk_size, size_t sector_size);

#define ERROR_TITLE "Image not loaded"

DiskImage* disk_image_open(const char* path, Storage* storage) {
    DiskImage* image = (DiskImage*)malloc(sizeof(DiskImage));

    image->file = storage_file_alloc(storage);

    FURI_LOG_I(TAG, "Opening ATR file %s", path);

    image->path = furi_string_alloc_set(path);

    if(!storage_file_open(image->file, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        set_last_error(ERROR_TITLE, "Failed to open file %s", path);
        goto cleanup;
    }

    uint8_t* header = image->header;

    if(storage_file_read(image->file, &header[0], ATR_FILE_HEADER_SIZE) != ATR_FILE_HEADER_SIZE) {
        set_last_error(ERROR_TITLE, "Failed to read ATR file header");
        goto cleanup;
    }

    if(header[0] != 0x96 || header[1] != 0x02) {
        set_last_error(ERROR_TITLE, "Invalid ATR file header (%02X 0x02X)", header[0], header[1]);
        goto cleanup;
    }

    image->disk_size = (image->header[2] + (header[3] << 8) + (header[6] << 16)) * 16;
    image->sector_size = header[4] + (header[5] << 8);

    if(image->sector_size != 128 && image->sector_size != 256) {
        set_last_error(ERROR_TITLE, "Invalid sector size (%d)", image->sector_size);
        goto cleanup;
    }

    if(storage_file_size(image->file) < image->disk_size + ATR_FILE_HEADER_SIZE) {
        set_last_error(
            ERROR_TITLE,
            "Invalid ATR file size (%d/%d)",
            storage_file_size(image->file),
            image->disk_size + ATR_FILE_HEADER_SIZE);
        goto cleanup;
    }

    if(!guess_disk_geometry(&image->geometry, image->disk_size, image->sector_size)) {
        FURI_LOG_W(TAG, "Failed to guess disk geometry");
    } else {
        FURI_LOG_I(
            TAG,
            "Guessed disk geometry: heads=%d, tracks=%d, sectors_per_track=%d, sector_size=%d, density=%d",
            image->geometry.heads,
            image->geometry.tracks,
            image->geometry.sectors_per_track,
            image->geometry.sector_size,
            image->geometry.density);
    }

    FURI_LOG_I(
        TAG,
        "ATR file: disk_size=%d, sector_size=%d, sector_count=%d",
        image->disk_size,
        image->sector_size,
        image->disk_size / image->sector_size);

    return image;

cleanup:
    disk_image_close(image);
    return NULL;
}

void disk_image_close(DiskImage* image) {
    if(image) {
        if(image->file) {
            storage_file_free(image->file);
        }

        if(image->path) {
            furi_string_free(image->path);
        }

        free(image);
    }
}

const FuriString* disk_image_path(const DiskImage* image) {
    return image->path;
}

DiskGeometry disk_geometry(const DiskImage* image) {
    return image->geometry;
}

size_t disk_image_size(const DiskImage* image) {
    return image->disk_size;
}

size_t disk_image_sector_size(const DiskImage* image) {
    return image->sector_size;
}

size_t disk_image_sector_count(const DiskImage* image) {
    return 3 + (image->disk_size - 768) / image->sector_size;
}

bool disk_image_get_write_protect(const DiskImage* image) {
    return (image->header[8] & 0x20) != 0;
}

bool disk_image_set_write_protect(DiskImage* image, bool write_protect) {
    if(write_protect) {
        image->header[8] |= 0x20;
    } else {
        image->header[8] &= ~0x20;
    }

    if(!storage_file_seek(image->file, 0, true)) {
        return false;
    }

    return storage_file_write(image->file, image->header, 1) == 1;
}

size_t disk_image_nth_sector_size(const DiskImage* image, size_t sector) {
    if(sector == 0 || sector > disk_image_sector_count(image)) {
        return 0;
    }

    return (sector <= 3) ? 128 : image->sector_size;
}

static uintptr_t disk_image_nth_sector_offset(const DiskImage* image, size_t sector) {
    if(sector == 0 || sector > disk_image_sector_count(image)) {
        return 0;
    }

    uintptr_t offset = ATR_FILE_HEADER_SIZE;

    if(sector <= 3) {
        offset += (sector - 1) * 128;
    } else {
        offset += 384 + (sector - 4) * image->sector_size;
    }

    return offset;
}

bool disk_image_read_sector(DiskImage* image, size_t sector, void* buffer) {
    size_t sector_size = disk_image_nth_sector_size(image, sector);

    if(sector_size == 0) {
        return false;
    }

    uintptr_t offset = disk_image_nth_sector_offset(image, sector);

    if(!storage_file_seek(image->file, offset, true)) {
        return false;
    }

    return storage_file_read(image->file, buffer, sector_size) == sector_size;
}

bool disk_image_write_sector(DiskImage* image, size_t sector, const void* buffer) {
    size_t sector_size = disk_image_nth_sector_size(image, sector);

    if(sector_size == 0) {
        return false;
    }

    uintptr_t offset = disk_image_nth_sector_offset(image, sector);

    if(!storage_file_seek(image->file, offset, true)) {
        return false;
    }

    return storage_file_write(image->file, buffer, sector_size) == image->sector_size;
}

static bool guess_disk_geometry(DiskGeometry* geom, size_t disk_size, size_t sector_size) {
    if(disk_size == 90 * 1024 && sector_size == 128) {
        geom->heads = 1;
        geom->tracks = 40;
        geom->sectors_per_track = 18;
        geom->sector_size = 128;
        geom->density = 0;
    } else if(disk_size == 130 * 1024 && sector_size == 128) {
        geom->heads = 1;
        geom->tracks = 40;
        geom->sectors_per_track = 26;
        geom->sector_size = 128;
        geom->density = 0x4;
    } else if(disk_size == (180 * 1024 - 3 * 128) && sector_size == 256) {
        geom->heads = 1;
        geom->tracks = 40;
        geom->sectors_per_track = 18;
        geom->sector_size = 256;
        geom->density = 0x4;
    } else if(disk_size == (360 * 1024 - 3 * 128) && sector_size == 256) {
        geom->heads = 2;
        geom->tracks = 40;
        geom->sectors_per_track = 18;
        geom->sector_size = 256;
        geom->density = 0x4;
    } else if(disk_size == (720 * 1024 - 3 * 128) && sector_size == 256) {
        geom->heads = 2;
        geom->tracks = 80;
        geom->sectors_per_track = 18;
        geom->sector_size = 256;
        geom->density = 0x4;
    } else {
        return false;
    }

    return true;
}
