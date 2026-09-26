#include "rootkit.hpp"
#include <cstdio>
#include <cctype>
#include <sstream>
#include <regex>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace rk {

std::string trim(std::string s) {
    size_t a = 0; while (a < s.size() && isspace((unsigned char)s[a])) a++;
    size_t b = s.size(); while (b > a && isspace((unsigned char)s[b-1])) b--;
    return s.substr(a, b - a);
}
static std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
bool contains_ci(const std::string& h, const std::string& n) {
    return lower(h).find(lower(n)) != std::string::npos;
}

CmdResult run(const std::string& cmd, int /*timeoutSec*/) {
    CmdResult r;
    std::string full = cmd + " 2>&1";
    FILE* p = POPEN(full.c_str(), "r");
    if (!p) { r.code = 1; r.out = "popen failed"; return r; }
    char buf[4096]; std::string o;
    while (fgets(buf, sizeof buf, p)) o += buf;
    int rc = PCLOSE(p);
    r.code = rc; r.out = trim(o);
    return r;
}

bool have(const std::string& tool) {
#ifdef _WIN32
    CmdResult r = run("where " + tool);
    return r.code == 0 && !r.out.empty();
#else
    CmdResult r = run("command -v " + tool);
    return r.code == 0 && !r.out.empty();
#endif
}

std::vector<AdbDev> adb_devices() {
    std::vector<AdbDev> v;
    CmdResult r = run("adb devices -l");
    std::istringstream in(r.out);
    std::string line; bool first = true;
    for (; std::getline(in, line);) {
        line = trim(line);
        if (first) { first = false; continue; } // "List of devices..."
        if (line.empty() || line[0] == '*') continue;
        std::istringstream ls(line);
        std::string id, st;
        if (ls >> id >> st) v.push_back({id, st, line});
    }
    return v;
}

std::vector<std::string> fastboot_devices() {
    std::vector<std::string> v;
    CmdResult r = run("fastboot devices -l");
    std::istringstream in(r.out);
    std::string line;
    while (std::getline(in, line)) { line = trim(line); if (!line.empty()) v.push_back(line); }
    return v;
}

std::map<std::string,std::string> getprops() {
    std::map<std::string,std::string> m;
    CmdResult r = run("adb shell getprop");
    std::regex re(R"(\[(.+?)\]: \[(.*)\])");
    std::istringstream in(r.out);
    std::string line;
    while (std::getline(in, line)) {
        std::smatch sm;
        if (std::regex_search(line, sm, re) && sm.size() == 3)
            m[sm[1].str()] = sm[2].str();
    }
    return m;
}

std::string get_setting(const std::string& ns, const std::string& key) {
    return run("adb shell settings get " + ns + " " + key).out;
}

static std::string get(const std::map<std::string,std::string>& p, const std::string& k) {
    auto it = p.find(k); return it == p.end() ? "" : it->second;
}

Diag diagnose(const std::map<std::string,std::string>& props) {
    Diag d;
    d.mfr = get(props, "ro.product.manufacturer");
    auto brand = get(props, "ro.product.brand");
    d.model = get(props, "ro.product.model");
    d.android = get(props, "ro.build.version.release");
    d.isSamsung = contains_ci(d.mfr + " " + brand, "samsung");
    std::string wb = get(props, "ro.boot.warranty_bit");
    if (wb.empty()) wb = run("adb shell getprop ro.boot.warranty_bit").out;
    wb = trim(wb);
    if (d.isSamsung) {
        if (wb == "1") d.knoxMsg = "Knox 0x1 TRIPPED (permanent, cannot reset)";
        else if (wb == "0") d.knoxMsg = "Knox 0x0 intact — unlock WILL trip it permanently";
        else d.knoxMsg = "Knox state unknown — assume unlock trips it";
    }
    std::string lk = get(props, "ro.boot.flash.locked");
    d.lockMsg = (lk == "1") ? "bootloader locked" : (lk == "0") ? "bootloader unlocked" : "lock state unknown";
    std::string oem = trim(get_setting("global", "oem_unlock_allowed"));
    d.oemMsg = (oem == "1") ? "OEM toggle allowed" : "OEM toggle OFF/missing — use repair helper";
    return d;
}

std::string unlock_guide(const std::map<std::string,std::string>& props) {
    std::string b = lower(get(props, "ro.product.manufacturer") + " " + get(props, "ro.product.brand"));
    if (b.find("samsung") != std::string::npos)
        return "Samsung:\n 1. Dev options > OEM unlocking + USB debugging\n 2. Power off, VolUp+VolDown + USB -> Download Mode\n 3. Long-press VolUp to unlock (wipes, trips Knox)\n 4. Flash TWRP .tar via Odin AP (Auto-Reboot OFF)\n 5. Button-combo to recovery, Magisk / patched AP\n";
    if (b.find("xiaomi") != std::string::npos || b.find("redmi") != std::string::npos || b.find("poco") != std::string::npos)
        return "Xiaomi/Redmi/POCO: bind Mi account (Mi Unlock status), wait 7 days, Mi Unlock Tool, then 'fastboot flash recovery twrp.img'\n";
    return "Generic fastboot:\n adb reboot bootloader\n fastboot flashing unlock  (confirm on device, DATA WIPES)\n fastboot flash recovery twrp.img\n";
}

std::string magisk_guide() {
    return "Magisk (systemless):\n 1. Backup boot in TWRP; get stock boot.img for exact build\n 2. Magisk app (github.com/topjohnwu/Magisk) > patch boot.img / AP.tar\n 3. fastboot flash boot magisk-patched.img (Samsung: patched AP via Odin)\n 4. Reboot, verify 'adb shell su -c id'\n Knox features + banking apps may break permanently.\n";
}

std::string oem_repair_text() {
    return "Missing OEM toggle (eligible devices only, NOT a carrier/KG bypass):\n"
           " a) Remove SIM, WiFi on, Google+Samsung login\n b) Date: manual, 10-14 days back; auto-update OFF\n"
           " c) Software update > Check, reboot, recheck Dev options\n d) Date back to auto, reboot; needs 7-day uptime / KG Checking OK\n"
           " Carrier (U/U1/W), KG Locked, RMM Prenormal, MDM owner => cannot be scripted away.\n";
}

} // namespace rk
