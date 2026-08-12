@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo --------------------------------
echo RIGOR Miner launcher
echo --------------------------------
echo Start rigor-wallet.exe or rigord.exe first.
echo.
echo Config: rigor.conf next to rigor-wallet.exe (portable)
echo     or  %%APPDATA%%\RIGOR\rigor.conf
echo.
echo Required:
echo   server=1
echo   rpcuser=user
echo   rpcpassword=pass
echo   rpcallowip=127.0.0.1
echo   solomining=1
echo.
echo Mainnet RPC: 127.0.0.1:8761
echo Payout: wallet Receive ORU address (rg1...)
echo Tip: set COINBASE_ADDR=rg1... to skip the prompt.
echo.
if exist "rigor-miner.exe" (
  echo Local rigor-miner.exe:
  dir /T:W "rigor-miner.exe" | findstr /i "rigor-miner.exe"
  echo.
)

set "RPC_URL=http://127.0.0.1:8761"
set "RPC_USER=user"
set "RPC_PASS=pass"

REM Default: all logical CPUs. Override with set MINER_THREADS=N
if not defined MINER_THREADS set "MINER_THREADS=%NUMBER_OF_PROCESSORS%"
if "%MINER_THREADS%"=="" set "MINER_THREADS=4"
if %MINER_THREADS% LSS 1 set "MINER_THREADS=1"

if defined COINBASE_ADDR goto have_addr
set /p COINBASE_ADDR=ORU mining payout address: 
:have_addr
if "%COINBASE_ADDR%"=="" (
  echo.
  echo ERROR: payout address is required.
  echo Example: set COINBASE_ADDR=rg1q...
  echo Then run this launcher again.
  echo.
  pause
  exit /b 1
)

echo.
echo Threads: %MINER_THREADS%
echo Payout:  %COINBASE_ADDR%
echo Starting miner...
echo.

rigor-miner.exe -a sha256d -q -t %MINER_THREADS% --no-getwork --coinbase-addr="%COINBASE_ADDR%" -o "%RPC_URL%" -u "%RPC_USER%" -p "%RPC_PASS%"
set "EC=%ERRORLEVEL%"

echo.
echo Miner exited with code %EC%.
if not "%EC%"=="0" (
  echo.
  echo Common causes:
  if "%EC%"=="-1073741515" (
    echo  - MISSING DLL ^(0xC0000135^): update rigor-miner.exe from current Alpha package
    echo    ^(must be static-linked; should NOT need libwinpthread-1.dll^)
  )
  echo  - Mismatched package: update BOTH rigor-miner.exe AND rigor-miner.cmd together
  echo  - Node not running or RPC not enabled
  echo  - Wrong rpcuser/rpcpassword or port
  echo  - Missing solomining=1 while Peers=0 ^(code -9^)
  echo  - Invalid --coinbase-addr
  echo  - OpusHash self-test failure - copy the text above
)
echo.
pause
endlocal
