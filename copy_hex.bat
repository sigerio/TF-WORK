@echo off
setlocal

rem Use the script directory as the project root, regardless of the working directory.
for %%P in (xj_jc wether sun_control) do (
    if not exist "%~dp0%%P\MDK\Objects\%%P.hex" (
        echo [ERROR] Missing: "%~dp0%%P\MDK\Objects\%%P.hex"
        echo Build %%P in Keil first. No files copied.
        exit /b 1
    )
)

for %%P in (xj_jc wether sun_control) do (
    copy /b /y "%~dp0%%P\MDK\Objects\%%P.hex" "%~dp0%%P.hex" >nul
    if errorlevel 1 (
        echo [ERROR] Failed to copy %%P.hex. Earlier copies may have completed.
        exit /b 1
    )
    echo [OK] %%P.hex
)

echo HEX files copied to "%~dp0"
exit /b 0
