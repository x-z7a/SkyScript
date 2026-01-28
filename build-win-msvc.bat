@echo off
setlocal enabledelayedexpansion

REM Windows build script using MSVC
REM Requires Visual Studio Developer Command Prompt or vcvars64.bat

set OBJDIR=build\win
set SDK=SDK

REM Create output directories
if not exist %OBJDIR%\src mkdir %OBJDIR%\src

REM Generate version.h
for /f %%i in ('git log -1 --pretty^=format:"%%h"') do set COMMIT=%%i
pushd ..\xplib
for /f %%i in ('git log -1 --pretty^=format:"%%h"') do set XPLIB_COMMIT=%%i
popd
for /f "tokens=1 delims==" %%a in ('findstr /B "VERSION=" version.mak') do set %%a
echo //generated file! > version.h
echo #define VERSION "%VERSION% %COMMIT%,XPLIB:%XPLIB_COMMIT%" >> version.h
echo #define VERSION_SHORT "%VERSION%" >> version.h
type version.h

REM Compiler flags
REM /Zc:preprocessor - Enable conforming preprocessor (required for __VA_OPT__)
REM /FI - Force include MSVC compatibility header to handle GCC-specific syntax
set CXXFLAGS=/std:c++20 /O2 /EHsc /MD /W3 /Zc:preprocessor /FImsvc_compat.h
set DEFINES=/DXPLM200 /DXPLM210 /DXPLM300 /DXPLM301 /DWINDOWS /DWIN32 /DIBM=1
set INCLUDES=/I. /I..\xplib /I%SDK%\CHeaders\XPLM /IUltralight-SDK-1.4.0-Win64\include

REM Compile source files from src directory (including subdirectories)
echo Compiling source files...
for /r src %%f in (*.cpp) do (
    echo Compiling %%f...
    set "RELPATH=%%f"
    set "RELPATH=!RELPATH:%CD%\=!"
    set "OBJFILE=%OBJDIR%\!RELPATH:.cpp=.obj!"
    for %%d in ("!OBJFILE!") do if not exist "%%~dpd" mkdir "%%~dpd"
    cl.exe /c %CXXFLAGS% %DEFINES% %INCLUDES% /Fo"!OBJFILE!" "%%f"
    if errorlevel 1 exit /b 1
)

echo Compiling ..\xplib\log_msg.cpp...
cl.exe /c %CXXFLAGS% %DEFINES% %INCLUDES% /Fo%OBJDIR%\log_msg.obj ..\xplib\log_msg.cpp
if errorlevel 1 exit /b 1

REM Collect all object files
set OBJS=%OBJDIR%\log_msg.obj
for /r %OBJDIR%\src %%f in (*.obj) do set "OBJS=!OBJS! %%f"

REM Link
echo Linking...
set LIBPATH=/LIBPATH:%SDK%\Libraries\Win /LIBPATH:Ultralight-SDK-1.4.0-Win64\lib
set LIBS=XPLM_64.lib winhttp.lib opengl32.lib Ultralight.lib UltralightCore.lib WebCore.lib AppCore.lib

link.exe /DLL /OUT:build\win.xpl %OBJS% %LIBPATH% %LIBS%
if errorlevel 1 exit /b 1

echo Build successful: build\win.xpl
dir build\win.xpl
