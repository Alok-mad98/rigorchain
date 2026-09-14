@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo --------------------------------
echo RIGOR local node ^(Windows solo^)
echo --------------------------------
echo This starts rigor-wallet.exe as a local RPC node for mining.
echo Miner RPC: 127.0.0.1:8761  (generated rpc-auth.conf)
echo SOLO mining pays ORU only. RFC-019: RES is not created by PoW / coinbase.
echo Network fees are ORU-only. Miner payout is ORU ^(rg1q...^) only.
echo.

if not exist "rigor-wallet.exe" if not exist "rigord.exe" (
  echo ERROR: rigor-wallet.exe / rigord.exe not found in:
  echo   %~dp0
  echo Copy this script next to the Alpha binaries, then run it again.
  echo.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

set "DATADIR="
if exist "%~dp0RIGOR Alpha\chainstate" set "DATADIR=%~dp0RIGOR Alpha"
if not defined DATADIR if exist "%~dp0datadir\chainstate" set "DATADIR=%~dp0datadir"
if not defined DATADIR if exist "%APPDATA%\RIGOR\chainstate" set "DATADIR=%APPDATA%\RIGOR"
if not defined DATADIR set "DATADIR=%APPDATA%\RIGOR"

if not exist "%DATADIR%" mkdir "%DATADIR%"
set "CONF=%DATADIR%\rigor.conf"

echo Datadir: %DATADIR%
echo Config:  %CONF%
echo.

tasklist /FI "IMAGENAME eq rigor-wallet.exe" 2>nul | find /I "rigor-wallet.exe" >nul
if not errorlevel 1 (
  echo ERROR: rigor-wallet.exe is still running.
  echo Close the wallet window AND the tray icon, wait 5 seconds, then run this script again.
  echo.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
tasklist /FI "IMAGENAME eq rigord.exe" 2>nul | find /I "rigord.exe" >nul
if not errorlevel 1 (
  echo ERROR: rigord.exe is still running. Close that console, then retry.
  echo.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

call :ensure_conf
call :strip_lottery_conf
call :load_rpc_auth
if errorlevel 1 (
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

if exist "rigor-cli.exe" (
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcclienttimeout=3 getblockcount >nul 2>&1
  if not errorlevel 1 (
    echo RPC already up on 127.0.0.1:8761
    goto :after_rpc
  )
)

REM Detect foreign listener on 8761 (common: WSL rigord mirrored via wslrelay)
netstat -ano | findstr /R /C:":8761 .*LISTENING" >nul 2>&1
if not errorlevel 1 (
  echo ERROR: 127.0.0.1:8761 is already in use by another process.
  echo Common cause: a Linux/WSL rigord still listening on 8761.
  echo.
  echo In WSL run:
  echo   ss -ltnp ^| grep 8761
  echo   then stop that rigord ^(rigor-cli stop^) and retry this script.
  echo.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

tasklist /FI "IMAGENAME eq rigor-wallet.exe" 2>nul | find /I "rigor-wallet.exe" >nul
if not errorlevel 1 (
  echo rigor-wallet.exe is running, but RPC is not listening.
  echo The wallet only reads rigor.conf at startup.
  echo.
  echo 1. Close the RIGOR Wallet window completely
  echo    ^(wait until it disappears from the taskbar^)
  echo 2. Press any key here to restart it with RPC enabled
  echo.
  pause
)

tasklist /FI "IMAGENAME eq rigord.exe" 2>nul | find /I "rigord.exe" >nul
if not errorlevel 1 (
  echo rigord.exe is running. Close that console, then re-run this script.
  echo.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

call :start_node_process

echo Waiting for RPC 127.0.0.1:8761 ...
set /a N=0
:wait_rpc
if exist "rigor-cli.exe" (
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcclienttimeout=3 getblockcount >nul 2>&1
  if not errorlevel 1 goto :after_rpc
)
set /a N+=1
if %N% GEQ 40 (
  echo.
  echo Timed out waiting for RPC.
  echo Confirm the wallet window is open, then run rigor-cli.cmd
  echo If a first-run wizard appears, finish it and re-run this script.
  echo.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
timeout /t 3 /nobreak >nul
goto :wait_rpc

:after_rpc
if exist "rigor-cli.exe" (
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 getblocktemplate "{\"rules\":[\"segwit\"]}" >"%TEMP%\rigor-start-node-gbt2.txt" 2>&1
  if errorlevel 1 (
    echo.
    echo WARNING: getblocktemplate failed. Common causes:
    echo  - missing solomining=1 while Peers=0
    echo  - miningresfeeaddr still in rigor.conf ^(mainnet rejects that flag^)
    echo  - RPC auth mismatch
    type "%TEMP%\rigor-start-node-gbt2.txt"
  )
)

echo.
echo Node ready. RPC 127.0.0.1:8761
echo Next: rigor-miner.cmd with ORU rg1q... payout
echo Mining pays ORU. RES is not mined; mint is a separate burn-to-mint transaction.
echo.
if /I not "%~1"=="/nopause" pause
exit /b 0

:ensure_conf
if not exist "%CONF%" goto :write_conf
findstr /i /c:"server=1" "%CONF%" >nul 2>&1 || echo server=1>>"%CONF%"
findstr /i /c:"includeconf=rpc-auth.conf" "%CONF%" >nul 2>&1 || echo includeconf=rpc-auth.conf>>"%CONF%"
findstr /i /c:"rpcallowip=" "%CONF%" >nul 2>&1 || echo rpcallowip=127.0.0.1>>"%CONF%"
findstr /i /c:"rpcbind=" "%CONF%" >nul 2>&1 || echo rpcbind=127.0.0.1>>"%CONF%"
findstr /i /c:"solomining=1" "%CONF%" >nul 2>&1 || echo solomining=1>>"%CONF%"
goto :eof

:write_conf
(
  echo # RIGOR local solo-mining node - used by rigor-miner.cmd
  echo server=1
  echo txindex=1
  echo listen=1
  echo includeconf=rpc-auth.conf
  echo rpcallowip=127.0.0.1
  echo rpcbind=127.0.0.1
  echo solomining=1
) > "%CONF%"
echo Wrote %CONF%
goto :eof

:strip_lottery_conf
REM RFC-019: -miningresfeeaddr is rejected on mainnet-shaped chains.
if not exist "%CONF%" goto :eof
findstr /i /b /c:"miningresfeeaddr=" "%CONF%" >nul 2>&1
if errorlevel 1 goto :eof
echo Removing miningresfeeaddr from %CONF% ^(not allowed on this chain after RFC-019^)
findstr /v /i /b /c:"miningresfeeaddr=" "%CONF%" > "%TEMP%\rigor-conf-rfc019.tmp"
move /y "%TEMP%\rigor-conf-rfc019.tmp" "%CONF%" >nul
goto :eof

:load_rpc_auth
if not exist "%~dp0load-rpc-auth.cmd" goto :missing_rpc_auth
call "%~dp0load-rpc-auth.cmd"
if errorlevel 1 exit /b 1
exit /b 0

:missing_rpc_auth
echo ERROR: load-rpc-auth.cmd missing. Refusing default RPC credentials.
exit /b 1

:start_node_process
REM %~dp0 ends with \ and breaks start /D "C:\path\"
set "HERE=%CD%"
if exist "%HERE%\rigor-wallet.exe" (
  echo Starting rigor-wallet.exe with -server=1 -solomining=1
  start "" /D "%HERE%" "%HERE%\rigor-wallet.exe" -datadir="%DATADIR%" -server=1 -solomining=1
) else (
  echo Starting rigord.exe with -server=1 -solomining=1
  start "" /D "%HERE%" "%HERE%\rigord.exe" -datadir="%DATADIR%" -server=1 -solomining=1 -printtoconsole
)
goto :eof
