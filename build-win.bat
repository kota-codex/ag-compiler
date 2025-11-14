@echo on
setlocal enabledelayedexpansion

set triplet=%1
set GENERATOR=Ninja
for /F %%p in ('powershell -Command "$((Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors)"') do set CORES=%%p

if exist "update.bat" (
    call update.bat
)

set BUILD_DIR=build\%triplet%
set OUT_DIR=..\..\out
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Building %triplet% in %BUILD_DIR%

cmake -S "." -B "%BUILD_DIR%" -G "%GENERATOR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=%triplet%-static ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DAG_OUT_DIR=%OUT_DIR% ^
    -DAG_TRIPLE=%triplet% ^
    -DVCPKG_BUILD_TYPE=release ^
    -DVCPKG_ALLOW_UNSUPPORTED=ON

cmake --build "%BUILD_DIR%" --parallel %CORES%

endlocal
