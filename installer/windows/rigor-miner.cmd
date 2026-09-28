@echo off
cd /d "%~dp0"
if /I not "%~1"=="GO" (
  cmd /k "%~f0" GO
  exit /b
)
title RIGOR MAINNET SOLO Miner
echo.
echo RIGOR MAINNET SOLO miner
echo.
REM Edit the lines below. Leave MINER_THREADS empty to keep a core free for the wallet.
set "RPC_USER=CHANGE_ME"
set "RPC_PASS=CHANGE_ME"
set "COINBASE_ADDR=rg1qCHANGE_ME"
set "MINER_THREADS="

if /I "%RPC_USER%"=="CHANGE_ME" goto need_edit
if /I "%RPC_PASS%"=="CHANGE_ME" goto need_edit
if /I "%COINBASE_ADDR%"=="rg1qCHANGE_ME" goto need_edit

if not exist "rigor-miner.exe" (
  echo rigor-miner.exe is missing from this folder.
  pause
  exit /b 1
)

set "THREAD_ARG="
if defined MINER_THREADS set "THREAD_ARG=-t %MINER_THREADS%"

echo Starting miner...
echo RPC http://127.0.0.1:8761  user=%RPC_USER%
echo Algorithm rgrpow01
echo Payout %COINBASE_ADDR%
if defined MINER_THREADS (
  echo Threads %MINER_THREADS%
) else (
  echo Threads auto - one core left for the wallet
)
echo.

rigor-miner.exe -a rgrpow01 --no-getwork -o "http://127.0.0.1:8761" -u "%RPC_USER%" -p "%RPC_PASS%" --coinbase-addr="%COINBASE_ADDR%" %THREAD_ARG%
echo.
echo Miner exited %ERRORLEVEL%
pause
exit /b %ERRORLEVEL%

:need_edit
echo Open this file in Notepad. Change RPC_USER, RPC_PASS, and COINBASE_ADDR.
echo Leave MINER_THREADS empty unless you want to pin a thread count.
pause
exit /b 1
