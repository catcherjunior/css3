cpp
// menu.cpp — imgui menu | INSERT to toggle
#include "menu.h"
#include "config.h"
#include <imgui.h>

void Menu::Draw() {
    ImGui::SetNextWindowSize({320, 300}, ImGuiCond_Once);
    ImGui::SetNextWindowPos({40, 40},   ImGuiCond_Once);
    ImGui::Begin("clientmod  |  INSERT = toggle", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    if (ImGui::BeginTabBar("##tabs")) {

        // ── Bhop ─────────────────────────────────────────────
        if (ImGui::BeginTabItem("Bhop")) {
            ImGui::Spacing();
            ImGui::Checkbox("Enabled##bh", &g_cfg.bhop_enabled);
            ImGui::Separator();
            ImGui::BeginDisabled(!g_cfg.bhop_enabled);
            ImGui::SliderInt("Chance %", &g_cfg.bhop_chance, 1, 100);
            ImGui::TextDisabled("100 = every frame  |  ~65 = human");
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        // ── ESP ──────────────────────────────────────────────
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Spacing();
            ImGui::Checkbox("Enable ESP",    &g_cfg.esp_enabled);
            ImGui::Separator();
            ImGui::BeginDisabled(!g_cfg.esp_enabled);
            ImGui::Checkbox("Boxes",         &g_cfg.esp_boxes);
            ImGui::Checkbox("Health bars",   &g_cfg.esp_health);
            ImGui::Checkbox("Weapon names",  &g_cfg.esp_weapons);
            ImGui::Checkbox("Snaplines",     &g_cfg.esp_snapline);
            ImGui::Checkbox("Distance",      &g_cfg.esp_distance);
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        // ── Aimbot ───────────────────────────────────────────
        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::Spacing();
            ImGui::Checkbox("Enabled##aim",  &g_cfg.aim_enabled);
            ImGui::Separator();
            ImGui::BeginDisabled(!g_cfg.aim_enabled);
            ImGui::SliderFloat("FOV",        &g_cfg.aim_fov,    1.f, 45.f, "%.1f");
            ImGui::SliderFloat("Smooth",     &g_cfg.aim_smooth, 1.f, 20.f, "%.1f");
            ImGui::TextDisabled("Smooth: higher = slower/more human");
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}