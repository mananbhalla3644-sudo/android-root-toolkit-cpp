#include "rootkit.hpp"
#include <iostream>
#include <filesystem>

static void deps() {
    std::cout << "=== deps ===\n";
    for (auto t : {"adb", "fastboot"}) std::cout << (rk::have(t) ? "[OK] " : "[MISSING] ") << t << "\n";
    if (!rk::have("adb")) std::cout << "Install Platform-Tools: https://developer.android.com/tools/releases/platform-tools\n";
    if (rk::have("heimdall")) std::cout << "[OK optional] heimdall\n";
}

static void cmd_diagnose() {
    auto devs = rk::adb_devices();
    if (devs.empty()) { std::cout << "[!] No adb device. Enable USB debugging + authorize.\n"; return; }
    auto p = rk::getprops();
    std::cout << "model=" << p["ro.product.model"] << " device=" << p["ro.product.device"]
              << " mfr=" << p["ro.product.manufacturer"] << " android=" << p["ro.build.version.release"] << "\n";
    rk::Diag d = rk::diagnose(p);
    if (d.isSamsung) std::cout << "Samsung: " << d.knoxMsg << "\nNOTE: no bypass keeps Knox 0x0 after custom flash.\n";
    std::cout << d.lockMsg << "\n" << d.oemMsg << "\n";
}

static void menu() {
    std::cout << "\nAndroid Root Toolkit (C++)\n1 deps  2 list  3 diagnose  4 oem-repair  5 guide  6 flash  0 exit\n> ";
}

int main(int argc, char** argv) {
    std::string a = argc > 1 ? argv[1] : "";
    if (a == "--help" || a == "-h") {
        std::cout << "Usage: rootkit_cli [diagnose|repair-oem|guide|flash <img>]\n"
                     "Official unlock only. Knox trip irreversible. Use on own devices.\n";
        return 0;
    }
    if (a == "diagnose") { deps(); cmd_diagnose(); return 0; }
    if (a == "repair-oem") { std::cout << rk::oem_repair_text(); return 0; }
    if (a == "guide") {
        auto p = rk::adb_devices().empty() ? std::map<std::string,std::string>{} : rk::getprops();
        std::cout << rk::unlock_guide(p) << rk::magisk_guide(); return 0;
    }
    if (a == "flash" && argc > 2) {
        std::string img = argv[2];
        if (!std::filesystem::exists(img)) { std::cout << "[!] not found: " << img << "\nGet exact TWRP at https://twrp.me/Devices/\n"; return 1; }
        std::cout << "[*] Flash manually: fastboot flash recovery \"" << img
                  << "\" (Samsung .tar via Odin AP).\nAuto-flash refused without interactive confirm.\n";
        return 0;
    }
    std::string c;
    while (true) { menu(); if (!std::getline(std::cin, c)) break; c = rk::trim(c);
        if (c == "0" || c == "q") break;
        else if (c == "1") deps();
        else if (c == "2") {
            for (auto& d : rk::adb_devices()) std::cout << "adb: " << d.line << "\n";
            for (auto& d : rk::fastboot_devices()) std::cout << "fastboot: " << d << "\n";
        }
        else if (c == "3") cmd_diagnose();
        else if (c == "4") std::cout << rk::oem_repair_text();
        else if (c == "5") {
            auto p = rk::adb_devices().empty() ? std::map<std::string,std::string>{} : rk::getprops();
            std::cout << rk::unlock_guide(p) << rk::magisk_guide();
        }
        else if (c == "6") std::cout << "Run: rootkit_cli flash <recovery.img|recovery.tar>\n";
        else std::cout << "unknown\n";
    }
    return 0;
}
