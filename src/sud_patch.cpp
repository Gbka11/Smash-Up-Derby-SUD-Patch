// ============================================================================
//  SUD Patch v1.0 - unofficial patch for Smash Up Derby (City Interactive, 2003)
//  Author: Sponge (Gbka)
//  Multiplayer testing: Grzelciu and Yoshi - thanks!
//
//  A d3d8.dll proxy placed next to cars.exe. No game files are modified.
//
//  Fixes:
//    [Display]  Widescreen: removes the 4:3 pillarbox in races, Hor+ FOV
//    [Display]  Automatic desktop resolution instead of the default 640x480
//    [Display]  White-screen guard (broken DX7 driver copy in VirtualStore)
//    [Sound]    Game no longer forces its volume in the Windows mixer
//    [Network]  LAN server search on every network adapter (VPN: Radmin/Hamachi)
//    [Network]  Game UDP sockets listen on all adapters (VPN connections work)
//
//  Build (x86 Native Tools Command Prompt for VS):
//    cl /nologo /LD /O2 /MT /EHa sud_patch.cpp /Fe:d3d8.dll user32.lib
//
//  Install: copy d3d8.dll + SUD_Patch.ini next to cars.exe.
// ============================================================================

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

#define PATCH_NAME    "SUD Patch"
#define PATCH_VERSION "1.0"

// ----------------------------------------------------------------------------
// Minimal D3D8 types (d3d8.h is not shipped with current Windows SDKs)
// ----------------------------------------------------------------------------
struct VP8  { DWORD X, Y, Width, Height; float MinZ, MaxZ; };
struct MAT8 { float m[4][4]; };
struct PP8  {                                   // D3DPRESENT_PARAMETERS (D3D8)
    UINT  BackBufferWidth;
    UINT  BackBufferHeight;
    DWORD BackBufferFormat;
    UINT  BackBufferCount;
    DWORD MultiSampleType;
    DWORD SwapEffect;
    HWND  hDeviceWindow;
    BOOL  Windowed;
    BOOL  EnableAutoDepthStencil;
    DWORD AutoDepthStencilFormat;
    DWORD Flags;
    UINT  FullScreen_RefreshRateInHz;
    UINT  FullScreen_PresentationInterval;
};

#define D3DTS_PROJECTION 3

// vtable indices
#define IDX_D3D8_CREATEDEVICE   15
#define IDX_DEV8_RESET          14
#define IDX_DEV8_SETTRANSFORM   37
#define IDX_DEV8_SETVIEWPORT    40

typedef void*   (WINAPI *Direct3DCreate8_t)(UINT);
typedef HRESULT (WINAPI *CreateDevice_t)(void* self, UINT adapter, DWORD type, HWND focus,
                                         DWORD flags, PP8* pp, void** ppDev);
typedef HRESULT (WINAPI *Reset_t)(void* dev, PP8* pp);
typedef HRESULT (WINAPI *SetTransform_t)(void* dev, DWORD state, const MAT8* m);
typedef HRESULT (WINAPI *SetViewport_t)(void* dev, const VP8* vp);

// ----------------------------------------------------------------------------
// Settings (SUD_Patch.ini)
// ----------------------------------------------------------------------------
static int g_logOn       = 1;   // [General]  Log
static int g_widescreen  = 1;   // [Display]  Widescreen
static int g_dispW       = 0;   // [Display]  Width   (0 = desktop, -1 = keep game setting)
static int g_dispH       = 0;   // [Display]  Height
static int g_fixRegistry = 1;   // [Display]  FixRegistry
static int g_fixVolume   = 1;   // [Sound]    FixVolume
static int g_netAllIf    = 1;   // [Network]  AllInterfaces
static int g_netBcastToo = 1;   // [Network]  BroadcastToo
static int g_netBindAny  = 1;   // [Network]  BindAnyUDP

// ----------------------------------------------------------------------------
// State
// ----------------------------------------------------------------------------
static HMODULE           g_real          = NULL;
static Direct3DCreate8_t g_realCreate    = NULL;
static FARPROC           g_pDebugSetMute = NULL;
static FARPROC           g_pValidatePS   = NULL;
static FARPROC           g_pValidateVS   = NULL;

static CreateDevice_t oCreateDevice = NULL;
static Reset_t        oReset        = NULL;
static SetTransform_t oSetTransform = NULL;
static SetViewport_t  oSetViewport  = NULL;

static UINT  g_bbW = 0, g_bbH = 0;
static HWND  g_hwnd         = NULL;
static float g_scale        = 1.0f;     // projection _11 multiplier (1.0 = unchanged)
static MAT8  g_rawProj;
static int   g_haveProj     = 0;
static int   g_loggedPillar = 0;

static char  g_logPath[MAX_PATH];
static char  g_iniPath[MAX_PATH];

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------
static void Log(const char* fmt, ...)
{
    if (!g_logOn) return;
    FILE* f = fopen(g_logPath, "a");
    if (!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void PatchPtr(void** table, int idx, void* hook, void** orig)
{
    if (table[idx] == hook) return;
    DWORD old;
    VirtualProtect(&table[idx], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    if (orig && !*orig) *orig = table[idx];
    table[idx] = hook;
    VirtualProtect(&table[idx], sizeof(void*), old, &old);
}

static const char* IpStr(DWORD a, char* out)
{
    BYTE* b = (BYTE*)&a;
    sprintf_s(out, 16, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return out;
}

static unsigned PortHost(unsigned short netPort)
{
    return (unsigned)((netPort >> 8) | ((netPort & 0xFF) << 8));
}

// ============================================================================
// [Display] Widescreen
// The game keeps the camera at 4:3 and Virtools renders it into a centred
// 4:3 viewport (pillarbox). We widen the viewport to the full back buffer and
// scale the projection so the image is not stretched (Hor+).
// ============================================================================
static void UpdateBackBuffer(PP8* pp, HWND fallback)
{
    UINT w = pp ? pp->BackBufferWidth  : 0;
    UINT h = pp ? pp->BackBufferHeight : 0;
    HWND wnd = (pp && pp->hDeviceWindow) ? pp->hDeviceWindow : fallback;
    if (wnd) g_hwnd = wnd;
    if ((!w || !h) && g_hwnd) {
        RECT rc; GetClientRect(g_hwnd, &rc);
        w = rc.right - rc.left; h = rc.bottom - rc.top;
    }
    g_bbW = w; g_bbH = h;
    g_scale = 1.0f;
    g_loggedPillar = 0;
    Log("Display: back buffer %ux%u (windowed=%d)", g_bbW, g_bbH, pp ? pp->Windowed : -1);
}

static void ApplyScale(MAT8* m)
{
    // perspective projection only (_34 == 1, _44 == 0)
    if (m->m[2][3] == 1.0f && m->m[3][3] == 0.0f) {
        m->m[0][0] *= g_scale;
        m->m[2][0] *= g_scale;
    }
}

static HRESULT WINAPI hkSetTransform(void* dev, DWORD state, const MAT8* m)
{
    if (state == D3DTS_PROJECTION && m) {
        g_rawProj  = *m;
        g_haveProj = 1;
        if (g_scale != 1.0f) {
            MAT8 t = *m;
            ApplyScale(&t);
            return oSetTransform(dev, state, &t);
        }
    }
    return oSetTransform(dev, state, m);
}

static HRESULT WINAPI hkSetViewport(void* dev, const VP8* vp)
{
    if (!vp || !g_widescreen || !g_bbW || !g_bbH)
        return oSetViewport(dev, vp);

    VP8   v        = *vp;
    float newScale = 1.0f;

    // 4:3 pillarbox: full height, narrower, ~4:3, centred
    if (v.Height + 2 >= g_bbH && v.Width + 8 < g_bbW) {
        int diff43 = abs((int)(v.Width * 3) - (int)(v.Height * 4));
        int center = abs((int)(v.X * 2 + v.Width) - (int)g_bbW);
        if (diff43 <= 12 && center <= 8) {
            newScale = (float)v.Width / (float)g_bbW;
            if (!g_loggedPillar) {
                Log("Display: pillarbox X=%u W=%u H=%u -> 0..%u, scale=%.4f",
                    v.X, v.Width, v.Height, g_bbW, newScale);
                g_loggedPillar = 1;
            }
            v.X = 0;
            v.Width = g_bbW;
        }
    }

    HRESULT hr = oSetViewport(dev, &v);

    if (newScale != g_scale) {
        g_scale = newScale;
        if (g_haveProj) {
            MAT8 t = g_rawProj;
            if (g_scale != 1.0f) ApplyScale(&t);
            oSetTransform(dev, D3DTS_PROJECTION, &t);
        }
    }
    return hr;
}

// ============================================================================
// [Display] Automatic resolution
// cars.exe reads "screen width/height" from the registry into 0x40C034 /
// 0x40C038 (default 640x480) and switches to fullscreen only after the engine
// has loaded (after our DllMain), so we overwrite the values in memory.
// The game saves the new resolution to the registry on exit by itself.
// ============================================================================
#define CARS_VAR_WIDTH   0x0040C034
#define CARS_VAR_HEIGHT  0x0040C038
#define CARS_STR_WIDTH   0x0040C1C8   // "screen width"  - exe version check
#define CARS_STR_HEIGHT  0x0040C1B8   // "screen height"

static void ApplyResolution()
{
    if (g_dispW < 0 || g_dispH < 0) { Log("Display: resolution override disabled in ini"); return; }

    HMODULE exe = GetModuleHandleA(NULL);
    if ((DWORD)exe != 0x00400000 ||
        memcmp((void*)CARS_STR_WIDTH,  "screen width",  13) ||
        memcmp((void*)CARS_STR_HEIGHT, "screen height", 14)) {
        Log("Display: unknown cars.exe version - resolution left unchanged");
        return;
    }

    DWORD w = (DWORD)g_dispW, h = (DWORD)g_dispH;
    if (!w || !h) {
        DEVMODEA dm; memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
            w = dm.dmPelsWidth; h = dm.dmPelsHeight;          // real size, ignores DPI scaling
        } else {
            w = GetSystemMetrics(SM_CXSCREEN); h = GetSystemMetrics(SM_CYSCREEN);
        }
    }
    if (w < 640 || h < 480) { Log("Display: odd resolution %ux%u - skipped", w, h); return; }

    DWORD* pw = (DWORD*)CARS_VAR_WIDTH;
    DWORD* ph = (DWORD*)CARS_VAR_HEIGHT;
    Log("Display: game resolution %ux%u -> %ux%u", *pw, *ph, w, h);
    *pw = w; *ph = h;
}

// ============================================================================
// [Display] White-screen guard
// Without admin rights the game's registry writes go to HKCU VirtualStore and
// are read back from there instead of HKLM. After a failed start the game may
// save a DX7 driver there, which gives a white screen from then on.
// If the stored driver is not DX8, the virtual copy is deleted.
// ============================================================================
#define VS_PARENT "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\tatanka"

static void CheckVirtualStore(const char* when)
{
    HKEY h;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, VS_PARENT "\\cars", 0,
                      KEY_READ | KEY_WOW64_64KEY, &h) != ERROR_SUCCESS)
        return;                                                // no copy - nothing to do

    char  drv[256] = "";
    DWORD type = 0, size = sizeof(drv) - 1;
    LONG  r = RegQueryValueExA(h, "driver name", NULL, &type, (BYTE*)drv, &size);
    RegCloseKey(h);

    if (r == ERROR_SUCCESS && type == REG_SZ && strstr(drv, "DX8")) {
        Log("Registry [%s]: driver OK (%s)", when, drv);
        return;
    }

    Log("Registry [%s]: bad driver '%s' - deleting the VirtualStore copy", when,
        r == ERROR_SUCCESS ? drv : "(none)");
    HKEY p;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, VS_PARENT, 0,
                      KEY_ALL_ACCESS | KEY_WOW64_64KEY, &p) == ERROR_SUCCESS) {
        LONG d = RegDeleteTreeA(p, "cars");
        RegCloseKey(p);
        Log("Registry [%s]: RegDeleteTree -> %ld", when, d);
    }
}

// ============================================================================
// [Sound] Volume fix
// The game (Virtools DX7SoundManager) sets the volume of the PRIMARY
// DirectSound buffer on every scene load. Since Vista, Windows maps that to
// the application's slider in the volume mixer (~81%). We block SetVolume on
// the primary buffer only; secondary buffers (music, effects) are untouched.
// ============================================================================
struct DSBDESC1 { DWORD dwSize, dwFlags, dwBufferBytes, dwReserved; void* lpwfxFormat; };
struct DSBCAPS1 { DWORD dwSize, dwFlags, dwBufferBytes, dwUnlockTransferRate, dwPlayCpuOverhead; };
#define DSBCAPS_PRIMARYBUFFER_   0x00000001
#define DSSCL_PRIORITY_          2
#define IDX_DS_CREATESOUNDBUFFER 3
#define IDX_DS_SETCOOPLEVEL      6
#define IDX_DSB_GETCAPS          3
#define IDX_DSB_SETVOLUME        15

typedef HRESULT (WINAPI *DirectSoundCreate_t)(const void* guid, void** ppDS, void* outer);
typedef HRESULT (WINAPI *DSCreateBuffer_t)(void* ds, const DSBDESC1* d, void** ppBuf, void* outer);
typedef HRESULT (WINAPI *DSSetCoop_t)(void* ds, HWND wnd, DWORD level);
typedef HRESULT (WINAPI *DSBGetCaps_t)(void* buf, DSBCAPS1* caps);
typedef HRESULT (WINAPI *DSBSetVolume_t)(void* buf, LONG vol);
typedef ULONG   (WINAPI *ComRelease_t)(void*);

static DSBSetVolume_t oDSBSetVolume = NULL;
static int  g_soundHooked = 0;
static int  g_volLogCount = 0;

static HRESULT WINAPI hkDSBSetVolume(void* buf, LONG vol)
{
    DSBCAPS1 caps = { sizeof(DSBCAPS1) };
    DSBGetCaps_t getCaps = (DSBGetCaps_t)(*(void***)buf)[IDX_DSB_GETCAPS];
    int primary = (getCaps(buf, &caps) >= 0) && (caps.dwFlags & DSBCAPS_PRIMARYBUFFER_);

    if (primary) {
        if (g_volLogCount < 20) {
            Log("Sound: primary SetVolume(%ld) -> %s", vol, g_fixVolume ? "blocked" : "passed");
            g_volLogCount++;
        }
        if (g_fixVolume) return 0;                              // DS_OK, nothing changed
    }
    return oDSBSetVolume(buf, vol);
}

static void InstallSoundHook(HWND wnd)
{
    if (g_soundHooked || !g_fixVolume) return;
    g_soundHooked = 1;

    HMODULE ds = LoadLibraryA("dsound.dll");
    DirectSoundCreate_t create = ds ? (DirectSoundCreate_t)GetProcAddress(ds, "DirectSoundCreate") : NULL;
    if (!create) { Log("Sound: DirectSoundCreate not found"); return; }

    void* dso = NULL;
    if (create(NULL, &dso, NULL) < 0 || !dso) { Log("Sound: DirectSoundCreate failed"); return; }
    void** dsv = *(void***)dso;

    if (((DSSetCoop_t)dsv[IDX_DS_SETCOOPLEVEL])(dso, wnd ? wnd : GetDesktopWindow(), DSSCL_PRIORITY_) < 0)
        Log("Sound: SetCooperativeLevel failed (continuing)");

    DSBDESC1 d = { sizeof(DSBDESC1), DSBCAPS_PRIMARYBUFFER_, 0, 0, NULL };
    void* prim = NULL;
    HRESULT hr = ((DSCreateBuffer_t)dsv[IDX_DS_CREATESOUNDBUFFER])(dso, &d, &prim, NULL);
    if (hr >= 0 && prim) {
        void** bv = *(void***)prim;
        PatchPtr(bv, IDX_DSB_SETVOLUME, (void*)hkDSBSetVolume, (void**)&oDSBSetVolume);
        Log("Sound: primary buffer SetVolume hooked");
        ((ComRelease_t)bv[2])(prim);
    } else Log("Sound: CreateSoundBuffer(primary) failed 0x%08X", (unsigned)hr);

    ((ComRelease_t)dsv[2])(dso);
}

// ============================================================================
// [Network] Multi-adapter fixes (nk2.dll / Virtools Server)
//  Ports: TCP 18005 game server, UDP 18023 LAN search
//         (multicast 225.226.227.228), plus OS-assigned UDP ports for game data.
//  Problems on modern Windows with several adapters (LAN + VPN):
//   - JoinGroup uses imr_interface = INADDR_ANY -> one adapter only,
//   - search query is sent without IP_MULTICAST_IF -> one adapter only,
//   - the server binds its UDP data socket to one adapter address, so VPN
//     traffic (e.g. Radmin 26.x.x.x) is dropped and the connection closes.
//  Fix: hook ws2_32 imports of nk2.dll (ordinals: 2 bind, 20 sendto, 21 setsockopt).
// ============================================================================
typedef unsigned int SOCK;
struct SIN     { short family; unsigned short port; DWORD addr; char zero[8]; };
struct MREQ    { DWORD multiaddr; DWORD iface; };
struct IPROW   { DWORD dwAddr, dwIndex, dwMask, dwBCastAddr, dwReasmSize; unsigned short unused1, wType; };
struct IPTABLE { DWORD dwNumEntries; IPROW table[1]; };

#define IPPROTO_IP_          0
#define IP_MULTICAST_IF_     9
#define IP_ADD_MEMBERSHIP_   12

typedef int   (WINAPI *setsockopt_t)(SOCK, int, int, const char*, int);
typedef int   (WINAPI *getsockopt_t)(SOCK, int, int, char*, int*);
typedef int   (WINAPI *sendto_t)(SOCK, const char*, int, int, const SIN*, int);
typedef int   (WINAPI *bind_t)(SOCK, const SIN*, int);
typedef DWORD (WINAPI *GetIpAddrTable_t)(IPTABLE*, ULONG*, BOOL);

static setsockopt_t oSetsockopt = NULL;
static sendto_t     oSendto     = NULL;
static bind_t       oBind       = NULL;
static getsockopt_t pGetsockopt = NULL;
static int  g_netPatched = 0;
static int  g_sendLog    = 0;

#define MAX_IF 16
static DWORD g_ifAddr[MAX_IF], g_ifMask[MAX_IF];
static int   g_ifCount = 0;
static DWORD g_ifTick  = 0;

static void RefreshInterfaces()
{
    DWORD now = GetTickCount();
    if (g_ifCount && now - g_ifTick < 5000) return;
    g_ifTick = now;

    static GetIpAddrTable_t pGet = NULL;
    if (!pGet) {
        HMODULE h = LoadLibraryA("iphlpapi.dll");
        if (h) pGet = (GetIpAddrTable_t)GetProcAddress(h, "GetIpAddrTable");
        if (!pGet) { Log("Net: GetIpAddrTable not found"); return; }
    }
    char buf[8192]; ULONG sz = sizeof(buf);
    IPTABLE* t = (IPTABLE*)buf;
    if (pGet(t, &sz, FALSE) != 0) { Log("Net: GetIpAddrTable failed"); return; }

    g_ifCount = 0;
    for (DWORD i = 0; i < t->dwNumEntries && g_ifCount < MAX_IF; i++) {
        DWORD a = t->table[i].dwAddr;                           // network byte order
        BYTE* b = (BYTE*)&a;
        if (a == 0 || b[0] == 127) continue;                    // skip 0.0.0.0 and loopback
        if (t->table[i].wType & 0x0008) continue;               // MIB_IPADDR_DISCONNECTED
        g_ifAddr[g_ifCount] = a;
        g_ifMask[g_ifCount] = t->table[i].dwMask;
        g_ifCount++;
    }
}

// Server: join the LAN-search multicast group on every adapter
static int WINAPI hkSetsockopt(SOCK s, int level, int name, const char* val, int len)
{
    if (g_netAllIf && level == IPPROTO_IP_ && name == IP_ADD_MEMBERSHIP_ && val && len >= 8) {
        const MREQ* m = (const MREQ*)val;
        if (m->iface == 0) {
            RefreshInterfaces();
            int ok = 0; char ip1[16], ip2[16];
            for (int i = 0; i < g_ifCount; i++) {
                MREQ r = { m->multiaddr, g_ifAddr[i] };
                int rc = oSetsockopt(s, level, name, (const char*)&r, sizeof(r));
                Log("Net: join %s on %s -> %s", IpStr(m->multiaddr, ip1), IpStr(g_ifAddr[i], ip2),
                    rc == 0 ? "OK" : "failed");
                if (rc == 0) ok = 1;
            }
            if (ok) return 0;
            Log("Net: join failed on all adapters - using the original call");
        }
    }
    return oSetsockopt(s, level, name, val, len);
}

// Client: send the LAN-search query through every adapter
// (multicast + subnet broadcast, since VPNs like Radmin may not carry multicast)
static int WINAPI hkSendto(SOCK s, const char* buf, int len, int flags, const SIN* to, int tolen)
{
    if (g_netAllIf && to && tolen >= 8 && to->family == 2) {
        BYTE* b = (BYTE*)&to->addr;
        int mcast = (b[0] >= 224 && b[0] <= 239);
        int bcast = (to->addr == 0xFFFFFFFF);
        if (mcast || bcast) {
            RefreshInterfaces();
            int sent = -1; char ip1[16], ip2[16];
            for (int i = 0; i < g_ifCount; i++) {
                int rc;
                if (mcast) {
                    DWORD ifa = g_ifAddr[i];
                    oSetsockopt(s, IPPROTO_IP_, IP_MULTICAST_IF_, (const char*)&ifa, 4);
                    rc = oSendto(s, buf, len, flags, to, tolen);
                } else {
                    SIN d = *to;                                // subnet-directed broadcast
                    d.addr = g_ifAddr[i] | ~g_ifMask[i];
                    rc = oSendto(s, buf, len, flags, &d, sizeof(d));
                }
                if (g_sendLog < 40) {
                    Log("Net: %s:%u via %s -> %d", mcast ? IpStr(to->addr, ip1) : "broadcast",
                        PortHost(to->port), IpStr(g_ifAddr[i], ip2), rc);
                    g_sendLog++;
                }
                if (rc >= 0) sent = rc;
            }
            if (mcast) {
                DWORD any = 0;
                oSetsockopt(s, IPPROTO_IP_, IP_MULTICAST_IF_, (const char*)&any, 4);
                if (g_netBcastToo) {
                    for (int i = 0; i < g_ifCount; i++) {
                        if (g_ifMask[i] == 0xFFFFFFFF) continue;     // /32 - no broadcast
                        SIN d = *to;
                        d.addr = g_ifAddr[i] | ~g_ifMask[i];
                        int rc = oSendto(s, buf, len, flags, &d, sizeof(d));
                        if (g_sendLog < 40) {
                            Log("Net: + broadcast %s:%u -> %d", IpStr(d.addr, ip1), PortHost(to->port), rc);
                            g_sendLog++;
                        }
                    }
                }
            }
            if (sent >= 0) return sent;
        }
    }
    return oSendto(s, buf, len, flags, to, tolen);
}

// Server/client: UDP sockets bound to one adapter address -> rebind to 0.0.0.0
static int WINAPI hkBind(SOCK s, const SIN* addr, int len)
{
    if (g_netBindAny && addr && len >= 8 && addr->family == 2) {
        int type = 0, tl = sizeof(type);
        if (pGetsockopt) pGetsockopt(s, 0xFFFF /*SOL_SOCKET*/, 0x1008 /*SO_TYPE*/, (char*)&type, &tl);
        BYTE* b = (BYTE*)&addr->addr;
        if (type == 2 /*SOCK_DGRAM*/ && addr->addr != 0 && b[0] != 127) {
            char ip[16];
            unsigned port = PortHost(addr->port);
            SIN a = *addr; a.addr = 0;
            int rc = oBind(s, &a, len);
            Log("Net: bind UDP %s:%u -> 0.0.0.0:%u (%d)", IpStr(addr->addr, ip), port, port, rc);
            if (rc == 0) return 0;
            Log("Net: bind to 0.0.0.0 failed - using the original address");
        }
    }
    return oBind(s, addr, len);
}

// Patch the IAT of 'mod' for ws2_32 imports (by ordinal or by name)
static int PatchWs2Iat(HMODULE mod)
{
    BYTE* base = (BYTE*)mod;
    BYTE* nt   = base + *(DWORD*)(base + 0x3C);
    DWORD impRva = *(DWORD*)(nt + 0x80);                         // import directory (PE32)
    if (!impRva) return 0;
    int n = 0;
    for (BYTE* d = base + impRva; *(DWORD*)(d + 12); d += 20) {
        const char* dll = (const char*)(base + *(DWORD*)(d + 12));
        if (_stricmp(dll, "WS2_32.dll") && _stricmp(dll, "WSOCK32.dll")) continue;
        DWORD* ilt = (DWORD*)(base + (*(DWORD*)(d + 0) ? *(DWORD*)(d + 0) : *(DWORD*)(d + 16)));
        void** iat = (void**)(base + *(DWORD*)(d + 16));
        for (int i = 0; ilt[i]; i++) {
            int which = 0;                                       // 1 setsockopt, 2 sendto, 3 bind
            if (ilt[i] & 0x80000000) {
                DWORD ord = ilt[i] & 0xFFFF;
                if (ord == 21) which = 1; else if (ord == 20) which = 2; else if (ord == 2) which = 3;
            } else {
                const char* nm = (const char*)(base + ilt[i] + 2);
                if      (!strcmp(nm, "setsockopt")) which = 1;
                else if (!strcmp(nm, "sendto"))     which = 2;
                else if (!strcmp(nm, "bind"))       which = 3;
            }
            if (which == 1) { PatchPtr(iat, i, (void*)hkSetsockopt, (void**)&oSetsockopt); n++; }
            if (which == 2) { PatchPtr(iat, i, (void*)hkSendto,     (void**)&oSendto);     n++; }
            if (which == 3) { PatchPtr(iat, i, (void*)hkBind,       (void**)&oBind);       n++; }
        }
    }
    return n;
}

static void InstallNetHook()
{
    if (g_netPatched || (!g_netAllIf && !g_netBindAny)) return;
    HMODULE nk = GetModuleHandleA("nk2.dll");
    if (!nk) return;                                             // not loaded yet - retried later
    g_netPatched = 1;
    HMODULE ws = GetModuleHandleA("ws2_32.dll");
    if (ws) pGetsockopt = (getsockopt_t)GetProcAddress(ws, "getsockopt");
    int n = PatchWs2Iat(nk);
    RefreshInterfaces();
    char ip[16];
    Log("Net: nk2.dll imports hooked: %d, IPv4 adapters: %d", n, g_ifCount);
    for (int i = 0; i < g_ifCount; i++) Log("Net:   adapter %d: %s", i, IpStr(g_ifAddr[i], ip));
}

// ============================================================================
// Direct3D 8 device hooks
// ============================================================================
static HRESULT WINAPI hkReset(void* dev, PP8* pp)
{
    HRESULT hr = oReset(dev, pp);
    if (SUCCEEDED(hr)) UpdateBackBuffer(pp, g_hwnd);
    else Log("D3D: Reset failed 0x%08X", (unsigned)hr);
    return hr;
}

static HRESULT WINAPI hkCreateDevice(void* self, UINT adapter, DWORD type, HWND focus,
                                     DWORD flags, PP8* pp, void** ppDev)
{
    if (pp)
        Log("D3D: CreateDevice %ux%u windowed=%d hz=%u flags=0x%lX",
            pp->BackBufferWidth, pp->BackBufferHeight, pp->Windowed,
            pp->FullScreen_RefreshRateInHz, flags);
    HRESULT hr = oCreateDevice(self, adapter, type, focus, flags, pp, ppDev);
    if (FAILED(hr) || !ppDev || !*ppDev) {
        Log("D3D: CreateDevice failed 0x%08X", (unsigned)hr);
        return hr;
    }
    UpdateBackBuffer(pp, focus);

    void** vtbl = *(void***)(*ppDev);
    PatchPtr(vtbl, IDX_DEV8_RESET,        (void*)hkReset,        (void**)&oReset);
    PatchPtr(vtbl, IDX_DEV8_SETTRANSFORM, (void*)hkSetTransform, (void**)&oSetTransform);
    PatchPtr(vtbl, IDX_DEV8_SETVIEWPORT,  (void*)hkSetViewport,  (void**)&oSetViewport);
    Log("D3D: device hooked (widescreen=%d)", g_widescreen);

    InstallSoundHook(g_hwnd ? g_hwnd : focus);
    InstallNetHook();
    return hr;
}

// ============================================================================
// d3d8.dll proxy
// ============================================================================
static void LoadReal()
{
    if (g_real) return;
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\d3d8.dll");
    g_real = LoadLibraryA(path);
    if (!g_real) { Log("D3D: cannot load %s", path); return; }
    g_realCreate    = (Direct3DCreate8_t)GetProcAddress(g_real, "Direct3DCreate8");
    g_pDebugSetMute = GetProcAddress(g_real, "DebugSetMute");
    g_pValidatePS   = GetProcAddress(g_real, "ValidatePixelShader");
    g_pValidateVS   = GetProcAddress(g_real, "ValidateVertexShader");
    Log("D3D: loaded %s", path);
}

extern "C" void* WINAPI Direct3DCreate8(UINT sdk)
{
    LoadReal();
    if (!g_realCreate) { Log("D3D: Direct3DCreate8 not found"); return NULL; }
    void* d3d = g_realCreate(sdk);
    Log("D3D: Direct3DCreate8(%u) -> %p", sdk, d3d);
    if (d3d) {
        void** vtbl = *(void***)d3d;
        PatchPtr(vtbl, IDX_D3D8_CREATEDEVICE, (void*)hkCreateDevice, (void**)&oCreateDevice);
    }
    return d3d;
}
#pragma comment(linker, "/EXPORT:Direct3DCreate8=_Direct3DCreate8@4")

extern "C" __declspec(naked) void Stub_DebugSetMute()         { __asm jmp g_pDebugSetMute }
extern "C" __declspec(naked) void Stub_ValidatePixelShader()  { __asm jmp g_pValidatePS }
extern "C" __declspec(naked) void Stub_ValidateVertexShader() { __asm jmp g_pValidateVS }
#pragma comment(linker, "/EXPORT:DebugSetMute=_Stub_DebugSetMute")
#pragma comment(linker, "/EXPORT:ValidatePixelShader=_Stub_ValidatePixelShader")
#pragma comment(linker, "/EXPORT:ValidateVertexShader=_Stub_ValidateVertexShader")

// ============================================================================
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);

        char base[MAX_PATH];
        GetModuleFileNameA(h, base, MAX_PATH);
        char* slash = strrchr(base, '\\');
        if (slash) slash[1] = 0;
        sprintf_s(g_iniPath, "%sSUD_Patch.ini", base);
        sprintf_s(g_logPath, "%sSUD_Patch.log", base);

        g_logOn       = GetPrivateProfileIntA("General", "Log",           1, g_iniPath);
        g_widescreen  = GetPrivateProfileIntA("Display", "Widescreen",    1, g_iniPath);
        g_dispW       = GetPrivateProfileIntA("Display", "Width",         0, g_iniPath);
        g_dispH       = GetPrivateProfileIntA("Display", "Height",        0, g_iniPath);
        g_fixRegistry = GetPrivateProfileIntA("Display", "FixRegistry",   1, g_iniPath);
        g_fixVolume   = GetPrivateProfileIntA("Sound",   "FixVolume",     1, g_iniPath);
        g_netAllIf    = GetPrivateProfileIntA("Network", "AllInterfaces", 1, g_iniPath);
        g_netBcastToo = GetPrivateProfileIntA("Network", "BroadcastToo",  1, g_iniPath);
        g_netBindAny  = GetPrivateProfileIntA("Network", "BindAnyUDP",    1, g_iniPath);

        if (g_logOn) { FILE* f = fopen(g_logPath, "w"); if (f) fclose(f); }

        // pin the module: hooked vtables/IAT point into our code
        HMODULE pinned;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPCSTR)&DllMain, &pinned);

        Log("==== %s v%s (PID %lu) ====", PATCH_NAME, PATCH_VERSION, GetCurrentProcessId());

        if (g_fixRegistry) CheckVirtualStore("start");
        ApplyResolution();
        LoadReal();
        InstallNetHook();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_fixRegistry) CheckVirtualStore("exit");
        Log("Exit (PID %lu)", GetCurrentProcessId());
    }
    return TRUE;
}
