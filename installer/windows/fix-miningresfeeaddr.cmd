@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
REM One-shot: create rg1p RES addr on a loaded wallet, write miningresfeeaddr, restart.
REM Multi-wallet nodes need /wallet/<name> (error -19 without it).
REM Optional: set RPCWALLET=JACK before running.

set "DATADIR="
if exist "%~dp0RIGOR Alpha\chainstate" set "DATADIR=%~dp0RIGOR Alpha"
if not defined DATADIR if exist "%APPDATA%\RIGOR\chainstate" set "DATADIR=%APPDATA%\RIGOR"
if not defined DATADIR set "DATADIR=%~dp0RIGOR Alpha"
if not exist "%DATADIR%" mkdir "%DATADIR%"
set "CONF=%DATADIR%\rigor.conf"

echo Datadir: %DATADIR%
echo Conf:    %CONF%
if defined RPCWALLET echo Forced RPCWALLET=%RPCWALLET%
echo.

if not exist "%~dp0load-rpc-auth.cmd" (
  echo ERROR: load-rpc-auth.cmd missing. Default rpcuser/rpcpassword are not allowed.
  pause
  exit /b 1
)
call "%~dp0load-rpc-auth.cmd"
if errorlevel 1 (
  echo ERROR: cannot load generated RPC credentials from %DATADIR%
  echo Run start-node.cmd first so rpc-auth.conf is generated.
  pause
  exit /b 1
)

if not exist "%~dp0load-rpc-auth.cmd" (
  echo ERROR: load-rpc-auth.cmd missing. Default rpcuser/rpcpassword are not allowed.
  pause
  exit /b 1
)
call "%~dp0load-rpc-auth.cmd"
if errorlevel 1 (
  echo ERROR: cannot load generated RPC credentials from %DATADIR%
  echo Run start-node.cmd first so rpc-auth.conf is generated.
  pause
  exit /b 1
)


findstr /i /b /c:"miningresfeeaddr=" "%CONF%" >nul 2>&1
if not errorlevel 1 (
  echo miningresfeeaddr already in conf:
  findstr /i /b /c:"miningresfeeaddr=" "%CONF%"
  echo If GBT still fails, close wallet and re-run start-node.cmd so it loads the flag.
  pause
  exit /b 0
)

echo Creating RES address via wallet RPC...
del "%TEMP%\rigor-res-addr.txt" "%TEMP%\rigor-res-wallet.txt" "%TEMP%\rigor-res-addr.err" >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$u=$env:RPC_USER; $p=$env:RPC_PASS; if (-not $u -or -not $p) { throw 'RPC credentials not loaded' };" ^
  "$c=[Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($u+':'+$p));" ^
  "$h=@{Authorization=('Basic '+$c)}; $base='http://127.0.0.1:8761';" ^
  "function Rpc([string]$uri,[string]$method,$params=@()) {" ^
  "  $body=@{jsonrpc='1.0';id='r';method=$method;params=$params}|ConvertTo-Json -Compress -Depth 8;" ^
  "  return Invoke-RestMethod -Uri $uri -Headers $h -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 45" ^
  "};" ^
  "try {" ^
  "  $forced=$env:RPCWALLET;" ^
  "  $names=@();" ^
  "  if ($forced) { $names=@($forced) } else {" ^
  "    $lw=Rpc $base/ 'listwallets';" ^
  "    if ($null -ne $lw.error -and $lw.error -ne '') { throw ('listwallets: '+($lw.error|ConvertTo-Json -Compress)) };" ^
  "    if ($lw.result) { $names=@($lw.result) };" ^
  "    if ($names.Count -eq 0) {" ^
  "      $wdir=Join-Path $env:DATADIR_OVERRIDE 'wallets';" ^
  "      if (-not $env:DATADIR_OVERRIDE) { $wdir='C:\rigor\RIGOR Alpha\wallets' };" ^
  "    }" ^
  "  };" ^
  "  if (-not $forced -and $names.Count -eq 0) {" ^
  "    $dirs=Get-ChildItem -Path (Join-Path '%DATADIR%' 'wallets') -Directory -ErrorAction SilentlyContinue;" ^
  "    foreach ($d in $dirs) {" ^
  "      try { $null=Rpc $base/ 'loadwallet' @($d.Name); $names += $d.Name } catch {}" ^
  "    }" ^
  "  };" ^
  "  if ($names.Count -eq 0) { throw 'No wallets loaded. Open a wallet in the UI, or set RPCWALLET=JACK' };" ^
  "  $prefer=@('JACK','SUDO') + $names | Select-Object -Unique;" ^
  "  $order=@(); foreach ($p in $prefer) { if ($names -contains $p) { $order += $p } };" ^
  "  foreach ($n in $names) { if ($order -notcontains $n) { $order += $n } };" ^
  "  $last='';" ^
  "  foreach ($w in $order) {" ^
  "    Write-Host ('Trying wallet: '+$w);" ^
  "    try {" ^
  "      $r=Rpc ($base+'/wallet/'+[uri]::EscapeDataString($w)) 'getnewaddress' @('mining-res-fee','RES');" ^
  "      if ($null -ne $r.error -and $r.error -ne '') { $last=($r.error|ConvertTo-Json -Compress); continue };" ^
  "      if (-not $r.result) { $last='empty result'; continue };" ^
  "      $addr=[string]$r.result;" ^
  "      if ($addr -notlike 'rg1p*') { $last=('not rg1p: '+$addr); continue };" ^
  "      [IO.File]::WriteAllText(($env:TEMP+'\rigor-res-addr.txt'), $addr);" ^
  "      [IO.File]::WriteAllText(($env:TEMP+'\rigor-res-wallet.txt'), $w);" ^
  "      Write-Host ('OK wallet='+$w+' addr='+$addr); exit 0" ^
  "    } catch { $last=$_.Exception.Message; continue }" ^
  "  };" ^
  "  throw ('all wallets failed: '+$last)" ^
  "} catch {" ^
  "  $_.Exception.Message | Out-File -FilePath ($env:TEMP+'\rigor-res-addr.err') -Encoding ascii;" ^
  "  Write-Host $_; exit 1" ^
  "}"

if errorlevel 1 (
  echo PowerShell path failed. Trying rigor-cli -rpcwallet=...
  if not exist "rigor-cli.exe" (
    echo ERROR: rigor-cli.exe missing
    if exist "%TEMP%\rigor-res-addr.err" type "%TEMP%\rigor-res-addr.err"
    pause
    exit /b 1
  )
  set "WNAME=%RPCWALLET%"
  if not defined WNAME if exist "%DATADIR%\wallets\JACK" set "WNAME=JACK"
  if not defined WNAME if exist "%DATADIR%\wallets\SUDO" set "WNAME=SUDO"
  if not defined WNAME (
    for /d %%D in ("%DATADIR%\wallets\*") do (
      if not defined WNAME set "WNAME=%%~nxD"
    )
  )
  if not defined WNAME (
    echo ERROR: no wallet name found under %DATADIR%\wallets
    if exist "%TEMP%\rigor-res-addr.err" type "%TEMP%\rigor-res-addr.err"
    pause
    exit /b 1
  )
  echo Using -rpcwallet=!WNAME!
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcwallet=!WNAME! getnewaddress mining-res-fee RES >"%TEMP%\rigor-res-addr.txt" 2>"%TEMP%\rigor-res-addr.err"
  echo !WNAME!>"%TEMP%\rigor-res-wallet.txt"
)

set "ADDR="
set "WNAME="
if exist "%TEMP%\rigor-res-addr.txt" set /p ADDR=<"%TEMP%\rigor-res-addr.txt"
if exist "%TEMP%\rigor-res-wallet.txt" set /p WNAME=<"%TEMP%\rigor-res-wallet.txt"
if defined ADDR for /f "tokens=* delims= " %%A in ("!ADDR!") do set "ADDR=%%A"
if defined WNAME for /f "tokens=* delims= " %%A in ("!WNAME!") do set "WNAME=%%A"

echo !ADDR! | findstr /i /b /c:"rg1p" >nul 2>&1
if errorlevel 1 (
  echo ERROR: did not get rg1p... address.
  if exist "%TEMP%\rigor-res-addr.err" type "%TEMP%\rigor-res-addr.err"
  echo.
  echo Your node has multiple wallets ^(JACK / SUDO^). Try:
  echo   set RPCWALLET=JACK
  echo   fix-miningresfeeaddr.cmd
  echo Or in UI: Receive -^> RES address, then:
  echo   echo miningresfeeaddr=rg1p...^>^>"%CONF%"
  pause
  exit /b 1
)

echo miningresfeeaddr=!ADDR!>>"%CONF%"
echo Wrote miningresfeeaddr=!ADDR!  ^(from wallet !WNAME!^)
echo.
echo Stopping wallet to reload config...
if exist "rigor-cli.exe" rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 stop >nul 2>&1
timeout /t 5 /nobreak >nul
taskkill /IM rigor-wallet.exe /F >nul 2>&1
timeout /t 2 /nobreak >nul

echo Starting wallet with -miningresfeeaddr...
start "RIGOR Wallet" /D "%~dp0" "%~dp0rigor-wallet.exe" -datadir="%DATADIR%" -server=1 -miningresfeeaddr="!ADDR!"

echo Waiting for RPC...
set /a N=0
:wait
rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcclienttimeout=3 getblockcount >nul 2>&1
if not errorlevel 1 goto :ok
set /a N+=1
if %N% GEQ 40 (
  echo Timed out. Open wallet manually, then re-run this script.
  pause
  exit /b 1
)
timeout /t 3 /nobreak >nul
goto :wait

:ok
echo.
echo Done. miningresfeeaddr=!ADDR!  wallet=!WNAME!
echo Next: rigor-miner.cmd with ORU rg1q... payout
echo.
pause
exit /b 0
