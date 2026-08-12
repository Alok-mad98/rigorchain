# Unblock solo mining after bad-rigor-res-winner-mismatch (Stage3 large burn won).
# Requires rebuilt rigor-wallet with mainnet auto-mint + getautomintfundaddress.
#
# Usage (PowerShell, wallet RPC up):
#   powershell -NoProfile -ExecutionPolicy Bypass -File unblock_res_mint_mining.ps1

param(
    [string]$RpcUrl = "http://127.0.0.1:8761/",
    [string]$RpcUser = "user",
    [string]$RpcPass = "pass",
    [string]$InstallDir = "C:\rigor\RIGOR Alpha",
    [decimal]$FundOru = 1.0
)

$ErrorActionPreference = "Stop"
$cred = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("${RpcUser}:${RpcPass}"))
function Rpc([string]$method, $params = @()) {
    $body = @{ jsonrpc = "1.0"; id = "unblock"; method = $method; params = $params } |
        ConvertTo-Json -Compress -Depth 20
    $r = Invoke-RestMethod -Uri $RpcUrl -Method Post -Headers @{ Authorization = ("Basic " + $cred) } `
        -ContentType "application/json" -Body $body -TimeoutSec 60
    if ($null -ne $r.error -and $r.error -ne "") {
        throw ("RPC {0}: {1}" -f $method, ($r.error | ConvertTo-Json -Compress))
    }
    return $r.result
}

Write-Host "=== 1) Automint fund address ==="
$fund = Rpc "getautomintfundaddress"
Write-Host ("address={0}" -f $fund.address)

Write-Host "=== 2) RES fee / signal address (-miningresfeeaddr) ==="
$resAddr = [string](Rpc "getnewaddress" @("mining-res-fee", "RES"))
Write-Host ("miningresfeeaddr={0}" -f $resAddr)

Write-Host "=== 3) Fund automint bootstrap with {0} ORU ===" -f $FundOru
$txid = Rpc "sendtoaddress" @($fund.address, $FundOru, "automint-fund", "", $false, $false, $null, "unset", $null, 1.1)
Write-Host ("fund_txid={0} (may sit in mempool until next block)" -f $txid)

$confPath = Join-Path $InstallDir "rigor.conf"
$conf = Get-Content $confPath -Raw -ErrorAction SilentlyContinue
if ($conf -notmatch 'miningresfeeaddr=') {
    Add-Content -Path $confPath -Value ("miningresfeeaddr={0}" -f $resAddr)
    Write-Host ("appended miningresfeeaddr to {0}" -f $confPath)
} else {
    Write-Host "rigor.conf already has miningresfeeaddr — update manually if needed:"
    Write-Host ("  miningresfeeaddr={0}" -f $resAddr)
}

Write-Host @"

=== NEXT (operator) ===
1. Rebuild & replace rigor-wallet.exe (mainnet auto-mint patch).
2. Restart rigor-wallet so -miningresfeeaddr is loaded.
3. Ensure fund tx is in mempool (or already confirmed).
4. Restart rigor-miner / GBT — CreateNewBlock should insert the RES mint.
5. After tip advances, check: getbalances -> res.trusted should become 100.00

"@
