cpp
// hooks.cpp — minhook EndScene/Reset/CreateMove + WndProc | CS:S OB v34 | 32-bit | MSVC C++20
#include "hooks.h"
#include "config.h"
#include "features.h"
#include "menu.h"
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ── imgui init ────────────────────────────────────────────────
static void InitImGui(IDirect3DDevice9* dev) {
    Hooks::g_Device = dev;
    Hooks::g_Hwnd   = FindWindowA("Valve001", nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = s.FrameRounding = s.GrabRounding = s.TabRounding = 5.f;
    s.WindowBorderSize = 1.f;
    s.Colors[ImGuiCol_WindowBg]      = {0.08f, 0.08f, 0.09f, 0.94f};
    s.Colors[ImGuiCol_TitleBgActive] = {0.15f, 0.15f, 0.17f, 1.00f};
    s.Colors[ImGuiCol_SliderGrab]    = {0.40f, 0.60f, 1.00f, 1.00f};
    s.Colors[ImGuiCol_CheckMark]     = {0.40f, 0.80f, 0.40f, 1.00f};

    ImGuiIO& io  = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr;

    ImGui_ImplWin32_Init(Hooks::g_Hwnd);
    ImGui_ImplDX9_Init(dev);
    Hooks::g_ImgInit = true;
}

// ── EndScene ──────────────────────────────────────────────────
static HRESULT __stdcall hEndScene(IDirect3DDevice9* dev) {
    if (!Hooks::g_ImgInit) InitImGui(dev);

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_cfg.esp_enabled && iface::engine && iface::engine->IsInGame())
        Features::DrawESP();

    if (g_cfg.menu_open)
        Menu::Draw();

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    return Hooks::oEndScene(dev);
}

// ── Reset ─────────────────────────────────────────────────────
static HRESULT __stdcall hReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    if (Hooks::g_ImgInit) ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = Hooks::oReset(dev, pp);
    if (Hooks::g_ImgInit && SUCCEEDED(hr)) ImGui_ImplDX9_CreateDeviceObjects();
    return hr;
}

// ── WndProc ───────────────────────────────────────────────────
static LRESULT CALLBACK hWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_cfg.menu_open = !g_cfg.menu_open;
        ShowCursor(g_cfg.menu_open);
        if (g_cfg.menu_open) ClipCursor(nullptr);
    }
    if (g_cfg.menu_open) {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        if (ImGui::GetIO().WantCaptureMouse) return 0;
    }
    return CallWindowProcA(Hooks::oWndProc, hWnd, msg, wParam, lParam);
}

// ── CreateMove ────────────────────────────────────────────────
static bool __fastcall hCreateMove(void* ecx, void*, float dt, CUserCmd* cmd) {
    bool r = Hooks::oCreateMove(ecx, dt, cmd);
    if (cmd) {
        Features::Bhop(cmd);
        Features::Aimbot(cmd);
    }
    return r;
}

// ── get D3D9 device vtable via dummy window ───────────────────
static IDirect3DDevice9* GetD3D9Device() {
    HWND tmp = CreateWindowExA(0, "STATIC", "", WS_POPUP | WS_MINIMIZE,
                               0, 0, 2, 2, nullptr, nullptr,
                               GetModuleHandleA(nullptr), nullptr);
    if (!tmp) return nullptr;

    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) { DestroyWindow(tmp); return nullptr; }

    auto D3DCreate = (IDirect3D9*(*)(UINT))GetProcAddress(d3d9, "Direct3DCreate9");
    IDirect3D9* d3d = D3DCreate(D3D_SDK_VERSION);
    if (!d3d) { DestroyWindow(tmp); return nullptr; }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed      = TRUE;
    pp.SwapEffect    = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = tmp;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, tmp,
                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    d3d->Release();
    if (FAILED(hr) || !dev) { DestroyWindow(tmp); return nullptr; }

    DestroyWindow(tmp);
    return dev;
}

// ── Hooks::Init ───────────────────────────────────────────────
bool Hooks::Init() {
    MH_Initialize();

    // --- D3D9 hooks ---
    IDirect3DDevice9* dev = GetD3D9Device();
    if (!dev) return false;

    void** vt = *(void***)dev;

    MH_CreateHook(vt[42], (void*)hEndScene,
                  reinterpret_cast<void**>(&oEndScene));
    MH_CreateHook(vt[16], (void*)hReset,
                  reinterpret_cast<void**>(&oReset));
    MH_EnableHook(vt[42]);
    MH_EnableHook(vt[16]);

    dev->Release();

    // --- WndProc ---
    g_Hwnd  = FindWindowA("Valve001", nullptr);
    if (g_Hwnd)
        oWndProc = (WndProc_t)SetWindowLongPtrA(
            g_Hwnd, GWLP_WNDPROC, (LONG_PTR)hWndProc);

    return true;
}

// ── Hooks::InitCreateMove ─────────────────────────────────────
// called after clientmode is found
bool InitCreateMove_Internal() {
    if (!iface::client_mode) return false;
    void** vt = *(void***)iface::client_mode;
    MH_CreateHook(vt[24], (void*)hCreateMove,
                  reinterpret_cast<void**>(&Hooks::oCreateMove));
    MH_EnableHook(vt[24]);
    return Hooks::oCreateMove != nullptr;
}

// ── Hooks::Shutdown ───────────────────────────────────────────
void Hooks::Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_Hwnd && oWndProc)
        SetWindowLongPtrA(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);

    if (g_ImgInit) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ImgInit = false;
    }
}
