#pragma once
// Stored app profiles: LittleFS on the `spiffs` partition (boards/nano_partitions.csv, 1.4MB),
// one JSON file per profile, /profiles/<id>.json (app_profiles/profile_json.h). A file whose
// id matches a built-in profile overrides it; any other id is a profile of its own.
//
// Called from the usb task only (boot load, companion uploads), so there's no locking.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mounts, formatting the partition on first use. False = no storage: built-ins only.
bool profile_store_init(void);

// Calls fn(id) for each stored profile.
void profile_store_list(void (*fn)(const char *id, void *ctx), void *ctx);

// The file's text, NUL-terminated, malloc'd (free() it), or NULL.
char *profile_store_read(const char *id, size_t *len);

// Written to a temp file and renamed over the old one, so a reset mid-write never leaves
// half a profile.
bool profile_store_write(const char *id, const char *text, size_t len);

bool profile_store_delete(const char *id);

#ifdef __cplusplus
}
#endif
