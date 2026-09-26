@echo off
REM Build C++ toolkit with MSYS2 g++
g++ -std=c++17 -O2 -o rootkit_cli.exe rootkit.cpp main_cli.cpp -lstdc++fs
if errorlevel 1 exit /b 1
g++ -std=c++17 -O2 -municode -o rootkit_gui.exe rootkit.cpp gui_win32.cpp -lcomctl32 -lgdi32 -lcomdlg32
if errorlevel 1 exit /b 1
echo Built: rootkit_cli.exe + rootkit_gui.exe
