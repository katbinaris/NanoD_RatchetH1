// AutoCAD -- not defined yet: the empty template (the knob scrolls). To be designed on its own,
// as AutoCAD works differently from the other CAD profiles (DEVELOPMENT_PLAN.md).
#include "app_profile.h"
#include "icons/app_icons.h"
#include "class/hid/hid.h"

const app_profile_t app_profile_autocad =
    APP_PROFILE_EMPTY_ICON("autocad", "AUTOCAD", app_icon_autocad_24, app_icon_autocad_48);
