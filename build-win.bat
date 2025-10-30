@echo on
setlocal enabledelayedexpansion

set GENERATOR=Ninja

set CONFIGS=Release Debug

if exist "update.bat" (
    call update.bat
)

set VS_VCVARSALL="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

for /f %%p in ('wmic cpu get NumberOfLogicalProcessors ^| findstr /r /v "^$"') do set CORES=%%p

REM --------------------------------
REM Compile platforms
REM --------------------------------
call :compilePlatform x64         x64-windows
call :compilePlatform amd64_arm64 arm64-windows

echo Build complete!
endlocal
exit /b


REM ==============================================
REM Function: compilePlatform vcvars_arg triplet
REM ==============================================
:compilePlatform
setlocal
set build_mode=%1
set triplet=%2

echo ================================
echo Building for %triplet% (%build_mode%)
echo ================================

call %VS_VCVARSALL% %build_mode%

set BUILD_DIR=build\%triplet%
if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"
set OUT_DIR=..\..\out
if not exist "!OUT_DIR!" mkdir "!OUT_DIR!"
for %%c in (%CONFIGS%) do (
    echo Building %triplet%, config %%c in !BUILD_DIR!

    cmake -S "." -B "!BUILD_DIR!" -G "%GENERATOR%" ^
        -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
        -DVCPKG_TARGET_TRIPLET=%triplet%-static ^
        -DCMAKE_BUILD_TYPE=%%c ^
        -DAG_OUT_DIR=!OUT_DIR! ^
        -DAG_TRIPLE=%triplet%

    cmake --build "!BUILD_DIR!" --parallel !CORES!
)

endlocal
exit /b
