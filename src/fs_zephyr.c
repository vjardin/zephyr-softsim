/*
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Copyright (c) 2025 Vincent Jardin <vjardin@free.fr>, Free Mobile
 *
 * Zephyr NVS-based filesystem implementation for onomondo-uicc
 *
 * This implements the fs.h interface using Zephyr's NVS storage.
 * Files are stored in NVS with IDs derived from path hashes.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <onomondo/softsim/fs.h>
#include <onomondo/softsim/storage.h>

LOG_MODULE_REGISTER(softsim_fs, CONFIG_SOFTSIM_LOG_LEVEL);

/* NVS configuration - use partition manager's settings partition */
#if FIXED_PARTITION_EXISTS(settings_storage)
#define SOFTSIM_NVS_PARTITION_LABEL settings_storage
#elif FIXED_PARTITION_EXISTS(storage_partition)
#define SOFTSIM_NVS_PARTITION_LABEL storage_partition
#else
#error "No suitable storage partition found"
#endif

#define SOFTSIM_NVS_SECTOR_SIZE     4096
#define SOFTSIM_NVS_SECTOR_COUNT    8  /* 32KB partition / 4KB sectors */

/* Use Kconfig values if available, otherwise defaults */
#ifndef CONFIG_SOFTSIM_MAX_FILE_SIZE
#define CONFIG_SOFTSIM_MAX_FILE_SIZE 1536
#endif

#ifndef CONFIG_SOFTSIM_MAX_OPEN_FILES
#define CONFIG_SOFTSIM_MAX_OPEN_FILES 4
#endif

/* NVS ID range for softsim files */
#define NVS_ID_BASE 0x1000
#define NVS_ID_MAX  0x1FFF

/* File handle structure - simulates a file in memory */
struct ss_file_handle {
    uint16_t nvs_id;           /* NVS ID for this file */
    uint8_t *buffer;           /* File content buffer */
    size_t size;               /* Current file size */
    size_t capacity;           /* Buffer capacity */
    size_t position;           /* Current read/write position */
    char path[SS_STORAGE_PATH_MAX];  /* Original path */
    bool modified;             /* True if file has been written to */
    bool is_open;              /* True if handle is in use */
};

/* Global NVS handle */
static struct nvs_fs softsim_nvs;
static bool nvs_initialized = false;

/* File handle pool */
static struct ss_file_handle file_handles[CONFIG_SOFTSIM_MAX_OPEN_FILES];

/* Storage path (used by storage.c) */
char storage_path[SS_STORAGE_PATH_MAX] = SS_STORAGE_PATH_DEFAULT;

/* Simple hash function for path to NVS ID */
static uint16_t path_to_nvs_id(const char *path)
{
    uint32_t hash = 5381;
    const char *p = path;

    while (*p) {
        hash = ((hash << 5) + hash) + (uint8_t)*p;
        p++;
    }

    /* Map to NVS ID range */
    return NVS_ID_BASE + (hash % (NVS_ID_MAX - NVS_ID_BASE));
}

/* Initialize NVS if not already done */
static int ensure_nvs_init(void)
{
    int err;

    if (nvs_initialized) {
        return 0;
    }

    LOG_INF("Initializing SoftSIM NVS storage - Copyright (c) Free Mobile");

    softsim_nvs.flash_device = FIXED_PARTITION_DEVICE(SOFTSIM_NVS_PARTITION_LABEL);
    if (!device_is_ready(softsim_nvs.flash_device)) {
        LOG_ERR("Flash device not ready");
        return -ENODEV;
    }

    softsim_nvs.offset = FIXED_PARTITION_OFFSET(SOFTSIM_NVS_PARTITION_LABEL);
    softsim_nvs.sector_size = SOFTSIM_NVS_SECTOR_SIZE;
    softsim_nvs.sector_count = SOFTSIM_NVS_SECTOR_COUNT;

    LOG_INF("NVS: offset=0x%lx, sector_size=%u, sector_count=%u",
            (unsigned long)softsim_nvs.offset, softsim_nvs.sector_size, softsim_nvs.sector_count);

    err = nvs_mount(&softsim_nvs);
    if (err) {
        LOG_ERR("NVS mount failed: %d", err);
        return err;
    }

    nvs_initialized = true;
    LOG_INF("SoftSIM NVS storage initialized successfully");

    return 0;
}

/* Find a free file handle */
static struct ss_file_handle *get_free_handle(void)
{
    for (int i = 0; i < CONFIG_SOFTSIM_MAX_OPEN_FILES; i++) {
        if (!file_handles[i].is_open) {
            return &file_handles[i];
        }
    }
    return NULL;
}

int ss_storage_set_path(const char *path)
{
    if (!path || strlen(path) >= SS_STORAGE_PATH_MAX || strlen(path) == 0) {
        return -1;
    }

    strncpy(storage_path, path, SS_STORAGE_PATH_MAX - 1);
    storage_path[SS_STORAGE_PATH_MAX - 1] = '\0';
    return 0;
}

const char *ss_storage_get_path(void)
{
    return storage_path;
}

ss_FILE ss_fopen(char *path, char *mode)
{
    struct ss_file_handle *handle;
    int err;

    if (!path || !mode) {
        LOG_ERR("ss_fopen: NULL path or mode");
        return NULL;
    }

    err = ensure_nvs_init();
    if (err) {
        LOG_ERR("ss_fopen: NVS init failed");
        return NULL;
    }

    handle = get_free_handle();
    if (!handle) {
        LOG_ERR("No free file handles");
        return NULL;
    }

    memset(handle, 0, sizeof(*handle));
    strncpy(handle->path, path, sizeof(handle->path) - 1);
    handle->nvs_id = path_to_nvs_id(path);
    handle->position = 0;
    handle->is_open = true;
    handle->modified = false;

    LOG_DBG("ss_fopen: path=%s mode=%s nvs_id=0x%04x", path, mode, handle->nvs_id);

    /* Allocate buffer */
    handle->buffer = malloc(CONFIG_SOFTSIM_MAX_FILE_SIZE);
    if (!handle->buffer) {
        LOG_ERR("Failed to allocate file buffer (%u bytes) for %s",
                CONFIG_SOFTSIM_MAX_FILE_SIZE, path);
        handle->is_open = false;
        return NULL;
    }
    handle->capacity = CONFIG_SOFTSIM_MAX_FILE_SIZE;
    memset(handle->buffer, 0xFF, CONFIG_SOFTSIM_MAX_FILE_SIZE);
    LOG_DBG("Allocated buffer %p for file %s", handle->buffer, path);

    if (strchr(mode, 'r') != NULL || strchr(mode, '+') != NULL) {
        /* Read mode - try to load existing content */
        ssize_t len = nvs_read(&softsim_nvs, handle->nvs_id,
                              handle->buffer, CONFIG_SOFTSIM_MAX_FILE_SIZE);
        if (len > 0) {
            handle->size = len;
            LOG_DBG("Loaded file %s (id=%04x, size=%zu)", path, handle->nvs_id, handle->size);
        } else {
            handle->size = 0;
            if (strchr(mode, 'r') != NULL && strchr(mode, '+') == NULL &&
                strchr(mode, 'w') == NULL) {
                /* Read-only mode and file doesn't exist */
                LOG_DBG("File not found: %s", path);
                free(handle->buffer);
                handle->buffer = NULL;
                handle->is_open = false;
                return NULL;
            }
        }
    }

    if (strchr(mode, 'w') != NULL) {
        /* Write mode - truncate file */
        handle->size = 0;
        handle->modified = true;
    }

    LOG_DBG("Opened file %s (id=%04x, mode=%s)", path, handle->nvs_id, mode);

    return (ss_FILE)handle;
}

int ss_fclose(ss_FILE f)
{
    struct ss_file_handle *handle = (struct ss_file_handle *)f;

    if (!handle || !handle->is_open) {
        LOG_ERR("ss_fclose: invalid handle");
        return -1;
    }

    /* If modified, write back to NVS */
    if (handle->modified && handle->size > 0) {
        LOG_DBG("ss_fclose: writing %s to NVS (id=0x%04x, size=%zu)",
                handle->path, handle->nvs_id, handle->size);
        int err = nvs_write(&softsim_nvs, handle->nvs_id,
                           handle->buffer, handle->size);
        if (err < 0) {
            LOG_ERR("ss_fclose: NVS write FAILED for %s: %d", handle->path, err);
        } else {
            LOG_DBG("ss_fclose: NVS write OK for %s (wrote %d bytes)",
                    handle->path, err);
        }
    }

    if (handle->buffer) {
        free(handle->buffer);
        handle->buffer = NULL;
    }

    handle->is_open = false;

    return 0;
}

size_t ss_fread(void *ptr, size_t size, size_t nmemb, ss_FILE f)
{
    struct ss_file_handle *handle = (struct ss_file_handle *)f;
    size_t total_bytes = size * nmemb;
    size_t available;
    size_t to_read;

    if (!handle || !handle->is_open || !ptr) {
        return 0;
    }

    available = (handle->position < handle->size) ?
                (handle->size - handle->position) : 0;
    to_read = (total_bytes < available) ? total_bytes : available;

    if (to_read > 0) {
        memcpy(ptr, handle->buffer + handle->position, to_read);
        handle->position += to_read;
    }

    return to_read / size;  /* Return number of items read */
}

size_t ss_fwrite(const void *ptr, size_t size, size_t count, ss_FILE f)
{
    struct ss_file_handle *handle = (struct ss_file_handle *)f;
    size_t total_bytes = size * count;
    size_t new_end;

    if (!handle || !handle->is_open || !ptr) {
        return 0;
    }

    new_end = handle->position + total_bytes;
    if (new_end > handle->capacity) {
        LOG_ERR("Write would exceed file capacity");
        return 0;
    }

    memcpy(handle->buffer + handle->position, ptr, total_bytes);
    handle->position += total_bytes;

    if (handle->position > handle->size) {
        handle->size = handle->position;
    }

    handle->modified = true;

    return count;
}

int ss_file_size(const char *path)
{
    int err;
    uint16_t nvs_id;
    ssize_t len;

    if (!path) {
        LOG_ERR("ss_file_size: NULL path");
        return -1;
    }

    err = ensure_nvs_init();
    if (err) {
        LOG_ERR("ss_file_size: NVS init failed");
        return -1;
    }

    nvs_id = path_to_nvs_id(path);

    /* Query size without reading data - NVS returns length when buffer is NULL */
    len = nvs_read(&softsim_nvs, nvs_id, NULL, 0);
    if (len < 0) {
        LOG_DBG("ss_file_size: file not found %s (id=%04x)", path, nvs_id);
        return -1;
    }

    LOG_DBG("ss_file_size: %s (id=%04x) = %zd bytes", path, nvs_id, len);
    return (int)len;
}

int ss_delete_file(const char *path)
{
    int err;
    uint16_t nvs_id;

    if (!path) {
        return -1;
    }

    err = ensure_nvs_init();
    if (err) {
        return -1;
    }

    nvs_id = path_to_nvs_id(path);

    err = nvs_delete(&softsim_nvs, nvs_id);
    if (err && err != -ENOENT) {
        LOG_ERR("Failed to delete file %s: %d", path, err);
        return -1;
    }

    LOG_DBG("Deleted file %s (id=%04x)", path, nvs_id);

    return 0;
}

int ss_delete_dir(const char *path)
{
    /* For NVS, directories don't really exist */
    /* We could iterate and delete all matching IDs, but for now just succeed */
    (void)path;
    return 0;
}

int ss_fseek(ss_FILE f, long offset, int whence)
{
    struct ss_file_handle *handle = (struct ss_file_handle *)f;
    long new_pos;

    if (!handle || !handle->is_open) {
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = (long)handle->position + offset;
        break;
    case SEEK_END:
        new_pos = (long)handle->size + offset;
        break;
    default:
        return -1;
    }

    if (new_pos < 0) {
        return -1;
    }

    handle->position = (size_t)new_pos;

    return 0;
}

int ss_access(const char *path, int amode)
{
    int err;
    uint16_t nvs_id;
    uint8_t buf[1];
    ssize_t len;

    (void)amode;  /* Ignore access mode, just check existence */

    if (!path) {
        return -1;
    }

    err = ensure_nvs_init();
    if (err) {
        return -1;
    }

    nvs_id = path_to_nvs_id(path);

    /* Check if entry exists */
    len = nvs_read(&softsim_nvs, nvs_id, buf, sizeof(buf));

    return (len >= 0) ? 0 : -1;
}

int ss_create_dir(const char *path, uint32_t mode)
{
    /* For NVS-based storage, directories are implicit */
    (void)path;
    (void)mode;
    return 0;
}
