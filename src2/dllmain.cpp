cpp
// dllmain.cpp — d3d9 proxy boot | CS:S OB v34 | 32-bit DLL | MSVC C++20
// Rename output to d3d9.dll, drop next to CMLauncher.exe
#include "sdk.h"
#include "config.h"
#include "netvars.h"
#include "hooks.h"

static HMODULE g_RealD3D9 = nullptr;

static void LoadRealD3D9() {
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\d3d9.dll");
    g_RealD3D9 = LoadLibraryA(path);
}

// ── d3d9 export forwards ──────────────────────────────────────
extern "C" {
    __declspec(dllexport) void* __stdcall Direct3DCreate9(UINT sdk) {
        static auto fn = (void*(__stdcall*)(UINT))
            GetProcAddress(g_RealD3D9, "Direct3DCreate9");
        return fn(sdk);
    }
    __declspec(dllexport) HRESULT __stdcall Direct3DCreate9Ex(UINT sdk, void** pp) {
        static auto fn = (HRESULT(__stdcall*)(UINT, void**))
            GetProcAddress(g_RealD3D9, "Direct3DCreate9Ex");
        return fn ? fn(sdk, pp) : E_NOTIMPL;
    }
    __declspec(dllexport) int   __stdcall D3DPERF_BeginEvent(DWORD c, LPCWSTR n)           { return 0; }
    __declspec(dllexport) int   __stdcall D3DPERF_EndEvent()                                { return 0; }
    __declspec(dllexport) DWORD __stdcall D3DPERF_GetStatus()                               { return 0; }
    __declspec(dllexport) BOOL  __stdcall D3DPERF_QueryRepeatFrame()                        { return FALSE; }
    __declspec(dllexport) void  __stdcall D3DPERF_SetMarker(DWORD c, LPCWSTR n)            {}
    __declspec(dllexport) void  __stdcall D3DPERF_SetOptions(DWORD o)                      {}
    __declspec(dllexport) void  __stdcall D3DPERF_SetRegion(DWORD c, LPCWSTR n)            {}
    __declspec(dllexport) void  __stdcall DebugSetLevel(DWORD l)                           {}
    __declspec(dllexport) void  __stdcall DebugSetMute(BOOL m)                             {}
    __declspec(dllexport) void  __stdcall Direct3D9EnableMaximizedWindowedModeShim(BOOL e) {}
}

// ── clientmode scan ───────────────────────────────────────────
static void* FindClientMode() {
    HMODULE h = GetModuleHandleA("client.dll");
    if (!h) return nullptr;
    MODULEINFO mi{};
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    auto* base = (uint8_t*)h;
    auto* end  = base + mi.SizeOfImage;
    for (auto* p = base; p < end - 9; ++p) {
        if (p[0] != 0x8B || p[1] != 0x0D) continue;
        if (p[6] != 0xFF || p[7] != 0x51) continue;
        uintptr_t ptr_addr = *(uintptr_t*)(p + 2);
        if (ptr_addr < (uintptr_t)h ||
            ptr_addr >= (uintptr_t)h + mi.SizeOfImage) continue;
        void* mode = *(void**)ptr_addr;
        if (!mode) continue;
        if (IsBadReadPtr(mode, sizeof(void*))) continue;
        void* vt = *(void**)mode;
        if (IsBadReadPtr(vt, sizeof(void*))) continue;
        return mode;
    }
    return nullptr;
}

// forward decl
bool InitCreateMove_Internal();

// ── cheat thread ──────────────────────────────────────────────
static DWORD WINAPI CheatThread(LPVOID) {
    while (!GetModuleHandleA("engine.dll") ||
           !GetModuleHandleA("client.dll"))
        Sleep(200);
    Sleep(400);

    HMODULE eng    = GetModuleHandleA("engine.dll");
    auto    engFac = (CreateInterfaceFn)GetProcAddress(eng, "CreateInterface");
    if (!engFac) return 1;

    iface::engine = (IVEngineClient*)engFac("VEngineClient013", nullptr);
    if (!iface::engine) return 1;

    HMODULE cdll      = GetModuleHandleA("client.dll");
    auto    clientFac = (CreateInterfaceFn)GetProcAddress(cdll, "CreateInterface");
    if (!clientFac) return 1;

    iface::client  = (IBaseClientDLL*)    clientFac("VClient017",           nullptr);
    iface::entlist = (IClientEntityList*) clientFac("VClientEntityList003", nullptr);
    if (!iface::client || !iface::entlist) return 1;

    InitNetvars(iface::client);

    while (!GetModuleHandleA("shaderapidx9.dll")) Sleep(100);
    Sleep(400);

    Hooks::Init();

    while (!iface::client_mode) {
        iface::client_mode = FindClientMode();
        if (iface::client_mode)
            InitCreateMove_Internal();
        else
            Sleep(500);
    }

    return 0;
}