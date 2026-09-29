// The fallback profile: the empty template (app_profile.h APP_PROFILE_EMPTY), used by the
// registry in place of a profile that is missing or fails its checks. Not in the PROFILE list.
#include "app_profile.h"
#include "icons/app_icons.h"
#include "class/hid/hid.h"

const app_profile_t app_profile_empty = APP_PROFILE_EMPTY("empty", "EMPTY");
