@echo off
setlocal
if not defined VSCMD_VER (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
)
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /Iinclude main.cpp src\*.cpp /Fo:build\ /Fe:filter.exe
