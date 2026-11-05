// Line Remove Ads
//
// A tray-only Win32 utility that removes the advertisement banners from LINE for Windows.
//
// How it works: LINE routes every ad through its AdvertisementManager class. A background thread
// waits for LINE.exe, locates two of that class's functions in the running process by byte
// signature, and overwrites their first bytes with "xor eax, eax; ret" so they return immediately.
// Nothing is written to LINE's files, and the change disappears when LINE exits; it is applied
// again the next time LINE starts.

#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// ---------------------------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------------------------
static const wchar_t* const APP_NAME  = L"Line Remove Ads";
static const wchar_t* const RUN_KEY   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* const APP_KEY   = L"Software\\LineRemoveAds";
static const wchar_t* const ABOUT_URL = L"https://github.com/RouZongWorks/line-remove-ads";
static const wchar_t* const LINE_EXE  = L"LINE.exe";

static const DWORD POLL_INTERVAL_MS = 500;       // how often the worker looks for LINE.exe
static const ULONGLONG GIVE_UP_MS   = 120000;    // stop scanning one LINE process after this long

enum : UINT { WM_TRAY = WM_APP + 1, WM_STATE = WM_APP + 2 };
enum : UINT { ID_AUTOSTART = 1001, ID_ABOUT = 1002, ID_EXIT = 1003 };
enum State { StateWaiting = 0, StatePatched = 1, StateFailed = 2 };

static HWND   g_hwnd;
static HANDLE g_stop;                   // signalled when the worker thread must exit
static UINT   g_taskbarCreated;
static HICON  g_icon;
static NOTIFYICONDATAW g_nid;

// ---------------------------------------------------------------------------------------------
// Start-with-Windows (HKCU\...\Run). Enabled by default the first time the program ever runs.
// ---------------------------------------------------------------------------------------------
static std::wstring ExePath() {
    wchar_t path[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, path, (DWORD)(sizeof(path) / sizeof(path[0])));
    return std::wstring(path, n);
}

static std::wstring RunCommand() {
    return L"\"" + ExePath() + L"\"";
}

static bool ReadRunValue(std::wstring* value) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    wchar_t buf[MAX_PATH * 2 + 4];
    DWORD type = 0, size = sizeof(buf) - sizeof(wchar_t);
    bool ok = RegQueryValueExW(key, APP_NAME, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ;
    RegCloseKey(key);
    if (ok && value) {
        buf[size / sizeof(wchar_t)] = 0;
        *value = buf;
    }
    return ok;
}

static bool AutostartEnabled() {
    return ReadRunValue(nullptr);
}

static void SetAutostart(bool enable) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    if (enable) {
        std::wstring cmd = RunCommand();
        RegSetValueExW(key, APP_NAME, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, APP_NAME);
    }
    RegCloseKey(key);
}

// First run: turn the option on. Later runs: keep the user's choice, but if the option is on and
// the program has been moved, point the registry entry at the new location.
static void InitSettings() {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, APP_KEY, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, L"Initialized", nullptr, &type, nullptr, &size) != ERROR_SUCCESS) {
        SetAutostart(true);
        DWORD one = 1;
        RegSetValueExW(key, L"Initialized", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
    } else {
        std::wstring current;
        if (ReadRunValue(&current) && _wcsicmp(current.c_str(), RunCommand().c_str()) != 0) SetAutostart(true);
    }
    RegCloseKey(key);
}

// ---------------------------------------------------------------------------------------------
// Patcher
// ---------------------------------------------------------------------------------------------
struct Target {
    const char* name;
    std::vector<BYTE> signature;        // first bytes of the function's prologue
};

static std::vector<BYTE> Hex(const char* text) {
    std::vector<BYTE> bytes;
    int high = -1;
    for (; *text; ++text) {
        int c = *text;
        int v = (c >= '0' && c <= '9') ? c - '0'
              : (c >= 'a' && c <= 'f') ? c - 'a' + 10
              : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) continue;            // spaces are ignored
        if (high < 0) { high = v; } else { bytes.push_back((BYTE)(high * 16 + v)); high = -1; }
    }
    return bytes;
}

static const std::vector<Target>& Targets() {
    static const std::vector<Target> targets = {
        { "AdvertisementManager::requestAdvertisements",
          Hex("44884424 18 89542410 4889 4c2408 55 4881ec60010000 488d6c2430 488b8540010000 4883c078 488985c8000000 48638548010000") },
        { "AdvertisementManager::onGetAdvertisements",
          Hex("44884c2420 4c89442418 89542410 4889 4c2408 55 4881ec30010000") },
    };
    return targets;
}

static const BYTE PATCH[3] = { 0x31, 0xC0, 0xC3 };      // xor eax, eax ; ret

// The first bytes of a target may already be the patch, so match the bytes after them and accept
// either the original head or the patched head. This lets the program recognise a LINE process it
// (or an earlier run) has already patched instead of treating it as a failure.
static void FindAll(const std::vector<BYTE>& data, size_t length, const std::vector<BYTE>& sig,
                    ULONG_PTR base, std::vector<ULONG_PTR>& hits) {
    const size_t head = sizeof(PATCH);
    if (sig.size() <= head || length < sig.size()) return;
    const BYTE* tail = sig.data() + head;
    const size_t tailLength = sig.size() - head;
    const BYTE* end = data.data() + length - tailLength + 1;
    for (const BYTE* p = data.data() + head; p < end; ++p) {
        p = (const BYTE*)memchr(p, tail[0], (size_t)(end - p));
        if (!p) break;
        if (memcmp(p, tail, tailLength) != 0) continue;
        const BYTE* start = p - head;
        if (memcmp(start, sig.data(), head) == 0 || memcmp(start, PATCH, head) == 0)
            hits.push_back(base + (ULONG_PTR)(start - data.data()));
    }
}

static bool IsExecutable(DWORD protect) {
    return (protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

enum PatchResult { NotReady, Done, Refused };

static PatchResult PatchProcess(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process) return NotReady;

    const std::vector<Target>& targets = Targets();
    std::vector<std::vector<ULONG_PTR>> hits(targets.size());
    std::vector<BYTE> buffer;

    // Scan the executable image regions of the target process.
    MEMORY_BASIC_INFORMATION mbi;
    ULONG_PTR address = 0;
    while (VirtualQueryEx(process, (LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        ULONG_PTR base = (ULONG_PTR)mbi.BaseAddress;
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE && IsExecutable(mbi.Protect) &&
            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && mbi.RegionSize < 0x20000000) {
            buffer.resize(mbi.RegionSize);
            SIZE_T got = 0;
            if (ReadProcessMemory(process, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &got) || got) {
                for (size_t i = 0; i < targets.size(); ++i) FindAll(buffer, got, targets[i].signature, base, hits[i]);
            }
        }
        address = base + mbi.RegionSize;
        if (address <= base) break;
    }

    // Every signature must match exactly once; anything else means "not ready" or "unknown build".
    PatchResult result = Done;
    for (size_t i = 0; i < targets.size(); ++i) {
        if (hits[i].empty()) { result = NotReady; break; }       // not unpacked yet, or LINE was updated
        if (hits[i].size() != 1) { result = Refused; break; }    // ambiguous: never guess
    }

    if (result == Done) {
        for (size_t i = 0; i < targets.size(); ++i) {
            LPVOID target = (LPVOID)hits[i][0];
            BYTE current[sizeof(PATCH)] = {};
            SIZE_T count = 0;
            if (ReadProcessMemory(process, target, current, sizeof(current), &count) && count == sizeof(current) &&
                memcmp(current, PATCH, sizeof(PATCH)) == 0)
                continue;                                         // already patched
            DWORD oldProtect = 0;
            if (!VirtualProtectEx(process, target, sizeof(PATCH), PAGE_EXECUTE_READWRITE, &oldProtect)) { result = Refused; break; }
            BOOL ok = WriteProcessMemory(process, target, PATCH, sizeof(PATCH), &count);
            VirtualProtectEx(process, target, sizeof(PATCH), oldProtect, &oldProtect);
            FlushInstructionCache(process, target, sizeof(PATCH));
            if (!ok || count != sizeof(PATCH)) { result = Refused; break; }
        }
    }
    CloseHandle(process);
    return result;
}

static std::vector<DWORD> FindLineProcesses() {
    std::vector<DWORD> pids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, LINE_EXE) == 0) pids.push_back(entry.th32ProcessID);
    }
    CloseHandle(snapshot);
    return pids;
}

static DWORD WINAPI WorkerThread(LPVOID) {
    struct Info { ULONGLONG firstSeen; bool done; bool gaveUp; };
    std::map<DWORD, Info> processes;
    int lastState = -1;

    for (;;) {
        std::vector<DWORD> pids = FindLineProcesses();
        for (auto it = processes.begin(); it != processes.end();) {
            if (std::find(pids.begin(), pids.end(), it->first) == pids.end()) it = processes.erase(it); else ++it;
        }

        ULONGLONG now = GetTickCount64();
        for (DWORD pid : pids) {
            Info& info = processes.emplace(pid, Info{ now, false, false }).first->second;
            if (info.done || info.gaveUp) continue;
            PatchResult r = PatchProcess(pid);
            if (r == Done) info.done = true;
            else if (r == Refused || now - info.firstSeen > GIVE_UP_MS) info.gaveUp = true;   // do not scan forever
        }

        int state = StateWaiting;
        for (const auto& kv : processes) {
            if (kv.second.done) { state = StatePatched; break; }
            if (kv.second.gaveUp) state = StateFailed;
        }
        if (state != lastState) {
            lastState = state;
            PostMessageW(g_hwnd, WM_STATE, (WPARAM)state, 0);
        }
        if (WaitForSingleObject(g_stop, POLL_INTERVAL_MS) == WAIT_OBJECT_0) break;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Tray icon, menu and window
// ---------------------------------------------------------------------------------------------
static void SetTooltip(const wchar_t* text) {
    wcsncpy_s(g_nid.szTip, text, _TRUNCATE);
}

static void AddTrayIcon() {
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_icon;
    SetTooltip(L"Line Remove Ads - waiting for LINE");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void ShowMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (AutostartEnabled() ? MF_CHECKED : MF_UNCHECKED), ID_AUTOSTART, L"Run at startup");
    AppendMenuW(menu, MF_STRING, ID_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);        // required so the menu closes when the user clicks elsewhere
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreated && message == g_taskbarCreated) {      // Explorer restarted: re-add the icon
        AddTrayIcon();
        return 0;
    }
    switch (message) {
    case WM_TRAY:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowMenu();
        return 0;
    case WM_STATE: {
        const wchar_t* tip =
            wParam == StatePatched ? L"Line Remove Ads - LINE ads removed" :
            wParam == StateFailed  ? L"Line Remove Ads - patch failed (LINE may have been updated)" :
                                     L"Line Remove Ads - waiting for LINE";
        SetTooltip(tip);
        g_nid.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_AUTOSTART: SetAutostart(!AutostartEnabled()); break;
        case ID_ABOUT:     ShellExecuteW(nullptr, L"open", ABOUT_URL, nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_EXIT:      DestroyWindow(hwnd); break;
        }
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\LineRemoveAds_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;       // already running

    InitSettings();

    g_icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"LineRemoveAdsTray";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, APP_NAME, WS_POPUP, 0, 0, 0, 0,
                             nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) return 1;

    AddTrayIcon();
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE worker = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    SetEvent(g_stop);
    WaitForSingleObject(worker, 5000);
    CloseHandle(worker);
    CloseHandle(g_stop);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
