@echo on
setlocal enabledelayedexpansion

set arch=%1
set GENERATOR=Ninja
for /F %%p in ('powershell -Command "$((Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors)"') do set CORES=%%p

if exist "update.bat" (
    call update.bat
)

set BUILD_DIR=build\%arch%-windows
set OUT_DIR=..\..\out
mkdir "%BUILD_DIR%"
mkdir "%OUT_DIR%"
copy /y "rel-triple-%arch%.cmake" "%VCPKG_ROOT%\triplets\community\%arch%-windows-rel.cmake"

echo Building %arch%-windows in %BUILD_DIR%

cmake -S "." -B "%BUILD_DIR%" -G "%GENERATOR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=%arch%-windows-static-release ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DAG_OUT_DIR=%OUT_DIR% ^
    -DAG_TRIPLE=%arch%-windows ^
    -DVCPKG_INSTALL_OPTIONS="--allow-unsupported"

cmake --build "%BUILD_DIR%" --parallel %CORES%

endlocal
