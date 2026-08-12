@echo off
setlocal
cd /d "%~dp0"

echo.
echo RIGOR CLI launcher
echo ==================
echo rigor-cli.exe is a console program (double-click alone will flash and exit).
echo.

if not exist "%APPDATA%\RIGOR\rigor.conf" (
  echo WARNING: %APPDATA%\RIGOR\rigor.conf not found.
  echo Create it with:
  echo   server=1
  echo   rpcuser=user
  echo   rpcpassword=pass
  echo   rpcallowip=127.0.0.1
  echo.
)

echo Example:
echo   rigor-cli.exe -rpcuser=user -rpcpassword=pass getblockchaininfo
echo.

rigor-cli.exe -rpcuser=user -rpcpassword=pass getblockchaininfo
set "EC=%ERRORLEVEL%"
echo.
echo Exit code %EC%.
echo.
pause
endlocal
