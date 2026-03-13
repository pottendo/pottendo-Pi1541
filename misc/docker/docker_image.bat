@echo off
setlocal

set IMAGE=pottendo-pi1541-builder
for /f %%a in ('powershell -command "[int](Get-Date -UFormat %%s)"') do set START=%%a

echo === Building Docker image: %IMAGE% ===
docker build -t %IMAGE% .
if errorlevel 1 (
    echo ERROR: docker build failed.
    exit /b 1
)

for /f %%a in ('powershell -command "[int](Get-Date -UFormat %%s)"') do set END=%%a
set /a ELAPSED=END-START
set /a MINS=ELAPSED/60
set /a SECS=ELAPSED%%60

echo.
echo === Image built successfully in %MINS%m %SECS%s. Run docker_build.bat to build Pi1541. ===

endlocal
