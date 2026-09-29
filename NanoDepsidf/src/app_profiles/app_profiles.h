#pragma once
// Registry of the built-in app profiles (app_profiles.c). Index order is the PROFILE row's
// order; the index is only ever used at runtime -- NVS stores the profile's `id`, so adding
// or reordering profiles never changes which one a saved setting points at.

#include "app_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_profiles_count(void);
const app_profile_t *app_profiles_get(int index); // clamped to a valid profile
int app_profiles_find(const char *id);             // index, or -1 if unknown

#ifdef __cplusplus
}
#endif
