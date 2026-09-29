#include "app_profiles.h"
#include <string.h>

// One line per profile. Each is defined in its own file in this folder.
extern const app_profile_t app_profile_plasticity;
extern const app_profile_t app_profile_figma;
extern const app_profile_t app_profile_onshape;

static const app_profile_t *const s_profiles[] = {
    &app_profile_plasticity,
    &app_profile_figma,
    &app_profile_onshape,
};

#define PROFILE_COUNT ((int)(sizeof(s_profiles) / sizeof(s_profiles[0])))

int app_profiles_count(void) {
    return PROFILE_COUNT;
}

const app_profile_t *app_profiles_get(int index) {
    if (index < 0 || index >= PROFILE_COUNT) index = 0;
    return s_profiles[index];
}

int app_profiles_find(const char *id) {
    for (int i = 0; i < PROFILE_COUNT; i++) {
        if (strcmp(s_profiles[i]->id, id) == 0) return i;
    }
    return -1;
}
