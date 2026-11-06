cpp
// features.cpp — bhop, esp, aimbot | CS:S OB v34 | 32-bit | MSVC C++20
#include "features.h"
#include "config.h"
#include "netvars.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>

static constexpr float M_PI_F = 3.14159265358979323846f;

// ── W2S ───────────────────────────────────────────────────────
bool Features::W2S(const Vector& w, float& sx, float& sy) {
    if (!iface::engine) return false;
    int sw, sh;
    iface::engine->GetScreenSize(sw, sh);
    const VMatrix& m = iface::engine->WorldToScreenMatrix();

    float clip_w = m.m[3][0]*w.x + m.m[3][1]*w.y + m.m[3][2]*w.z + m.m[3][3];
    if (clip_w < 0.001f) return false;

    float nx = (m.m[0][0]*w.x + m.m[0][1]*w.y + m.m[0][2]*w.z + m.m[0][3]) / clip_w;
    float ny = (m.m[1][0]*w.x + m.m[1][1]*w.y + m.m[1][2]*w.z + m.m[1][3]) / clip_w;

    sx = sw * 0.5f + nx * (sw * 0.5f);
    sy = sh * 0.5f - ny * (sh * 0.5f);
    return true;
}

// ── Bhop ──────────────────────────────────────────────────────
void Features::Bhop(CUserCmd* cmd) {
    if (!g_cfg.bhop_enabled)       return;
    if (!(cmd->buttons & IN_JUMP)) return;
    if (!iface::engine || !iface::entlist) return;
    if (!g_off.m_fFlags) return;

    auto* local = iface::entlist->GetClientEntity(iface::engine->GetLocalPlayer());
    if (!local) return;

    int flags = local->At<int>(g_off.m_fFlags);
    if (!(flags & FL_ONGROUND)) {
        static std::mt19937 rng{ std::random_device{}() };
        static std::uniform_int_distribution<int> dist(1, 100);
        if (dist(rng) > g_cfg.bhop_chance)
            cmd->buttons &= ~IN_JUMP;
    }
}

// ── Aimbot ────────────────────────────────────────────────────
void Features::Aimbot(CUserCmd* cmd) {
    if (!g_cfg.aim_enabled) return;
    if (!iface::engine || !iface::entlist) return;
    if (!g_off.m_vecOrigin || !g_off.m_iHealth ||
        !g_off.m_lifeState || !g_off.m_angEyeAngles) return;

    int   local_idx  = iface::engine->GetLocalPlayer();
    auto* local      = iface::entlist->GetClientEntity(local_idx);
    if (!local) return;

    int local_team = g_off.m_iTeamNum ? local->At<int>(g_off.m_iTeamNum) : 0;

    // get local eye position
    Vector local_origin = local->At<Vector>(g_off.m_vecOrigin);
    Vector view_offset  = g_off.m_vecViewOffset
                          ? local->At<Vector>(g_off.m_vecViewOffset)
                          : Vector{0,0,64.f};
    Vector eye_pos = { local_origin.x + view_offset.x,
                       local_origin.y + view_offset.y,
                       local_origin.z + view_offset.z };

    // current view angles from cmd
    float best_fov  = g_cfg.aim_fov;
    IClientEntity* best_ent = nullptr;
    Vector         best_head{};

    int max_ent = iface::entlist->GetHighestEntityIndex();
    for (int i = 1; i <= max_ent; ++i) {
        if (i == local_idx) continue;
        auto* ent = iface::entlist->GetClientEntity(i);
        if (!ent) continue;

        ClientClass* cc = ent->GetClientClass();
        if (!cc || !cc->m_pNetworkName) continue;
        if (!strstr(cc->m_pNetworkName, "CSPlayer")) continue;

        if (ent->At<uint8_t>(g_off.m_lifeState) != LIFE_ALIVE) continue;
        if (ent->At<int>(g_off.m_iHealth) <= 0) continue;
        if (g_off.m_iTeamNum && ent->At<int>(g_off.m_iTeamNum) == local_team) continue;

        Vector feet = ent->At<Vector>(g_off.m_vecOrigin);
        Vector head = { feet.x, feet.y, feet.z + 62.f }; // approx head

        // delta from eye to target head
        Vector delta = { head.x - eye_pos.x,
                         head.y - eye_pos.y,
                         head.z - eye_pos.z };
        float dist = delta.length();
        if (dist < 0.001f) continue;

        // angle to target
        float pitch = -asinf(delta.z / dist) * (180.f / M_PI_F);
        float yaw   =  atan2f(delta.y, delta.x) * (180.f / M_PI_F);

        // fov check
        float dp = pitch - cmd->viewangles.pitch;
        float dy = yaw   - cmd->viewangles.yaw;
        // normalize yaw delta
        while (dy > 180.f)  dy -= 360.f;
        while (dy < -180.f) dy += 360.f;

        float fov = sqrtf(dp*dp + dy*dy);
        if (fov < best_fov) {
            best_fov  = fov;
            best_ent  = ent;
            best_head = head;
        }
    }

    if (!best_ent) return;

    // recompute angles to best target
    Vector delta = { best_head.x - eye_pos.x,
                     best_head.y - eye_pos.y,
                     best_head.z - eye_pos.z };
    float dist = delta.length();
    float t_pitch = -asinf(delta.z / dist) * (180.f / M_PI_F);
    float t_yaw   =  atan2f(delta.y, delta.x) * (180.f / M_PI_F);

    float dp = t_pitch - cmd->viewangles.pitch;
    float dy = t_yaw   - cmd->viewangles.yaw;
    while (dy > 180.f)  dy -= 360.f;
    while (dy < -180.f) dy += 360.f;

    // smooth
    cmd->viewangles.pitch += dp / g_cfg.aim_smooth;
    cmd->viewangles.yaw   += dy / g_cfg.aim_smooth;
}

// ── ESP ───────────────────────────────────────────────────────
void Features::DrawESP() {
    if (!g_cfg.esp_enabled) return;
    if (!iface::entlist || !iface::engine) return;
    if (!g_off.m_iHealth || !g_off.m_lifeState || !g_off.m_vecOrigin) return;

    ImDrawList* dl    = ImGui::GetBackgroundDrawList();
    int   local_idx   = iface::engine->GetLocalPlayer();
    auto* local_ent   = iface::entlist->GetClientEntity(local_idx);
    int   local_team  = (local_ent && g_off.m_iTeamNum)
                        ? local_ent->At<int>(g_off.m_iTeamNum) : 0;
    int   max_ent     = iface::entlist->GetHighestEntityIndex();

    int sw, sh;
    iface::engine->GetScreenSize(sw, sh);
    float cx = sw * 0.5f, cy = sh * 0.5f;

    for (int i = 1; i <= max_ent; ++i) {
        if (i == local_idx) continue;
        auto* ent = iface::entlist->GetClientEntity(i);
        if (!ent) continue;

        ClientClass* cc = ent->GetClientClass();
        if (!cc || !cc->m_pNetworkName) continue;
        if (!strstr(cc->m_pNetworkName, "CSPlayer")) continue;

        if (ent->At<uint8_t>(g_off.m_lifeState) != LIFE_ALIVE) continue;
        int hp = ent->At<int>(g_off.m_iHealth);
        if (hp <= 0) continue;

        Vector feet = ent->At<Vector>(g_off.m_vecOrigin);
        Vector head = { feet.x, feet.y, feet.z + 72.f };

        float fx, fy, hx, hy;
        if (!W2S(feet, fx, fy) || !W2S(head, hx, hy)) continue;

        float height = fy - hy;
        if (height < 2.f) continue;
        float width  = height * 0.4f;
        float left   = hx - width * 0.5f;
        float right  = hx + width * 0.5f;
        float top    = hy, bottom = fy;

        int team  = g_off.m_iTeamNum ? ent->At<int>(g_off.m_iTeamNum) : 0;
        bool enemy = (team != local_team);
        ImU32 col  = enemy ? IM_COL32(220, 50,  50,  230)
                           : IM_COL32(50,  210, 50,  230);

        // box
        if (g_cfg.esp_boxes) {
            dl->AddRect({left-1,top-1},{right+1,bottom+1}, IM_COL32(0,0,0,160));
            dl->AddRect({left,  top  },{right,  bottom  }, col);
        }

        // health bar
        if (g_cfg.esp_health) {
            float ratio   = (float)hp / 100.f;
            if (ratio > 1.f) ratio = 1.f;
            float bx      = left - 5.f;
            float bar_top = bottom - height * ratio;
            dl->AddRectFilled({bx-1,top    },{bx+3,bottom }, IM_COL32(0,0,0,160));
            ImU32 hcol = IM_COL32(
                (uint8_t)((1.f-ratio)*255.f),
                (uint8_t)(ratio*210.f),
                0, 230);
            dl->AddRectFilled({bx-1,bar_top},{bx+3,bottom }, hcol);
            char buf[8]; snprintf(buf, sizeof(buf), "%d", hp);
            dl->AddText({left, bottom+2.f}, IM_COL32(220,220,220,220), buf);
        }

        // weapon name
        if (g_cfg.esp_weapons && g_off.m_hActiveWeapon) {
            int handle = ent->At<int>(g_off.m_hActiveWeapon);
            int widx   = handle & 0xFFF;
            if (widx > 0 && widx <= max_ent) {
                auto* w = iface::entlist->GetClientEntity(widx);
                if (w) {
                    ClientClass* wcc = w->GetClientClass();
                    if (wcc && wcc->m_pNetworkName) {
                        const char* wn = wcc->m_pNetworkName;
                        if (!strncmp(wn, "CWeapon", 7)) wn += 7;
                        dl->AddText({left, bottom+13.f},
                                    IM_COL32(190,190,190,210), wn);
                    }
                }
            }
        }

        // snapline
        if (g_cfg.esp_snapline) {
            dl->AddLine({cx, cy}, {hx, hy}, col, 1.f);
        }

        // distance
        if (g_cfg.esp_distance && local_ent) {
            Vector lp = local_ent->At<Vector>(g_off.m_vecOrigin);
            float  d  = (feet - lp).length() * 0.01905f; // units -> meters
            char   buf[16]; snprintf(buf, sizeof(buf), "%.0fm", d);
            dl->AddText({left, top - 13.f}, IM_COL32(200,200,200,200), buf);
        }
    }
}