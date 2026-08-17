# RIGOR Windows AlphaPlus acceptance checklist (run on a fresh Windows machine)
# Historical Alpha1 naming before AlphaPlus release identity migration (filename kept).
# No consensus changes. Operator executes after installing rigor-wallet-0.1.1-alphaplus.exe

param(
    [string]$InstallDir = "${env:ProgramFiles}\RIGOR AlphaPlus",
    [string]$DataDir = "$env:APPDATA\RIGOR",
    [switch]$Regtest
)

$ErrorActionPreference = "Stop"
Write-Host "=== RIGOR Windows AlphaPlus Acceptance ==="
Write-Host "InstallDir=$InstallDir"
Write-Host "DataDir=$DataDir"

function Assert-File($p) {
    if (-not (Test-Path $p)) { throw "MISSING: $p" }
    Write-Host "PASS present $p"
}

Assert-File "$InstallDir\rigor-wallet.exe"
Assert-File "$InstallDir\rigord.exe"
Assert-File "$InstallDir\rigor-miner.exe"

# PE MZ check
foreach ($bin in @("rigor-wallet.exe","rigord.exe","rigor-miner.exe")) {
    $bytes = [System.IO.File]::ReadAllBytes((Join-Path $InstallDir $bin))[0..1]
    if (-not ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A)) { throw "NOT PE: $bin" }
    Write-Host "PASS MZ $bin"
}

Write-Host @"

Manual UAT (GUI + local mining):
  1. Launch Start Menu → RIGOR Alpha → RIGOR Wallet
  2. Create wallet / backup
  3. Start node (embedded or rigor-wallet connection to local rigord)
  4. Receive ORU (mine or send)
  5. Run rigor-miner.exe against local RPC
  6. Create CONVERGENCE
  7. Observe W=32 lifecycle in wallet
  8. Confirm 100.00 RES minted (or verified no-mint)

Regtest tip:
  & '$InstallDir\rigord.exe' -regtest -datadir='$DataDir\regtest' -daemon
  & '$InstallDir\rigor-wallet.exe' -regtest -datadir='$DataDir\regtest'

Datadir is retained on uninstall ($DataDir).
"@
