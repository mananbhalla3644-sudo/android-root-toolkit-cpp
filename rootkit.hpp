#pragma once
// Core: adb/fastboot wrappers, official paths only. No Knox bypass.
#include <string>
#include <vector>
#include <map>

namespace rk {

struct CmdResult { int code = 1; std::string out; };

CmdResult run(const std::string& cmd, int timeoutSec = 15);
bool have(const std::string& tool);
std::string trim(std::string s);
bool contains_ci(const std::string& h, const std::string& n);

struct AdbDev { std::string id, state, line; };
std::vector<AdbDev> adb_devices();
std::vector<std::string> fastboot_devices();
std::map<std::string,std::string> getprops();
std::string get_setting(const std::string& ns, const std::string& key);

struct Diag {
    bool isSamsung = false;
    std::string knoxMsg, lockMsg, oemMsg, model, mfr, android;
};
Diag diagnose(const std::map<std::string,std::string>& props);

std::string unlock_guide(const std::map<std::string,std::string>& props);
std::string magisk_guide();
std::string oem_repair_text();

} // namespace rk
