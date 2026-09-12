@echo off
setlocal EnableDelayedExpansion

where cl >nul 2>nul
if errorlevel 1 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
      set "VS_DIR=%%i"
    )
  )
  if defined VS_DIR (
    if exist "!VS_DIR!\VC\Auxiliary\Build\vcvars64.bat" (
      echo [MouseRipple] Initializing Visual Studio environment...
      call "!VS_DIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
  )
)

where cl >nul 2>nul
if errorlevel 1 (
  echo [MouseRipple] Visual Studio C++ compiler was not found.
  echo Open "Developer Command Prompt for VS 2022" and run this file again.
  pause
  exit /b 1
)

taskkill /f /im MouseRipple.exe >nul 2>nul
cl /nologo /EHsc /O2 /std:c++17 /DUNICODE /D_UNICODE MouseRipple.cpp /link /SUBSYSTEM:WINDOWS gdiplus.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib advapi32.lib /OUT:MouseRipple.exe
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)

if exist MouseRipple.obj del MouseRipple.obj

echo.
echo Built successfully: MouseRipple.exe
pause
