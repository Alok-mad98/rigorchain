@echo off
REM Load or generate AlphaPlus RPC credentials (CSW-8).
REM Sets RPC_USER and RPC_PASS. Expects DATADIR to be set.
if not defined DATADIR (
  echo ERROR: DATADIR is not set.
  exit /b 1
)
if not exist "%DATADIR%" mkdir "%DATADIR%"
set "RPCAUTH=%DATADIR%\rpc-auth.conf"
set "RPC_USER="
set "RPC_PASS="

if exist "%RPCAUTH%" goto :read_auth

echo Generating RPC authentication credentials...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$dir=$env:DATADIR; if (-not $dir) { $dir='%~1' };" ^
  "$bytes=New-Object byte[] 40; [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes);" ^
  "$user='rigor_' + ([BitConverter]::ToString($bytes[0..7]) -replace '-').ToLowerInvariant();" ^
  "$pass=([BitConverter]::ToString($bytes[8..39]) -replace '-').ToLowerInvariant();" ^
  "$p=Join-Path $dir 'rpc-auth.conf';" ^
  "@('# AlphaPlus generated RPC credentials. Do not share.','rpcuser='+$user,'rpcpassword='+$pass) | Set-Content -Path $p -Encoding ascii;" ^
  "Write-Output ('generated '+$p)"
if errorlevel 1 (
  echo ERROR: failed to generate RPC credentials.
  exit /b 1
)
echo Generated configuration:
echo   file: %RPCAUTH%

:read_auth
for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b /c:"rpcuser=" "%RPCAUTH%"`) do set "RPC_USER=%%B"
for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b /c:"rpcpassword=" "%RPCAUTH%"`) do set "RPC_PASS=%%B"
if not defined RPC_USER (
  echo ERROR: rpcuser missing in %RPCAUTH%
  exit /b 1
)
if not defined RPC_PASS (
  echo ERROR: rpcpassword missing in %RPCAUTH%
  exit /b 1
)
if /I "%RPC_USER%"=="user" if /I "%RPC_PASS%"=="pass" (
  echo ERROR: default rpcuser/rpcpassword are not allowed on AlphaPlus mainnet.
  echo Delete %RPCAUTH% and restart to generate unique credentials.
  exit /b 1
)
if /I "%RPC_PASS%"=="pass" (
  echo ERROR: default rpcpassword is not allowed on AlphaPlus mainnet.
  exit /b 1
)
exit /b 0
