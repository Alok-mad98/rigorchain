@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo.
echo --------------------------------
echo RIGOR Miner Dashboard
echo --------------------------------
echo Read-only visualization for your Windows SOLO mining account.
echo Path: wallet → rigor-miner → local rigord GBT → submitblock.
echo Open in this PC's browser: http://127.0.0.1:18083/
echo Explorer: http://127.0.0.1:18081/
echo This dashboard does not sign, spend, or read private keys.
echo.

set "DASHBOARD_DIR=%~dp0dashboard"
if not exist "%DASHBOARD_DIR%\server.py" set "DASHBOARD_DIR=%~dp0windows-miner-dashboard"
if not exist "%DASHBOARD_DIR%\server.py" set "DASHBOARD_DIR=%~dp0..\..\mining\windows-miner-dashboard"
if not exist "%DASHBOARD_DIR%\server.py" (
  echo Dashboard sources were not found next to this launcher.
  echo Start it from the official tree instead:
  echo   python3 mining/windows-miner-dashboard/server.py
  echo Then open http://127.0.0.1:18083/ in Microsoft Edge or Chrome.
  echo.
  start "" "http://127.0.0.1:18083/"
  pause
  exit /b 1
)

set "DASHBOARD_HOST=127.0.0.1"
set "DASHBOARD_PORT=18083"
set "MINER_PLATFORM=Windows"

where python >nul 2>&1
if errorlevel 1 (
  where python3 >nul 2>&1
  if errorlevel 1 (
    echo Python 3 is required to host the local dashboard.
    echo Install Python 3, or start server.py from WSL:
    echo   cd ~/rigor-official-tree/mining/windows-miner-dashboard
    echo   python3 server.py
    echo Then refresh http://127.0.0.1:18083/ on Windows.
    echo.
    start "" "http://127.0.0.1:18083/"
    pause
    exit /b 1
  )
  set "PY=python3"
) else (
  set "PY=python"
)

echo Starting dashboard on 127.0.0.1:18083 ...
start "" "http://127.0.0.1:18083/"
"%PY%" "%DASHBOARD_DIR%\server.py"
set "EC=%ERRORLEVEL%"
echo.
echo Dashboard exited with code %EC%.
pause
endlocal
