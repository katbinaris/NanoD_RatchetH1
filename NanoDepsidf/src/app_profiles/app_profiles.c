#include "app_profiles.h"
#include "profile_json.h"
#include "../profile_store.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
static const char *TAG = "profiles";
static int64_t now_us(void) { return esp_timer_get_time(); }
#else // host test build (tools/profile_json_test)
#include <stdio.h>
#define ESP_LOGI(tag, fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
static int64_t now_us(void) { return 0; }
#endif

// One line per built-in. Each is defined in its own file in this folder. Blender and AutoCAD
// are still the empty template (app_profile.h APP_PROFILE_EMPTY).
extern const app_profile_t app_profile_music;
extern const app_profile_t app_profile_plasticity;
extern const app_profile_t app_profile_figma;
extern const app_profile_t app_profile_onshape;
extern const app_profile_t app_profile_blender;
extern const app_profile_t app_profile_autocad;
extern const app_profile_t app_profile_empty; // the fallback, not listed

static const app_profile_t *const s_builtins[] = {
    &app_profile_music, // first = the default on a fresh device
    &app_profile_plasticity,
    &app_profile_figma,
    &app_profile_onshape,
    &app_profile_blender,
    &app_profile_autocad,
};
#define BUILTIN_COUNT ((int)(sizeof(s_builtins) / sizeof(s_builtins[0])))
_Static_assert(BUILTIN_COUNT <= APP_PROFILES_MAX, "APP_PROFILES_MAX");

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

static bool macro_ref_ok(const app_profile_t *p, uint8_t ref) {
    return ref <= p->macro_count;
}

bool app_profiles_valid(const app_profile_t *p) {
    if (p == NULL || p->version != APP_PROFILE_VERSION || !str_ok(p->id) || !str_ok(p->name)) return false;
    if (p->macro_count > APP_MACROS_MAX || (p->macro_count && p->macros == NULL)) return false;
    for (int m = 0; m < p->macro_count; m++) {
        const app_macro_t *mac = &p->macros[m];
        if (!str_ok(mac->name) || mac->count > APP_MACRO_STEPS_MAX || (mac->count && mac->steps == NULL)) return false;
        for (int i = 0; i < mac->count; i++) {
            const app_mstep_t *st = &mac->steps[i];
            if (st->kind > APP_MSTEP_WAIT || (st->kind == APP_MSTEP_TEXT && st->text == NULL)) return false;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (p->legend[i] == NULL) return false;
    }
    if (p->visual > APP_VISUAL_SHAPE || p->shape > APP_SHAPE_OCTA || p->shape_style > APP_SHAPE_STYLE_THICK) return false;
    bool wheel = false;
    for (int i = 0; i < APP_SLOT_COUNT; i++) {
        const app_action_t *a = &p->slot[i];
        if (a->kind > APP_ACT_MEDIA || a->feel >= HAPTIC_TYPE_COUNT) return false;
        if (!macro_ref_ok(p, a->macro) || !macro_ref_ok(p, a->tap_macro)) return false;
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
            if (cmd->kind > APP_CMD_MACRO || (cmd->kind == APP_CMD_MACRO && cmd->macro == 0) || !macro_ref_ok(p, cmd->macro)) return false;
            if (cmd->param && (cmd->param->label == NULL || cmd->param->min > cmd->param->max)) return false;
        }
    }
    return true;
}

// --- the registry ---
// Entry i: its built-in (or NULL), stored copy and live edit (owned, or NULL). s_view[i] is
// what readers get: live, else stored, else the built-in. Only the usb task writes.
typedef struct {
    const app_profile_t *builtin;
    app_profile_t *stored;
    app_profile_t *live;
} entry_t;

static entry_t s_e[APP_PROFILES_MAX];
static const app_profile_t *_Atomic s_view[APP_PROFILES_MAX];
static _Atomic int s_count = 0;
static _Atomic uint32_t s_version = 0;

// Replaced profiles, freed after a grace period: every reader re-fetches its pointer at least
// once a frame (display, LEDs) or a tick (control loop), so a few seconds is plenty.
#define RETIRE_MAX 8
#define RETIRE_US 3000000
static struct {
    app_profile_t *p;
    int64_t at;
} s_retired[RETIRE_MAX];

static void retire(app_profile_t *p) {
    if (p == NULL) return;
    for (int i = 0; i < RETIRE_MAX; i++) {
        if (s_retired[i].p == NULL) {
            s_retired[i].p = p;
            s_retired[i].at = now_us();
            return;
        }
    }
    // Full (a burst of edits): keep it rather than risk a use after free. Bounded by edits
    // in a few seconds, and the companion throttles those.
    ESP_LOGW(TAG, "retire list full: leaking one profile");
}

void app_profiles_reap(void) {
    int64_t now = now_us();
    for (int i = 0; i < RETIRE_MAX; i++) {
        if (s_retired[i].p && now - s_retired[i].at >= RETIRE_US) {
            profile_json_free(s_retired[i].p);
            s_retired[i].p = NULL;
        }
    }
}

static const app_profile_t *view_of(const entry_t *e) {
    if (e->live) return e->live;
    if (e->stored) return e->stored;
    return e->builtin && app_profiles_valid(e->builtin) ? e->builtin : &app_profile_empty;
}

static void publish(int i) {
    atomic_store(&s_view[i], view_of(&s_e[i]));
}

static void changed(void) {
    atomic_fetch_add(&s_version, 1);
}

static int find(const char *id) {
    int n = atomic_load(&s_count);
    for (int i = 0; i < n; i++) {
        const app_profile_t *p = s_e[i].builtin ? s_e[i].builtin : s_e[i].stored ? s_e[i].stored : s_e[i].live;
        if (p && p->id && strcmp(p->id, id) == 0) return i;
    }
    return -1;
}

// --- boot ---

static void load_one(const char *id, void *ctx) {
    (void)ctx;
    size_t len = 0;
    char *text = profile_store_read(id, &len);
    if (text == NULL) return;
    char err[80];
    app_profile_t *p = profile_json_read(text, len, err, sizeof(err));
    free(text);
    if (p == NULL) {
        ESP_LOGE(TAG, "%s.json: %s -- ignored", id, err);
        return;
    }
    if (strcmp(p->id, id) != 0) {
        ESP_LOGE(TAG, "%s.json: says it's \"%s\" -- ignored", id, p->id);
        profile_json_free(p);
        return;
    }
    int i = find(id);
    if (i < 0) {
        i = atomic_load(&s_count);
        if (i >= APP_PROFILES_MAX) {
            ESP_LOGE(TAG, "%s.json: no room (max %d profiles) -- ignored", id, APP_PROFILES_MAX);
            profile_json_free(p);
            return;
        }
        s_e[i] = (entry_t){0};
        atomic_store(&s_count, i + 1);
    }
    s_e[i].stored = p;
    ESP_LOGI(TAG, "%s: %s", id, s_e[i].builtin ? "stored, overrides the built-in" : "stored");
}

static int by_id(const void *a, const void *b) {
    return strcmp(((const entry_t *)a)->stored->id, ((const entry_t *)b)->stored->id);
}

#ifdef ESP_PLATFORM
// cJSON makes a node per value: tens of thousands of small blocks for a big profile, which
// would all land in internal RAM. They go to PSRAM instead.
static void *json_malloc(size_t n) {
    return heap_caps_malloc_prefer(n, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
}
#endif

void app_profiles_init(void) {
#ifdef ESP_PLATFORM
    cJSON_Hooks hooks = {json_malloc, free};
    cJSON_InitHooks(&hooks);
#endif
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        s_e[i] = (entry_t){s_builtins[i], NULL, NULL};
        if (!app_profiles_valid(s_builtins[i])) ESP_LOGE(TAG, "built-in %d fails its checks: shown empty", i);
    }
    atomic_store(&s_count, BUILTIN_COUNT);
    if (profile_store_init()) profile_store_list(load_one, NULL);
    int n = atomic_load(&s_count);
    if (n - BUILTIN_COUNT > 1) qsort(&s_e[BUILTIN_COUNT], n - BUILTIN_COUNT, sizeof(entry_t), by_id);
    for (int i = 0; i < n; i++) publish(i);
    changed();
}

// --- reading ---

int app_profiles_count(void) {
    return atomic_load(&s_count);
}

const app_profile_t *app_profiles_get(int index) {
    int n = atomic_load(&s_count);
    if (index < 0 || index >= n) index = 0;
    const app_profile_t *p = atomic_load(&s_view[index]);
    return p ? p : &app_profile_empty; // before init
}

int app_profiles_find(const char *id) {
    return id ? find(id) : -1;
}

uint32_t app_profiles_version(void) {
    return atomic_load(&s_version);
}

uint8_t app_profiles_flags(int index) {
    if (index < 0 || index >= atomic_load(&s_count)) return 0;
    const entry_t *e = &s_e[index];
    return (e->builtin ? APP_PROFILE_BUILTIN : 0) | (e->stored ? APP_PROFILE_STORED : 0) | (e->live ? APP_PROFILE_LIVE : 0);
}

// --- changes ---

static bool index_ok(int i) {
    return i >= 0 && i < atomic_load(&s_count);
}

// Takes entry i out; the ones after it move up.
static void drop(int i) {
    int n = atomic_load(&s_count);
    for (int k = i; k < n - 1; k++) {
        s_e[k] = s_e[k + 1];
        publish(k);
    }
    atomic_store(&s_count, n - 1);
    s_e[n - 1] = (entry_t){0};
    atomic_store(&s_view[n - 1], NULL);
}

app_profiles_err_t app_profiles_put_live(app_profile_t *p, int *index) {
    int i = find(p->id);
    if (i < 0) {
        i = atomic_load(&s_count);
        if (i >= APP_PROFILES_MAX) {
            profile_json_free(p);
            return APP_PROFILES_ERR_FULL;
        }
        s_e[i] = (entry_t){0};
        s_e[i].live = p;
        publish(i); // the view before the count, so a reader never sees an empty slot
        atomic_store(&s_count, i + 1);
    } else {
        retire(s_e[i].live);
        s_e[i].live = p;
        publish(i);
    }
    if (index) *index = i;
    changed();
    return APP_PROFILES_OK;
}

app_profiles_err_t app_profiles_save(int index) {
    if (!index_ok(index)) return APP_PROFILES_ERR_INDEX;
    entry_t *e = &s_e[index];
    if (e->live == NULL) return APP_PROFILES_OK;
    size_t len = 0;
    char *text = profile_json_write(e->live, &len);
    bool ok = text && profile_store_write(e->live->id, text, len);
    free(text);
    if (!ok) return APP_PROFILES_ERR_STORAGE;
    retire(e->stored);
    e->stored = e->live;
    e->live = NULL;
    publish(index); // same pointer as before: readers don't notice
    changed();
    return APP_PROFILES_OK;
}

app_profiles_err_t app_profiles_revert(int index, bool *removed) {
    if (removed) *removed = false;
    if (!index_ok(index)) return APP_PROFILES_ERR_INDEX;
    entry_t *e = &s_e[index];
    if (e->live == NULL) return APP_PROFILES_OK;
    retire(e->live);
    e->live = NULL;
    if (e->builtin == NULL && e->stored == NULL) {
        drop(index);
        if (removed) *removed = true;
    } else {
        publish(index);
    }
    changed();
    return APP_PROFILES_OK;
}

app_profiles_err_t app_profiles_remove(int index, bool *removed) {
    if (removed) *removed = false;
    if (!index_ok(index)) return APP_PROFILES_ERR_INDEX;
    entry_t *e = &s_e[index];
    if (e->stored && !profile_store_delete(e->stored->id)) return APP_PROFILES_ERR_STORAGE;
    retire(e->stored);
    retire(e->live);
    e->stored = e->live = NULL;
    if (e->builtin == NULL) {
        drop(index);
        if (removed) *removed = true;
    } else {
        publish(index);
    }
    changed();
    return APP_PROFILES_OK;
}
