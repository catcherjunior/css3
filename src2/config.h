cpp
// config.h — runtime toggles
#pragma once

struct Config {
    // bhop
    bool bhop_enabled = true;
    int  bhop_chance  = 65;

    // esp
    bool esp_enabled  = true;
    bool esp_boxes    = true;
    bool esp_health   = true;
    bool esp_weapons  = true;
    bool esp_snapline = false;
    bool esp_distance = false;

    // aimbot
    bool aim_enabled  = false;
    float aim_fov     = 8.f;   // degrees radius
    float aim_smooth  = 6.f;   // higher = slower/more human
    bool aim_visible  = false; // visible check (not impl yet, placeholder)

    // misc
    bool menu_open    = false;
};

inline Config g_cfg;