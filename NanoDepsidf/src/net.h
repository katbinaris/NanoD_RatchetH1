#pragma once

#include <stdbool.h>
#include <stdint.h>

// WiFi, as a station: the clock (SNTP) and the knob's name on the network (mDNS). Off until
// set up over USB (EXT_CMD_NET, ext_link.c); the SSID, password and on/off live in NVS.
//
// Core 0 is the motor loop's, so everything WiFi runs on core 1: the radio task and lwIP by
// sdkconfig, and esp_wifi_init() is called from a core-1 task so the radio's interrupt lands
// there too. One thing can't move: ESP-IDF's event task is pinned to core 0, at the control
// task's priority -- so the event handlers here only note what happened and wake net_task.

#define NET_SSID_MAX 32
#define NET_PASS_MAX 63
#define NET_HOST_MAX 23

typedef enum {
    NET_OFF = 0,     // not set up, or switched off
    NET_CONNECTING,  // trying (again, after a drop: with a growing pause)
    NET_CONNECTED,   // has an address
    NET_NO_SSID,     // the network isn't in range
    NET_BAD_PASS,    // refused the password
} net_state_t;

typedef struct {
    net_state_t state;
    bool enabled;
    bool time_set;   // SNTP has set the clock
    int8_t rssi;     // dBm, while connected
    uint32_t ip;     // IPv4, network order; 0 = none
    char ssid[NET_SSID_MAX + 1];
    char host[NET_HOST_MAX + 1]; // the mDNS name (<host>.local)
} net_status_t;

void net_start(void); // app_main, after nvs_flash_init(): loads the setup, connects if it's on
void net_status(net_status_t *out);
// Store the setup and apply it (usb task). NULL ssid / pass = keep the stored one.
bool net_configure(const char *ssid, const char *pass, bool enabled);
// What the radio adds to the board's draw while it's on (the LED power budget, SYS INFO).
float net_current_ma(void);
