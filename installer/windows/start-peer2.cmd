@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo --------------------------------
echo RIGOR local peer-2 ^(Windows^)
echo --------------------------------
echo Starts a second mainnet node and peers it with the wallet node.
echo Primary P2P: 127.0.0.1:8760   Peer-2 P2P: 127.0.0.1:8762
echo Primary RPC: 127.0.0.1:8761   Peer-2 RPC: 127.0.0.1:8763
echo RPC credentials are read from generated rpc-auth.conf ^(never shipped^).
echo.

if not exist "rigord.exe" (
  echo ERROR: rigord.exe not found in %~dp0
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
if not exist "rigor-cli.exe" (
  echo ERROR: rigor-cli.exe not found in %~dp0
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
if not exist "load-rpc-auth.cmd" (
  echo ERROR: load-rpc-auth.cmd missing. Default rpcuser/rpcpassword are not allowed.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

set "PRIMARYDIR="
if exist "%~dp0RIGOR Alpha\chainstate" set "PRIMARYDIR=%~dp0RIGOR Alpha"
if not defined PRIMARYDIR if exist "%APPDATA%\RIGOR\chainstate" set "PRIMARYDIR=%APPDATA%\RIGOR"
if not defined PRIMARYDIR set "PRIMARYDIR=%APPDATA%\RIGOR"

set "DATADIR=%PRIMARYDIR%"
call "%~dp0load-rpc-auth.cmd"
if errorlevel 1 (
  echo ERROR: cannot load primary RPC credentials from %PRIMARYDIR%
  echo Run start-node.cmd first so rpc-auth.conf is generated.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
set "PRIMARY_USER=%RPC_USER%"
set "PRIMARY_PASS=%RPC_PASS%"

REM Primary must already be up (start-node.cmd)
rigor-cli.exe -rpcuser=%PRIMARY_USER% -rpcpassword=%PRIMARY_PASS% -rpcport=8761 -rpcclienttimeout=3 getblockcount >nul 2>&1
if errorlevel 1 (
  echo ERROR: primary node RPC 8761 is down.
  echo Run start-node.cmd first, wait for "Node ready", then run this.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)

set "PEERDIR=%~dp0peer2"
if not exist "%PEERDIR%" mkdir "%PEERDIR%"
set "CONF=%PEERDIR%\rigor.conf"

set "DATADIR=%PEERDIR%"
call "%~dp0load-rpc-auth.cmd"
if errorlevel 1 (
  echo ERROR: cannot generate or load peer-2 RPC credentials.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
set "PEER_USER=%RPC_USER%"
set "PEER_PASS=%RPC_PASS%"

set "MAINCONF="
if exist "%~dp0RIGOR Alpha\rigor.conf" set "MAINCONF=%~dp0RIGOR Alpha\rigor.conf"
if not defined MAINCONF if exist "%APPDATA%\RIGOR\rigor.conf" set "MAINCONF=%APPDATA%\RIGOR\rigor.conf"

set "RES_FEE_ADDR="
if defined MAINCONF (
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b /c:"miningresfeeaddr=" "%MAINCONF%" 2^>nul`) do set "RES_FEE_ADDR=%%B"
)
if defined RES_FEE_ADDR for /f "tokens=* delims= " %%A in ("!RES_FEE_ADDR!") do set "RES_FEE_ADDR=%%A"

(
  echo # Local AlphaPlus peer-2 - mesh with wallet node on 8760
  echo server=1
  echo listen=1
  echo txindex=1
  echo dnsseed=0
  echo discover=0
  echo includeconf=rpc-auth.conf
  echo rpcallowip=127.0.0.1
  echo rpcbind=127.0.0.1
  echo rpcport=8763
  echo port=8762
  echo bind=127.0.0.1
  echo addnode=127.0.0.1:8760
  echo connect=127.0.0.1:8760
  if defined RES_FEE_ADDR echo miningresfeeaddr=!RES_FEE_ADDR!
) > "%CONF%"

if defined RES_FEE_ADDR (
  echo Peer-2 miningresfeeaddr=!RES_FEE_ADDR! ^(copied from primary^)
) else (
  echo WARNING: primary has no miningresfeeaddr yet.
  echo Run start-node.cmd first so RES fee addr is created, then re-run peer-2.
)

REM Ensure primary will also dial peer-2 after restart
if defined MAINCONF (
  findstr /i /c:"addnode=127.0.0.1:8762" "%MAINCONF%" >nul 2>&1 || (
    echo addnode=127.0.0.1:8762>>"%MAINCONF%"
    echo Appended addnode=127.0.0.1:8762 to %MAINCONF%
  )
)

netstat -ano | findstr /R /C:":8762 .*LISTENING" >nul 2>&1
if not errorlevel 1 (
  echo Peer-2 P2P 8762 already listening.
  goto :mesh
)

echo Starting peer-2 rigord.exe datadir=%PEERDIR%
if defined RES_FEE_ADDR (
  start "RIGOR Peer-2" /D "%~dp0" "%~dp0rigord.exe" -datadir="%PEERDIR%" -miningresfeeaddr="!RES_FEE_ADDR!" -printtoconsole
) else (
  start "RIGOR Peer-2" /D "%~dp0" "%~dp0rigord.exe" -datadir="%PEERDIR%" -printtoconsole
)

echo Waiting for peer-2 RPC 8763 ...
set /a N=0
:wait_peer
rigor-cli.exe -rpcuser=%PEER_USER% -rpcpassword=%PEER_PASS% -rpcport=8763 -rpcclienttimeout=3 getblockcount >nul 2>&1
if not errorlevel 1 goto :mesh
set /a N+=1
if %N% GEQ 40 (
  echo Timed out waiting for peer-2 RPC.
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
timeout /t 2 /nobreak >nul
goto :wait_peer

:mesh
echo Wiring addnode both ways...
rigor-cli.exe -rpcuser=%PEER_USER% -rpcpassword=%PEER_PASS% -rpcport=8763 addnode "127.0.0.1:8760" "onetry" >nul 2>&1
rigor-cli.exe -rpcuser=%PEER_USER% -rpcpassword=%PEER_PASS% -rpcport=8763 addnode "127.0.0.1:8760" "add" >nul 2>&1
rigor-cli.exe -rpcuser=%PRIMARY_USER% -rpcpassword=%PRIMARY_PASS% -rpcport=8761 addnode "127.0.0.1:8762" "onetry" >nul 2>&1
rigor-cli.exe -rpcuser=%PRIMARY_USER% -rpcpassword=%PRIMARY_PASS% -rpcport=8761 addnode "127.0.0.1:8762" "add" >nul 2>&1

timeout /t 3 /nobreak >nul
echo.
echo Primary peers:
rigor-cli.exe -rpcuser=%PRIMARY_USER% -rpcpassword=%PRIMARY_PASS% -rpcport=8761 getconnectioncount
echo Peer-2 peers:
rigor-cli.exe -rpcuser=%PEER_USER% -rpcpassword=%PEER_PASS% -rpcport=8763 getconnectioncount
echo.
echo Wallet Convergence "Peers" should be ^>= 1 now.
echo Keep this peer-2 console open while testing Convergence.
echo.
if /I not "%~1"=="/nopause" pause
exit /b 0
