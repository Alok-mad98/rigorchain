@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo --------------------------------
echo RIGOR Miner launcher
echo --------------------------------
echo Start Rigor Wallet first so RPC is up, then run this miner.
echo ORU payout only ^(rg1q...^). Coinbase scriptSig / RES mint / paid-winner
echo are copied from getblocktemplate; the miner only searches OpuHash nonces.
echo Tip: set COINBASE_ADDR=rg1q... to skip the prompt.
echo.

if exist "rigor-miner.exe" (
  echo Local rigor-miner.exe:
  dir /T:W "rigor-miner.exe" | findstr /i "rigor-miner.exe"
  echo.
)

set "RPC_URL=http://127.0.0.1:8761"
set "DATADIR="
if exist "%~dp0RIGOR Alpha\chainstate" set "DATADIR=%~dp0RIGOR Alpha"
if not defined DATADIR if exist "%~dp0datadir\chainstate" set "DATADIR=%~dp0datadir"
if not defined DATADIR if exist "%APPDATA%\RIGOR\chainstate" set "DATADIR=%APPDATA%\RIGOR"
if not defined DATADIR set "DATADIR=%APPDATA%\RIGOR"
if exist "%~dp0load-rpc-auth.cmd" (
  call "%~dp0load-rpc-auth.cmd"
  if errorlevel 1 (
    echo ERROR: RPC credentials missing. Start the node once, then retry.
    pause
    exit /b 1
  )
) else (
  echo ERROR: load-rpc-auth.cmd missing. Default rpcuser/rpcpassword are not allowed.
  pause
  exit /b 1
)

if not defined MINER_THREADS set "MINER_THREADS=%NUMBER_OF_PROCESSORS%"
if "%MINER_THREADS%"=="" set "MINER_THREADS=4"
if %MINER_THREADS% LSS 1 set "MINER_THREADS=1"

if defined COINBASE_ADDR goto have_addr
set /p COINBASE_ADDR=ORU mining payout address: 
:have_addr
if "%COINBASE_ADDR%"=="" (
  echo.
  echo ERROR: ORU payout address is required ^(rg1q...^).
  echo.
  pause
  exit /b 1
)

echo %COINBASE_ADDR% | findstr /i /b /c:"rg1p" >nul 2>&1
if not errorlevel 1 (
  echo.
  echo WARNING: that looks like a RES address ^(rg1p...^).
  echo Miner payout must be ORU ^(rg1q...^). Convergence RES goes to the draw winner.
  echo.
)

if exist "rigor-cli.exe" (
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 getblocktemplate "{\"rules\":[\"segwit\"],\"coinbase_address\":\"%COINBASE_ADDR%\"}" >"%TEMP%\rigor-gbt-preflight.txt" 2>&1
  if errorlevel 1 (
    echo.
    echo ERROR: getblocktemplate failed. Is the node running with RPC on port 8761?
    echo.
    type "%TEMP%\rigor-gbt-preflight.txt"
    echo.
    pause
    exit /b 1
  )
)

echo.
echo Threads: %MINER_THREADS%
echo ORU payout: %COINBASE_ADDR%
echo Starting miner...
echo.

rigor-miner.exe -a sha256d -q -t %MINER_THREADS% --no-getwork --coinbase-addr="%COINBASE_ADDR%" -o "%RPC_URL%" -u "%RPC_USER%" -p "%RPC_PASS%"
set "EC=%ERRORLEVEL%"

echo.
echo Miner exited with code %EC%.
if not "%EC%"=="0" (
  echo.
  echo Common causes:
  echo  - Node/RPC down or wrong user/pass
  echo  - Missing solomining=1 while Peers=0
  echo  - Invalid ORU --coinbase-addr
  echo  - Stale node: coinbase scriptSig must be height + C + paid-winner (copy default_coinbase_scriptsig)
)
echo.
pause
endlocal
