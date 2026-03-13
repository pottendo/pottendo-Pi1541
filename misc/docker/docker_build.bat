@echo off
setlocal

set IMAGE=pottendo-pi1541-builder
set OUTPUT=%~dp0..\Pi-Bootpart
set ARCH=%1
for /f %%a in ('powershell -command "[int](Get-Date -UFormat %%s)"') do set START=%%a

echo === Copying Pi-Bootpart base (ROMs, firmware, boot files) to %OUTPUT% ===
docker run --rm -v "%OUTPUT%:/output" --entrypoint sh %IMAGE% -c "cp -r /build/Pi-Bootpart/. /output/"
if errorlevel 1 (
    echo ERROR: failed to copy Pi-Bootpart base. Is the image built?
    exit /b 1
)

echo.
echo === Building Pi1541 %ARCH% ===
if "%ARCH%"=="" (
    docker run --rm -v "%OUTPUT%:/build/Pi-Bootpart" %IMAGE%
) else (
    docker run --rm -v "%OUTPUT%:/build/Pi-Bootpart" %IMAGE% -a %ARCH%
)
if errorlevel 1 (
    echo ERROR: build failed.
    exit /b 1
)

for /f %%a in ('powershell -command "[int](Get-Date -UFormat %%s)"') do set END=%%a
set /a ELAPSED=END-START
set /a MINS=ELAPSED/60
set /a SECS=ELAPSED%%60

echo.
echo === Done in %MINS%m %SECS%s. Files in %OUTPUT% ===
dir %OUTPUT%

endlocal
