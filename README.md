# Android Root Toolkit — C++ port

Native C++17, no Qt/Python needed. Official unlock paths only.
Knox 0x1 trip is irreversible — no bypass included by design.

## Build (Windows + MSYS2 g++)
```
cd android-root-toolkit-cpp
build.bat
```
Outputs `rootkit_cli.exe` + `rootkit_gui.exe` (Win32 tabs GUI).

Manual:
```
g++ -std=c++17 -O2 -o rootkit_cli.exe rootkit.cpp main_cli.cpp
g++ -std=c++17 -O2 -municode -o rootkit_gui.exe rootkit.cpp gui_win32.cpp -lcomctl32 -lgdi32 -lcomdlg32
```

## Use
```
rootkit_cli.exe --help
rootkit_cli.exe diagnose
rootkit_gui.exe
```
Requires Platform-Tools adb/fastboot in PATH.
