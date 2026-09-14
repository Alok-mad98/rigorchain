@echo off
REM Load or generate RPC credentials. Sets RPC_USER and RPC_PASS.
REM Requires DATADIR. Reads ASCII sidecar files (not findstr / PowerShell stdout).
if not defined DATADIR (
  echo ERROR: DATADIR is not set.
  exit /b 1
)
if not exist "%DATADIR%" mkdir "%DATADIR%"
set "RPC_USER="
set "RPC_PASS="
set "GENPS1=%~dp0gen-rpc-auth.ps1"
if not exist "%GENPS1%" (
  echo ERROR: missing %GENPS1%
  echo Copy gen-rpc-auth.ps1 next to load-rpc-auth.cmd
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%GENPS1%" "%DATADIR%"
if errorlevel 1 (
  echo ERROR: failed to write RPC credentials into %DATADIR%
  echo Close Rigor Wallet completely, then retry.
  exit /b 1
)

if not exist "%DATADIR%\rpc-user.txt" (
  echo ERROR: %DATADIR%\rpc-user.txt was not created.
  echo Close the wallet and retry.
  exit /b 1
)
if not exist "%DATADIR%\rpc-pass.txt" (
  echo ERROR: %DATADIR%\rpc-pass.txt was not created.
  echo Close the wallet and retry.
  exit /b 1
)

set /p RPC_USER=<"%DATADIR%\rpc-user.txt"
set /p RPC_PASS=<"%DATADIR%\rpc-pass.txt"
if not defined RPC_USER goto :fail_read
if not defined RPC_PASS goto :fail_read

if /I "%RPC_USER%"=="user" if /I "%RPC_PASS%"=="pass" (
  echo ERROR: default rpcuser/rpcpassword are not allowed.
  exit /b 1
)
if /I "%RPC_PASS%"=="pass" (
  echo ERROR: default rpcpassword is not allowed.
  exit /b 1
)
echo RPC credentials ready.
exit /b 0

:fail_read
echo ERROR: rpc-user.txt / rpc-pass.txt are empty.
echo Close the wallet, delete those two files, and retry start-node.cmd.
exit /b 1
