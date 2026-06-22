@echo off
setlocal

set BUNDLE_NAME=OfxBase.ofx.bundle
set INSTALL_DIR=C:\Program Files\Common Files\OFX\Plugins

:: Relaunch as administrator if not already elevated
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:: Find the bundle next to this script
set BUNDLE_PATH=%~dp0%BUNDLE_NAME%
if not exist "%BUNDLE_PATH%\" (
    echo Error: %BUNDLE_NAME% not found next to this installer.
    echo Expected location: %BUNDLE_PATH%
    pause
    exit /b 1
)

echo Installing %BUNDLE_NAME% to %INSTALL_DIR% ...

if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
)

if exist "%INSTALL_DIR%\%BUNDLE_NAME%" (
    rmdir /s /q "%INSTALL_DIR%\%BUNDLE_NAME%"
)

xcopy /e /i /q "%BUNDLE_PATH%" "%INSTALL_DIR%\%BUNDLE_NAME%\"
if %errorlevel% neq 0 (
    echo Error: Failed to copy files.
    pause
    exit /b 1
)

echo.
echo Installation complete.
echo   Location: %INSTALL_DIR%\%BUNDLE_NAME%
echo.
echo Please launch (or restart) DaVinci Resolve and verify the plugin is loaded.
echo.
pause
