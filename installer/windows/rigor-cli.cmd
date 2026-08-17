@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo RIGOR CLI launcher
echo ==================
echo rigor-cli.exe is a console program (double-click alone will flash and exit).
echo.

set "DATADIR="
if exist "%~dp0RIGOR Alpha\chainstate" set "DATADIR=%~dp0RIGOR Alpha"
if not defined DATADIR if exist "%~dp0datadir\chainstate" set "DATADIR=%~dp0datadir"
if not defined DATADIR if exist "%APPDATA%\RIGOR\chainstate" set "DATADIR=%APPDATA%\RIGOR"
if not defined DATADIR set "DATADIR=%APPDATA%\RIGOR"

if exist "%~dp0load-rpc-auth.cmd" (
  call "%~dp0load-rpc-auth.cmd"
  if errorlevel 1 (
    echo ERROR: RPC credentials are not available.
    echo Start the node once to generate rpc-auth.conf, then retry.
    pause
    exit /b 1
  )
) else (
  echo ERROR: load-rpc-auth.cmd missing. Do not use default rpcuser/rpcpassword.
  pause
  exit /b 1
)

echo Example:
echo   rigor-cli.exe -rpcuser=^<generated^> -rpcpassword=^<generated^> getblockchaininfo
echo.

rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% getblockchaininfo
set "EC=%ERRORLEVEL%"
echo.
echo Exit code %EC%.
echo.
pause
endlocal
