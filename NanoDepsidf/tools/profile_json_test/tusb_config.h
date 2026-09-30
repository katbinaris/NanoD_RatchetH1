// Just enough TinyUSB config for the host test to include class/hid/hid.h (the key codes).
#pragma once
#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUD_ENABLED 0
