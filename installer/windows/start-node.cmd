@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo --------------------------------
echo RIGOR local node ^(Windows solo^)
echo --------------------------------
echo This starts rigor-wallet.exe as a local RPC node for mining.
echo Miner RPC: 127.0.0.1:8761  (generated rpc-auth.conf)
echo Convergence mint: wallet Settings "Auto-settle Convergence when mining locally" ^(default ON^)
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
set "RES_FEE_ADDR="
set "NEED_RESTART=0"

echo Datadir: %DATADIR%
echo Config:  %CONF%
echo.

call :ensure_conf
call :load_rpc_auth
if errorlevel 1 (
  if /I not "%~1"=="/nopause" pause
  exit /b 1
)
call :load_res_fee_from_conf

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
call :ensure_res_fee_addr
call :probe_gbt_res_fee
if "!NEED_RESTART!"=="1" (
  if defined RES_FEE_ADDR (
    echo.
    echo Convergence mint arming missing/stale in running node. Restarting...
    call :restart_node_with_res_fee
    set "NEED_RESTART=0"
    call :probe_gbt_res_fee
  )
)
if "!FATAL_RES!"=="1" (
  call :probe_gbt_res_fee
  if "!NEED_RESTART!"=="1" (
    echo.
    echo ERROR: Convergence mint assembly is not armed.
    echo In rigor-wallet: Settings -^> Wallet -^> enable
    echo   "Auto-settle Convergence when mining locally"
    echo Or set miningresfeeaddr=rg1p... in %CONF% and restart.
    echo.
    if /I not "%~1"=="/nopause" pause
    exit /b 1
  )
  echo NOTE: could not write miningresfeeaddr via RPC; relying on wallet Auto-settle if GBT works.
)

call :try_fund_automint

REM After funding attempt, surface remaining GBT blockers clearly
if exist "rigor-cli.exe" (
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 getblocktemplate "{\"rules\":[\"segwit\"]}" >"%TEMP%\rigor-start-node-gbt2.txt" 2>&1
  findstr /i /c:"automatic Convergence RES mint assembly failed" /c:"automatic RES mint assembly failed" "%TEMP%\rigor-start-node-gbt2.txt" >nul 2>&1
  if not errorlevel 1 (
    echo.
    echo WARNING: winner present but automint ORU fund UTXO not ready yet.
    echo If balances allow, send ORU to getautomintfundaddress and retry GBT.
  )
)

echo.
echo Node ready. RPC 127.0.0.1:8761
if defined RES_FEE_ADDR echo Convergence mint armed ^(miningresfeeaddr=!RES_FEE_ADDR!^)
echo Next: rigor-miner.cmd with ORU rg1q... payout
echo Wallet: keep "Auto-settle Convergence when mining locally" enabled
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

:load_rpc_auth
if exist "%~dp0load-rpc-auth.cmd" (
  call "%~dp0load-rpc-auth.cmd"
  exit /b %ERRORLEVEL%
)
echo ERROR: load-rpc-auth.cmd missing. Refusing default RPC credentials.
exit /b 1

:load_res_fee_from_conf
set "RES_FEE_ADDR="
if not exist "%CONF%" goto :eof
for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b /c:"miningresfeeaddr=" "%CONF%" 2^>nul`) do (
  set "RES_FEE_ADDR=%%B"
)
if defined RES_FEE_ADDR (
  for /f "tokens=* delims= " %%A in ("!RES_FEE_ADDR!") do set "RES_FEE_ADDR=%%A"
)
goto :eof

:ensure_res_fee_addr
set "FATAL_RES=0"
if not exist "rigor-cli.exe" (
  echo ERROR: rigor-cli.exe missing - cannot auto-create miningresfeeaddr.
  set "FATAL_RES=1"
  goto :eof
)
call :load_res_fee_from_conf
if defined RES_FEE_ADDR (
  echo Conf already has miningresfeeaddr=!RES_FEE_ADDR!
  goto :eof
)

echo.
echo Creating witness-v1 RES address to arm Convergence mint assembly ...
echo ^(Not a fee address — network fees are ORU-only; prize pays the Convergence winner^)
echo Prefer: wallet Settings -^> Auto-settle Convergence when mining locally
echo Datadir/conf: %CONF%
echo Note: multi-wallet nodes need -rpcwallet / /wallet/NAME ^(error -19 otherwise^)
set "RES_FEE_ADDR="
set "RPCWALLET="
set "RES_OUT=%TEMP%\rigor-res-addr.txt"
set "RES_WALLET_OUT=%TEMP%\rigor-res-wallet.txt"
set "RES_ERR=%TEMP%\rigor-res-addr.err"
del "%RES_OUT%" "%RES_WALLET_OUT%" "%RES_ERR%" >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$u=$env:RPC_USER; $p=$env:RPC_PASS; $c=[Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($u+':'+$p));" ^
  "$h=@{Authorization=('Basic '+$c)}; $base='http://127.0.0.1:8761'; $datadir='%~dp0';" ^
  "function Rpc([string]$uri,[string]$method,$params=@()) {" ^
  "  $body=@{jsonrpc='1.0';id='r';method=$method;params=$params}|ConvertTo-Json -Compress -Depth 8;" ^
  "  return Invoke-RestMethod -Uri $uri -Headers $h -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 45" ^
  "};" ^
  "try {" ^
  "  $forced=$env:RPCWALLET; $names=@();" ^
  "  if ($forced) { $names=@($forced) } else {" ^
  "    $lw=Rpc ($base+'/') 'listwallets';" ^
  "    if ($null -ne $lw.error -and $lw.error -ne '') { throw ('listwallets: '+($lw.error|ConvertTo-Json -Compress)) };" ^
  "    if ($lw.result) { $names=@($lw.result) }" ^
  "  };" ^
  "  if (-not $forced -and $names.Count -eq 0) {" ^
  "    $dirs=Get-ChildItem -Path (Join-Path '%DATADIR%' 'wallets') -Directory -ErrorAction SilentlyContinue;" ^
  "    foreach ($d in $dirs) { try { $null=Rpc ($base+'/') 'loadwallet' @($d.Name); $names += $d.Name } catch {} }" ^
  "  };" ^
  "  if ($names.Count -eq 0) { throw 'No wallets loaded. Open a wallet in the UI, or set RPCWALLET=JACK' };" ^
  "  $prefer=@('JACK','SUDO') + $names | Select-Object -Unique;" ^
  "  $order=@(); foreach ($x in $prefer) { if ($names -contains $x) { $order += $x } };" ^
  "  foreach ($n in $names) { if ($order -notcontains $n) { $order += $n } };" ^
  "  $last='';" ^
  "  foreach ($w in $order) {" ^
  "    Write-Host ('Trying wallet: '+$w);" ^
  "    try {" ^
  "      $r=Rpc ($base+'/wallet/'+[uri]::EscapeDataString($w)) 'getnewaddress' @('mining-res-fee','RES');" ^
  "      if ($null -ne $r.error -and $r.error -ne '') { $last=($r.error|ConvertTo-Json -Compress); continue };" ^
  "      if (-not $r.result) { $last='empty result'; continue };" ^
  "      $addr=[string]$r.result; if ($addr -notlike 'rg1p*') { $last=('not rg1p: '+$addr); continue };" ^
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

if exist "%RES_OUT%" set /p RES_FEE_ADDR=<"%RES_OUT%"
if exist "%RES_WALLET_OUT%" set /p RPCWALLET=<"%RES_WALLET_OUT%"

if not defined RES_FEE_ADDR (
  echo PowerShell getnewaddress failed; trying rigor-cli -rpcwallet=...
  if exist "%RES_ERR%" type "%RES_ERR%"
  if not defined RPCWALLET if exist "%DATADIR%\wallets\JACK" set "RPCWALLET=JACK"
  if not defined RPCWALLET if exist "%DATADIR%\wallets\SUDO" set "RPCWALLET=SUDO"
  if not defined RPCWALLET (
    for /d %%D in ("%DATADIR%\wallets\*") do if not defined RPCWALLET set "RPCWALLET=%%~nxD"
  )
  if defined RPCWALLET (
    echo Using -rpcwallet=!RPCWALLET!
    rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcwallet=!RPCWALLET! getnewaddress mining-res-fee RES >"%RES_OUT%" 2>"%RES_ERR%"
    set /p RES_FEE_ADDR=<"%RES_OUT%"
  )
)

if not defined RES_FEE_ADDR if defined RPCWALLET (
  echo Trying bech32m on wallet !RPCWALLET! ...
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcwallet=!RPCWALLET! getnewaddress mining-res-fee bech32m >"%RES_OUT%" 2>"%RES_ERR%"
  set /p RES_FEE_ADDR=<"%RES_OUT%"
)

if defined RES_FEE_ADDR for /f "tokens=* delims= " %%A in ("!RES_FEE_ADDR!") do set "RES_FEE_ADDR=%%A"
if defined RPCWALLET for /f "tokens=* delims= " %%A in ("!RPCWALLET!") do set "RPCWALLET=%%A"

if not defined RES_FEE_ADDR (
  echo ERROR: getnewaddress RES/bech32m failed.
  if exist "%RES_ERR%" (
    echo --- RPC error ---
    type "%RES_ERR%"
    echo -----------------
  )
  echo Tip: set RPCWALLET=JACK ^(or SUDO^) then re-run. Multi-wallet requires -rpcwallet.
  echo Or create RES address in UI and add miningresfeeaddr=rg1p... to:
  echo   %CONF%
  set "FATAL_RES=1"
  set "NEED_RESTART=0"
  goto :eof
)

echo !RES_FEE_ADDR! | findstr /i /b /c:"rg1p" >nul 2>&1
if errorlevel 1 (
  echo ERROR: expected rg1p... RES address, got: !RES_FEE_ADDR!
  set "RES_FEE_ADDR="
  set "FATAL_RES=1"
  set "NEED_RESTART=0"
  goto :eof
)

echo miningresfeeaddr=!RES_FEE_ADDR!>>"%CONF%"
echo Appended miningresfeeaddr to %CONF% ^(wallet=!RPCWALLET!^)
set "NEED_RESTART=1"
goto :eof

:probe_gbt_res_fee
REM Detect live GBT failure when Convergence mint assembly is not armed.
if not exist "rigor-cli.exe" goto :eof
rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 getblocktemplate "{\"rules\":[\"segwit\"]}" >"%TEMP%\rigor-start-node-gbt.txt" 2>&1
REM Only the "not armed" gate — not the automint-fund error text.
findstr /i /c:"mint assembly is not armed" /c:"no RES receive script" "%TEMP%\rigor-start-node-gbt.txt" >nul 2>&1
if not errorlevel 1 (
  echo GBT: Convergence mint assembly still not armed in the running process.
  set "NEED_RESTART=1"
) else (
  set "NEED_RESTART=0"
)
goto :eof

:start_node_process
if defined RES_FEE_ADDR (
  if exist "rigor-wallet.exe" (
    echo Starting rigor-wallet.exe with -server=1 -solomining=1 -miningresfeeaddr=...
    start "RIGOR Wallet" /D "%~dp0" "%~dp0rigor-wallet.exe" -datadir="%DATADIR%" -server=1 -solomining=1 -miningresfeeaddr="!RES_FEE_ADDR!"
  ) else (
    echo Starting rigord.exe with -server=1 -solomining=1 -miningresfeeaddr=...
    start "RIGOR Node" /D "%~dp0" "%~dp0rigord.exe" -datadir="%DATADIR%" -server=1 -solomining=1 -miningresfeeaddr="!RES_FEE_ADDR!" -printtoconsole
  )
) else (
  if exist "rigor-wallet.exe" (
    echo Starting rigor-wallet.exe with -server=1 -solomining=1
    start "RIGOR Wallet" /D "%~dp0" "%~dp0rigor-wallet.exe" -datadir="%DATADIR%" -server=1 -solomining=1
  ) else (
    echo Starting rigord.exe with -server=1 -solomining=1
    start "RIGOR Node" /D "%~dp0" "%~dp0rigord.exe" -datadir="%DATADIR%" -server=1 -solomining=1 -printtoconsole
  )
)
goto :eof

:restart_node_with_res_fee
if not defined RES_FEE_ADDR goto :eof
echo Stopping wallet/node to reload miningresfeeaddr...
if exist "rigor-cli.exe" (
  rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 stop >nul 2>&1
)
timeout /t 5 /nobreak >nul
taskkill /IM rigor-wallet.exe /F >nul 2>&1
taskkill /IM rigord.exe /FI "WINDOWTITLE eq RIGOR Wallet*" /F >nul 2>&1
timeout /t 2 /nobreak >nul
call :start_node_process
set /a N=0
:wait_rpc2
rigor-cli.exe -rpcuser=%RPC_USER% -rpcpassword=%RPC_PASS% -rpcport=8761 -rpcclienttimeout=3 getblockcount >nul 2>&1
if not errorlevel 1 (
  set "NEED_RESTART=0"
  goto :eof
)
set /a N+=1
if %N% GEQ 40 (
  echo WARNING: restart timed out - close wallet manually and re-run start-node.cmd
  goto :eof
)
timeout /t 3 /nobreak >nul
goto :wait_rpc2

:try_fund_automint
if not exist "rigor-cli.exe" goto :eof
if not defined RES_FEE_ADDR goto :eof
REM Best-effort: fund automint bootstrap so CreateNewBlock can attach RES mint.
REM sendtoaddress is wallet RPC — must use /wallet/NAME when multiple wallets loaded.
if not defined RPCWALLET if exist "%DATADIR%\wallets\JACK" set "RPCWALLET=JACK"
if not defined RPCWALLET if exist "%DATADIR%\wallets\SUDO" set "RPCWALLET=SUDO"
powershell -NoProfile -Command ^
  "$u=$env:RPC_USER;$p=$env:RPC_PASS;$c=[Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($u+':'+$p)); $h=@{Authorization=('Basic '+$c)}; $base='http://127.0.0.1:8761';" ^
  "$w=$env:RPCWALLET; if (-not $w) { $w='JACK' };" ^
  "try {" ^
  " $r=Invoke-RestMethod -Uri ($base+'/') -Headers $h -Method Post -ContentType 'application/json' -Body '{\"jsonrpc\":\"1.0\",\"id\":\"f\",\"method\":\"getautomintfundaddress\",\"params\":[]}' -TimeoutSec 15;" ^
  " if ($r.result.address) {" ^
  "  $txid=Invoke-RestMethod -Uri ($base+'/wallet/'+[uri]::EscapeDataString($w)) -Headers $h -Method Post -ContentType 'application/json' -Body ('{\"jsonrpc\":\"1.0\",\"id\":\"s\",\"method\":\"sendtoaddress\",\"params\":[\"'+$r.result.address+'\",1.0]}') -TimeoutSec 60;" ^
  "  if ($txid.result) { Write-Output ('automint_fund_txid='+$txid.result+' wallet='+$w) } else { Write-Output ('automint_fund_skip='+($txid.error.message)) }" ^
  " } else { Write-Output 'automint_fund_skip=no_address' }" ^
  "} catch { Write-Output ('automint_fund_skip='+$_.Exception.Message) }" 2>nul
goto :eof
