// Advanced Win32 GUI (no Qt/deps): tabs + threaded adb + log pane.
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <functional>
#include <vector>
#include "rootkit.hpp"

#pragma comment(lib, "comctl32.lib")

static HINSTANCE g_hInst;
static HWND g_hTab, g_hLog, g_hPages[5];
static HWND g_hDepLbl, g_hDevList, g_hDiagList, g_hImgEdit, g_hPartCombo;
static std::map<std::string,std::string> g_props;
#define WM_LOG (WM_APP + 1)

static std::wstring s2w(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
static void Log(const std::string& s) {
    std::wstring* p = new std::wstring(s2w(s + "\n"));
    PostMessageW(GetParent(g_hLog), WM_LOG, 0, (LPARAM)p);
}
static void LogW(const std::wstring& s) {
    std::wstring* p = new std::wstring(s + L"\n");
    PostMessageW(GetParent(g_hLog), WM_LOG, 0, (LPARAM)p);
}
static void AppendLogText(const std::wstring& t) {
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, 0, (LPARAM)t.c_str());
}

static void Async(std::function<void()> fn) { std::thread([fn]{ fn(); }).detach(); }

static void DoDeps() {
    Async([]{
        std::string s = std::string("adb: ") + (rk::have("adb") ? "OK" : "MISSING") +
            " | fastboot: " + (rk::have("fastboot") ? "OK" : "MISSING") +
            " | heimdall: " + (rk::have("heimdall") ? "OK" : "missing");
        SetWindowTextW(g_hDepLbl, s2w(s).c_str());
        Log(s);
        if (!rk::have("adb")) Log("Install: winget install Google.PlatformTools");
    });
}
static void DoRefresh() {
    Async([]{
        auto devs = rk::adb_devices();
        auto fdevs = rk::fastboot_devices();
        SendMessageW(g_hDevList, LB_RESETCONTENT, 0, 0);
        for (auto& d : devs) SendMessageW(g_hDevList, LB_ADDSTRING, 0, (LPARAM)s2w("adb: " + d.line).c_str());
        for (auto& d : fdevs) SendMessageW(g_hDevList, LB_ADDSTRING, 0, (LPARAM)s2w("fastboot: " + d).c_str());
        if (!devs.empty()) {
            g_props = rk::getprops();
            rk::Diag dg = rk::diagnose(g_props);
            Log(dg.mfr + " " + dg.model + " Android " + dg.android + " | " + dg.lockMsg + " | " + dg.oemMsg);
            if (dg.isSamsung) Log("Samsung: " + dg.knoxMsg);
        } else Log("No adb device. Enable USB debugging + authorize.");
    });
}
static void DoDiagnose() {
    Async([]{
        auto p = rk::getprops();
        if (p.empty()) { Log("No adb device."); return; }
        g_props = p;
        SendMessageW(g_hDiagList, LB_RESETCONTENT, 0, 0);
        for (auto& kv : p) {
            if (kv.first.rfind("ro.", 0) == 0 || kv.first.find("oem") != std::string::npos ||
                kv.first.find("warranty") != std::string::npos || kv.first.rfind("sys.", 0) == 0)
                SendMessageW(g_hDiagList, LB_ADDSTRING, 0, (LPARAM)s2w(kv.first + " = " + kv.second).c_str());
        }
        rk::Diag d = rk::diagnose(p);
        Log(d.lockMsg + " | " + d.oemMsg + (d.isSamsung ? " | " + d.knoxMsg : ""));
        if (d.isSamsung) Log("NOTE: no software bypass keeps Knox 0x0 after custom flash.");
    });
}
static void DoVerifyRoot() {
    Async([]{
        auto r = rk::run("adb shell su -c id");
        if (r.code == 0 && r.out.find("uid=0") != std::string::npos) Log("ROOT OK: " + r.out);
        else Log("No root: " + (r.out.empty() ? "su not found" : r.out));
    });
}
static void DoFlash() {
    wchar_t img[MAX_PATH] = {0};
    GetWindowTextW(g_hImgEdit, img, MAX_PATH);
    wchar_t part[64] = {0};
    GetWindowTextW(g_hPartCombo, part, 64);
    std::wstring wimg(img), wpart(part[0] ? part : L"recovery");
    Async([wimg, wpart]{
        if (wimg.empty()) { Log("Pick a recovery image first."); return; }
        char a[MAX_PATH * 2] = {0}, b[64] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wimg.c_str(), -1, a, sizeof a, 0, 0);
        WideCharToMultiByte(CP_UTF8, 0, wpart.c_str(), -1, b, sizeof b, 0, 0);
        std::string brand = rk::getprops()["ro.product.manufacturer"];
        if (rk::contains_ci(brand, "samsung")) {
            Log("Samsung: flash .tar via Odin AP (Auto-Reboot OFF), then button-combo to recovery.");
            if (rk::have("heimdall")) Log(rk::run(std::string("heimdall flash --RECOVERY \"") + a + "\"").out);
            return;
        }
        Log(rk::run(std::string("fastboot flash ") + b + " \"" + a + "\"").out);
    });
}

static HWND MkBtn(HWND p, const wchar_t* t, int x, int y, int w, int h, HMENU id) {
    return CreateWindowW(L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h, p, id, g_hInst, 0);
}
static HWND MkLabel(HWND p, const wchar_t* t, int x, int y, int w, int h) {
    return CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y, w, h, p, 0, g_hInst, 0);
}
static HWND MkList(HWND p, int x, int y, int w, int h) {
    return CreateWindowW(L"LISTBOX", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        x, y, w, h, p, 0, g_hInst, 0);
}

static void BuildPages(HWND parent) {
    for (int i = 0; i < 5; i++) {
        g_hPages[i] = CreateWindowW(L"STATIC", L"", WS_CHILD | (i == 0 ? WS_VISIBLE : 0),
            10, 40, 940, 380, parent, 0, g_hInst, 0);
    }
    // p0 dashboard
    MkLabel(g_hPages[0], L"Deps + devices. Official unlock only — Knox trip irreversible.", 10, 5, 900, 20);
    g_hDepLbl = MkLabel(g_hPages[0], L"—", 10, 28, 900, 20);
    g_hDevList = MkList(g_hPages[0], 10, 52, 900, 150);
    MkBtn(g_hPages[0], L"Refresh", 10, 215, 110, 30, (HMENU)101);
    MkBtn(g_hPages[0], L"adb reboot", 130, 215, 110, 30, (HMENU)102);
    MkBtn(g_hPages[0], L"bootloader", 250, 215, 110, 30, (HMENU)103);
    MkBtn(g_hPages[0], L"recovery", 370, 215, 110, 30, (HMENU)104);
    // p1 diagnose
    MkBtn(g_hPages[1], L"Full diagnose", 10, 5, 140, 30, (HMENU)201);
    MkBtn(g_hPages[1], L"Verify root", 160, 5, 140, 30, (HMENU)202);
    g_hDiagList = MkList(g_hPages[1], 10, 45, 900, 300);
    // p2 samsung
    MkLabel(g_hPages[2], L"KNOX 0x1 trips on first custom flash and is PERMANENT. No bypass. OEM helper only restores toggle when eligible.", 10, 5, 900, 32);
    MkBtn(g_hPages[2], L"Check MDM/lock", 10, 45, 150, 30, (HMENU)301);
    MkBtn(g_hPages[2], L"Disable auto-time", 170, 45, 150, 30, (HMENU)302);
    MkBtn(g_hPages[2], L"Reboot", 330, 45, 110, 30, (HMENU)303);
    HWND g = CreateWindowW(L"EDIT", L"Samsung: Download Mode (VolUp+VolDown+USB), long-press VolUp to unlock (wipes, trips Knox). Flash .tar via Odin AP.\r\nOEM fix: SIM out, WiFi on, logins, date 10-14d back, update check, reboot, date auto, 7-day uptime.",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY, 10, 85, 900, 260, g_hPages[2], 0, g_hInst, 0);
    (void)g;
    // p3 flash
    MkLabel(g_hPages[3], L"Recovery image for EXACT model (TWRP: twrp.me/Devices). Samsung needs .tar for Odin.", 10, 5, 900, 20);
    g_hImgEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 30, 700, 26, g_hPages[3], 0, g_hInst, 0);
    MkBtn(g_hPages[3], L"Browse…", 720, 28, 100, 30, (HMENU)401);
    g_hPartCombo = CreateWindowW(L"COMBOBOX", 0, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 830, 28, 90, 120, g_hPages[3], 0, g_hInst, 0);
    for (auto s : {L"recovery", L"boot", L"init_boot", L"vendor_boot"}) SendMessageW(g_hPartCombo, CB_ADDSTRING, 0, (LPARAM)s);
    SendMessageW(g_hPartCombo, CB_SETCURSEL, 0, 0);
    MkBtn(g_hPages[3], L"Flash recovery", 10, 70, 160, 32, (HMENU)402);
    // p4 root
    HWND r = CreateWindowW(L"EDIT", L"Magisk: backup boot in TWRP, patch stock boot.img / AP.tar in Magisk app, fastboot flash boot magisk.img (Samsung: patched AP via Odin), verify su.",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY, 10, 5, 900, 200, g_hPages[4], 0, g_hInst, 0);
    (void)r;
    MkBtn(g_hPages[4], L"Verify root", 10, 215, 140, 32, (HMENU)501);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_LOG) { std::wstring* p = (std::wstring*)l; AppendLogText(*p); delete p; return 0; }
    if (m == WM_NOTIFY) {
        LPNMHDR nh = (LPNMHDR)l;
        if (nh->idFrom == 1 && nh->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_hTab);
            for (int i = 0; i < 5; i++) ShowWindow(g_hPages[i], i == sel ? SW_SHOW : SW_HIDE);
        }
        return 0;
    }
    if (m == WM_COMMAND) {
        int id = LOWORD(w);
        if (id == 101) { DoDeps(); DoRefresh(); }
        else if (id == 102) Async([]{ Log(rk::run("adb reboot").out); });
        else if (id == 103) Async([]{ Log(rk::run("adb reboot bootloader").out); });
        else if (id == 104) Async([]{ Log(rk::run("adb reboot recovery").out); });
        else if (id == 201) DoDiagnose();
        else if (id == 202 || id == 501) DoVerifyRoot();
        else if (id == 301) Async([]{ Log(rk::run("adb shell dumpsys device_policy_manager | grep -i -m5 admin").out); });
        else if (id == 302) Async([]{ rk::run("adb shell settings put global auto_time 0"); Log("auto_time off. Set date back manually."); });
        else if (id == 303) Async([]{ Log(rk::run("adb reboot").out); });
        else if (id == 401) {
            wchar_t f[MAX_PATH] = {0};
            OPENFILENAMEW of = {sizeof of, h, 0, L"Recovery\0*.img;*.tar;*.tar.md5\0All\0*.*\0", 0, 0, 1, f, MAX_PATH};
            if (GetOpenFileNameW(&of)) SetWindowTextW(g_hImgEdit, f);
        }
        else if (id == 402) {
            if (MessageBoxW(h, L"Flash this recovery? Image must match EXACT model.", L"Confirm", MB_YESNO) == IDYES) DoFlash();
        }
        return 0;
    }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int show) {
    g_hInst = hi;
    INITCOMMONCONTROLSEX ic = {sizeof ic, ICC_TAB_CLASSES};
    InitCommonControlsEx(&ic);
    WNDCLASSW wc = {0, WndProc, 0, 0, hi, 0, (HCURSOR)LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), 0, L"RkGui"};
    RegisterClassW(&wc);
    HWND win = CreateWindowW(L"RkGui", L"Android Root & Recovery Toolkit (C++ Win32)",
        WS_OVERLAPPEDWINDOW, 100, 100, 980, 680, 0, 0, hi, 0);
    g_hTab = CreateWindowW(WC_TABCONTROLW, 0, WS_CHILD | WS_VISIBLE | TCS_TABS, 10, 10, 940, 30, win, (HMENU)1, hi, 0);
    for (auto t : {L"Dashboard", L"Diagnose", L"Samsung / OEM", L"Flash", L"Root"}) {
        TCITEMW ti = {TCIF_TEXT, 0, 0, (LPWSTR)t};
        TabCtrl_InsertItem(g_hTab, TabCtrl_GetItemCount(g_hTab), &ti);
    }
    BuildPages(win);
    MkLabel(win, L"Log:", 10, 425, 100, 18);
    g_hLog = CreateWindowW(L"EDIT", L"Ready. Click Refresh on Dashboard.\r\n",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 445, 940, 175, win, 0, hi, 0);
    ShowWindow(win, show);
    MSG msg; while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
