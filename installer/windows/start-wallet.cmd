@echo off
setlocal EnableExtensions
REM %~dp0 always ends with \ which breaks: start /D "C:\path\"
cd /d "%~dp0"
set "HERE=%CD%"
set "DATADIR=%APPDATA%\RIGOR"
if not exist "%HERE%\rigor-wallet.exe" (
  echo ERROR: rigor-wallet.exe not found in %HERE%
  pause
  exit /b 1
)
if not exist "%DATADIR%" mkdir "%DATADIR%"

REM v0.1.0-alpha shipped rpcuser=user / rpcpassword=pass. Production wallet refuses those
REM and used to look like a no-op if the error dialog was missed. Quarantine them.
call :quarantine_insecure_conf "%DATADIR%\rigor.conf"
call :quarantine_insecure_conf "%DATADIR%\bitcoin.conf"

echo Starting RIGOR Wallet
echo Data directory: %DATADIR%
echo If no window appears, open: %DATADIR%\debug.log
echo.

REM Empty title "" is required when the command path is quoted.
start "" /D "%HERE%" "%HERE%\rigor-wallet.exe" -datadir="%DATADIR%" %*
if errorlevel 1 (
  echo ERROR: failed to start rigor-wallet.exe
  pause
  exit /b 1
)
exit /b 0

:quarantine_insecure_conf
if not exist "%~1" goto :eof
findstr /i /c:"rpcpassword=pass" /c:"rpcuser=user" "%~1" >nul 2>&1
if errorlevel 1 goto :eof
echo WARNING: %~1 has default RPC secrets. Renaming to %~1.bad-default-rpc
move /y "%~1" "%~1.bad-default-rpc" >nul
goto :eof
