#include "profile_store.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "pstore";

#define BASE "/fs"
#define DIR_PATH BASE "/profiles"
#define MAX_FILE (96 * 1024) // well over the biggest profile (a full wheel + both icons)

static bool s_ok = false;

bool profile_store_init(void) {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = BASE,
        .partition_label = "spiffs",
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s -- built-in profiles only", esp_err_to_name(err));
        return false;
    }
    mkdir(DIR_PATH, 0775); // EEXIST after the first boot
    size_t total = 0, used = 0;
    esp_littlefs_info("spiffs", &total, &used);
    ESP_LOGI(TAG, "mounted: %u of %u KB used", (unsigned)(used / 1024), (unsigned)(total / 1024));
    s_ok = true;
    return true;
}

static void path_for(char *out, size_t n, const char *id, const char *ext) {
    snprintf(out, n, DIR_PATH "/%s%s", id, ext);
}

void profile_store_list(void (*fn)(const char *id, void *ctx), void *ctx) {
    if (!s_ok) return;
    DIR *d = opendir(DIR_PATH);
    if (d == NULL) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        char id[32];
        size_t len = strlen(e->d_name);
        if (len <= 5 || len - 5 >= sizeof(id) || strcmp(e->d_name + len - 5, ".json") != 0) continue;
        memcpy(id, e->d_name, len - 5);
        id[len - 5] = '\0';
        fn(id, ctx);
    }
    closedir(d);
}

char *profile_store_read(const char *id, size_t *len) {
    if (!s_ok) return NULL;
    char path[64];
    path_for(path, sizeof(path), id, ".json");
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = NULL;
    if (size > 0 && size <= MAX_FILE) {
        buf = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM);
        if (buf && fread(buf, 1, size, f) == (size_t)size) {
            buf[size] = '\0';
            if (len) *len = size;
        } else {
            free(buf);
            buf = NULL;
        }
    }
    fclose(f);
    if (buf == NULL) ESP_LOGW(TAG, "%s: unreadable (%ld bytes)", path, size);
    return buf;
}

bool profile_store_write(const char *id, const char *text, size_t len) {
    if (!s_ok) return false;
    char path[64], tmp[64];
    path_for(path, sizeof(path), id, ".json");
    path_for(tmp, sizeof(tmp), id, ".tmp");
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return false;
    bool ok = fwrite(text, 1, len, f) == len;
    ok = (fclose(f) == 0) && ok;
    if (ok) {
        remove(path); // LittleFS rename replaces, but not every VFS does
        ok = rename(tmp, path) == 0;
    }
    if (!ok) {
        remove(tmp);
        ESP_LOGE(TAG, "%s: write failed", path);
    } else {
        ESP_LOGI(TAG, "%s: saved (%u bytes)", path, (unsigned)len);
    }
    return ok;
}

bool profile_store_delete(const char *id) {
    if (!s_ok) return false;
    char path[64];
    path_for(path, sizeof(path), id, ".json");
    return remove(path) == 0;
}
