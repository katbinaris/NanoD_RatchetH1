#include "app_profiles.h"
#include <string.h>

// One line per profile. Each is defined in its own file in this folder. Blender and AutoCAD are
// still the empty template (app_profile.h APP_PROFILE_EMPTY).
extern const app_profile_t app_profile_plasticity;
extern const app_profile_t app_profile_figma;
extern const app_profile_t app_profile_onshape;
extern const app_profile_t app_profile_blender;
extern const app_profile_t app_profile_autocad;
extern const app_profile_t app_profile_empty; // the fallback, not listed

static const app_profile_t *const s_profiles[] = {
    &app_profile_plasticity,
    &app_profile_figma,
    &app_profile_onshape,
    &app_profile_blender,
    &app_profile_autocad,
};

#define PROFILE_COUNT ((int)(sizeof(s_profiles) / sizeof(s_profiles[0])))

static bool str_ok(const char *s) {
    return s != NULL && s[0] != '\0';
}

static bool scene_ok(const app_scene_t *s) {
    if (s == NULL) return true; // no card: the wheel shows the name only
    if (s->n_base && s->base == NULL) return false;
    if (s->n_frames && s->frames == NULL) return false;
    for (int i = 0; i < s->n_frames; i++) {
        if (s->frames[i].n && s->frames[i].el == NULL) return false;
    }
    return true;
}

// Everything the engine and the screens dereference or index without further checks. Built-in
// profiles are compiled in, so this mostly guards against a slip in a new file today -- and is
// the gate uploaded profiles will go through later (Milestone 2).
static bool profile_ok(const app_profile_t *p) {
    if (p == NULL || p->version != APP_PROFILE_VERSION || !str_ok(p->id) || !str_ok(p->name)) return false;
    for (int i = 0; i < 4; i++) {
        if (p->legend[i] == NULL) return false;
    }
    if (p->visual > APP_VISUAL_SHAPE || p->shape > APP_SHAPE_OCTA || p->shape_style > APP_SHAPE_STYLE_THICK) return false;
    bool wheel = false;
    for (int i = 0; i < APP_SLOT_COUNT; i++) {
        const app_action_t *a = &p->slot[i];
        if (a->kind > APP_ACT_COMMANDS || a->feel >= HAPTIC_TYPE_COUNT) return false;
        if (a->kind == APP_ACT_COMMANDS) wheel = true;
    }
    if (p->ring_count && p->rings == NULL) return false;
    if (wheel && p->ring_count == 0) return false;
    for (int r = 0; r < p->ring_count; r++) {
        const app_ring_t *ring = &p->rings[r];
        if (!str_ok(ring->name) || !str_ok(ring->tab) || ring->count == 0 || ring->cmds == NULL) return false;
        if (ring->slot >= APP_SLOT_COUNT || ring->count > 254) return false;
        for (int c = 0; c < ring->count; c++) {
            const app_cmd_t *cmd = &ring->cmds[c];
            if (!str_ok(cmd->name) || !scene_ok(cmd->scene)) return false;
            if (cmd->kind == APP_CMD_ACTIONS && (!str_ok(cmd->phrase) || p->search.open.keycode == 0)) return false;
            if (cmd->param && (cmd->param->label == NULL || cmd->param->min > cmd->param->max)) return false;
        }
    }
    return true;
}

// Checked once per profile, on first use (0 = not yet, 1 = ok, -1 = replaced by the fallback).
// A race between tasks only means checking twice; the answer is the same.
static int8_t s_checked[PROFILE_COUNT];

int app_profiles_count(void) {
    return PROFILE_COUNT;
}

const app_profile_t *app_profiles_get(int index) {
    if (index < 0 || index >= PROFILE_COUNT) index = 0;
    if (s_checked[index] == 0) s_checked[index] = profile_ok(s_profiles[index]) ? 1 : -1;
    return s_checked[index] > 0 ? s_profiles[index] : &app_profile_empty;
}

int app_profiles_find(const char *id) {
    for (int i = 0; i < PROFILE_COUNT; i++) {
        if (s_profiles[i] != NULL && s_profiles[i]->id != NULL && strcmp(s_profiles[i]->id, id) == 0) return i;
    }
    return -1;
}
