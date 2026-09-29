// Blender -- not defined yet: the empty template (the knob scrolls). To be designed on its own,
// as Blender works differently from the other CAD profiles (DEVELOPMENT_PLAN.md).
#include "app_profile.h"
#include "icons/app_icons.h"
#include "class/hid/hid.h"

const app_profile_t app_profile_blender =
    APP_PROFILE_EMPTY_ICON("blender", "BLENDER", app_icon_blender_24, app_icon_blender_48);
