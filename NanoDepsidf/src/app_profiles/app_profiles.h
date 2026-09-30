#pragma once
// Registry of the app profiles (app_profiles.c): the built-ins compiled in from this folder,
// each possibly overridden by a stored copy (profile_store.h), then the stored profiles of
// their own. Index order is the PROFILE row's order: built-ins first, then the others (by id
// at boot; one added later goes to the end until the next boot).
// The index is only ever used at runtime -- NVS stores the profile's `id`, so adding or
// reordering profiles never changes which one a saved setting points at.
//
// Every profile can also carry a live edit from the companion app: in use right away but not
// stored until saved (like the settings' live values vs NVS).
//
// Reading is lock-free and cheap (the control loop calls app_profiles_get() every tick).
// Changes come from the usb task only. A replaced profile stays allocated for a few seconds
// (app_profiles_reap()), so a task still holding the old pointer finishes with it safely.

#include "app_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_PROFILES_MAX 16

// Mounts the store and loads what's in it. Before any other call (main, before the tasks).
void app_profiles_init(void);

int app_profiles_count(void);
const app_profile_t *app_profiles_get(int index); // clamped to a valid profile
int app_profiles_find(const char *id);             // index, or -1 if unknown
// Bumped on every change: a screen caching names / icons rebuilds when it moves.
uint32_t app_profiles_version(void);

// Where a profile comes from, for the companion app's list.
#define APP_PROFILE_BUILTIN 0x01 // compiled in (RESET TO DEFAULT is possible)
#define APP_PROFILE_STORED 0x02  // a stored file (overrides the built-in, if there is one)
#define APP_PROFILE_LIVE 0x04    // a live edit, not stored yet
uint8_t app_profiles_flags(int index);

// The checks every profile passes before use: what the engine and the screens dereference or
// index without further checks.
bool app_profiles_valid(const app_profile_t *p);

// --- changes (usb task) ---
typedef enum {
    APP_PROFILES_OK = 0,
    APP_PROFILES_ERR_FULL,    // APP_PROFILES_MAX reached
    APP_PROFILES_ERR_STORAGE, // the file couldn't be written / removed
    APP_PROFILES_ERR_INDEX,
} app_profiles_err_t;

// Takes ownership of `p` (from profile_json_read) as the live edit of the profile with its
// id, adding a new one if the id is unknown. *index = where it is.
app_profiles_err_t app_profiles_put_live(app_profile_t *p, int *index);
// Stores the live edit; it becomes the stored version. Nothing to store is OK.
app_profiles_err_t app_profiles_save(int index);
// Drops the live edit: back to the stored version (or the built-in). A new profile that was
// never stored has nothing to go back to: it's removed (*removed = true, as below).
app_profiles_err_t app_profiles_revert(int index, bool *removed);
// Removes the stored copy and any live edit: a built-in goes back to its default, any other
// profile is gone -- the ones after it move up one index (*removed = true).
app_profiles_err_t app_profiles_remove(int index, bool *removed);

// Frees replaced profiles once nobody can still be using them. Every so often (usb task).
void app_profiles_reap(void);

#ifdef __cplusplus
}
#endif
