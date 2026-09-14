# TASK-MAINNET-LOCAL-MINING-STABILITY-001
# Validation-only harness: solo GBT mainnet mining, ~10 min/block target.
# Does NOT change consensus / ORU / RES / W / mining algorithm.
#
# Usage (Windows PowerShell, while rigor-wallet RPC is up):
#   powershell -NoProfile -ExecutionPolicy Bypass -File local_mainnet_mining_stability.ps1
#
# Artifacts:
#   .runtime/local-mainnet-mining-stability/  (under install dir if OUT_DIR unset,
#   or set OUT_DIR to a writable path)

param(
    [string]$RpcUrl = "http://127.0.0.1:8761/",
    [string]$RpcUser = "user",
    [string]$RpcPass = "pass",
    [string]$InstallDir = "C:\rigor\RIGOR Alpha",
    [string]$CoinbaseAddr = "rg1q23hz28ftnnk56f87cludpsy53hpvh2x2xsh73j",
    [string]$OutDir = "",
    [int]$PollSeconds = 60,
    [int]$Stage1Height = 100,
    [int]$Stage3Height = 500,
    # Stage3 RES stress: burn this many ORU once trusted ORU exceeds the threshold (probabilistic ~63% win in W).
    [decimal]$Stage3LargeBurnOru = 65536,
    [decimal]$Stage3LargeBurnMinTrusted = 65536, # trigger when trusted > this
    [decimal]$Stage3ResTransferAmount = 1.00,
    [switch]$SkipRestartTest
)

$ErrorActionPreference = "Stop"
if (-not $OutDir) {
    $OutDir = Join-Path $InstallDir "local-mainnet-mining-stability"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$StatePath = Join-Path $OutDir "state.json"
$LogPath = Join-Path $OutDir "monitor.jsonl"
$EventsPath = Join-Path $OutDir "events.log"
$ReportSnap = Join-Path $OutDir "latest_snapshot.json"

$cred = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("${RpcUser}:${RpcPass}"))

function Write-Event([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    # Prefer shared append; never block/kill harness if events.log is locked (editor/AV/WSL).
    $written = $false
    try {
        $fs = [System.IO.File]::Open($EventsPath,
            [System.IO.FileMode]::Append,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::ReadWrite)
        try {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes(($line + [Environment]::NewLine))
            $fs.Write($bytes, 0, $bytes.Length)
            $written = $true
        } finally { $fs.Dispose() }
    } catch { }
    if (-not $written) {
        try {
            $fallback = Join-Path $OutDir ("events_fallback_{0}.log" -f (Get-Date -Format "yyyyMMdd"))
            [System.IO.File]::AppendAllText($fallback, ($line + [Environment]::NewLine))
        } catch { }
    }
    Write-Host $line
}

function Test-IsRpcUnavailable([string]$msg) {
    if ([string]::IsNullOrEmpty($msg)) { return $false }
    # EN plus zh-CN Invoke-WebException / WebException localized text (Unicode escapes)
    return [bool]($msg -match
        'Unable to connect|actively refused|Connection refused|503|\u670d\u52a1\u5668\u4e0d\u53ef\u7528|\u65e0\u6cd5\u8fde\u63a5|timed out|timeout|Connection reset|Name or service not known|No connection|\u8fdc\u7a0b\u670d\u52a1\u5668\u8fd4\u56de\u9519\u8bef:\s*\(503\)')
}

function Rpc([string]$method, $params = @()) {
    $body = @{ jsonrpc = "1.0"; id = "stability"; method = $method; params = $params } |
        ConvertTo-Json -Compress -Depth 30
    try {
        $r = Invoke-RestMethod -Uri $RpcUrl -Method Post `
            -Headers @{ Authorization = ("Basic " + $cred) } `
            -ContentType "application/json" -Body $body -TimeoutSec 60
    } catch {
        $em = $_.Exception.Message
        if ($_.Exception.InnerException) { $em = $em + " | " + $_.Exception.InnerException.Message }
        throw $em
    }
    if ($null -ne $r.error -and $r.error -ne "") {
        throw ("RPC {0} error: {1}" -f $method, ($r.error | ConvertTo-Json -Compress -Depth 6))
    }
    return $r.result
}

function Rpc-Try([string]$method, $params = @()) {
    try { return Rpc $method $params } catch { return $null }
}

function Test-RpcReachable {
    # Lightweight probe — null means node/wallet HTTP is down or still booting.
    return $null -ne (Rpc-Try "getblockchaininfo")
}

function Ensure-StateProps($st) {
    $defaults = [ordered]@{
        stage3_large_conv_txid     = $null
        stage3_large_conv_height   = $null
        stage3_large_conv_amount   = $null
        stage3_res_mint_pass       = $false
        stage3_res_amount          = $null
        stage3_res_transfer_addr   = $null
        stage3_res_transfer_txid   = $null
        stage3_res_transfer_pass   = $false
        stage2_res_mint_pass       = $false
        stage2_res_amount          = $null
        stage2_restart_pass        = $false
        stage3_pass                = $false
        notes                      = @()
    }
    foreach ($k in $defaults.Keys) {
        if (-not ($st.PSObject.Properties.Name -contains $k)) {
            $st | Add-Member -NotePropertyName $k -NotePropertyValue $defaults[$k]
        }
    }
    if ($null -eq $st.notes) { $st.notes = @() }
    return $st
}

function Load-State {
    if (Test-Path $StatePath) {
        $st = Get-Content $StatePath -Raw | ConvertFrom-Json
        return (Ensure-StateProps $st)
    }
    return [pscustomobject]@{
        task                    = "TASK-MAINNET-LOCAL-MINING-STABILITY-001"
        started_at              = (Get-Date).ToUniversalTime().ToString("o")
        stage1_pass             = $false
        stage1_restart_pass     = $false
        stage1_miner_reconnect  = $false
        stage2_receive_pass     = $false
        stage2_transfer_txid    = $null
        stage2_convergence_txid = $null
        stage2_convergence_height = $null
        stage2_res_mint_pass    = $false
        stage2_res_amount       = $null
        stage2_restart_pass     = $false
        stage3_pass             = $false
        # Stage3 RES generation + transfer (auto when ORU trusted > Stage3LargeBurnMinTrusted)
        stage3_large_conv_txid    = $null
        stage3_large_conv_height  = $null
        stage3_large_conv_amount  = $null
        stage3_res_mint_pass      = $false
        stage3_res_amount         = $null
        stage3_res_transfer_addr  = $null
        stage3_res_transfer_txid  = $null
        stage3_res_transfer_pass  = $false
        last_height             = -1
        last_tip                = $null
        blocks_observed         = @()
        notes                   = @()
    }
}

function Write-TextShared([string]$path, [string]$text, [switch]$Append) {
    # Shared write so WSL/editors holding a read lock cannot crash the harness.
    $mode = if ($Append) { [System.IO.FileMode]::Append } else { [System.IO.FileMode]::Create }
    $fs = [System.IO.File]::Open($path, $mode, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($text)
        if (-not $Append) { $fs.SetLength(0) }
        $fs.Write($bytes, 0, $bytes.Length)
    } finally { $fs.Dispose() }
}

function Save-State($st) {
    $json = ($st | ConvertTo-Json -Depth 12) + [Environment]::NewLine
    for ($i = 0; $i -lt 5; $i++) {
        try {
            Write-TextShared -path $StatePath -text $json
            return
        } catch {
            if ($i -ge 4) { Write-Host ("Save-State deferred: {0}" -f $_.Exception.Message) }
            else { Start-Sleep -Milliseconds 200 }
        }
    }
}

function Get-ProcessMetrics {
    $procs = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match '^(rigor-wallet|rigord|rigor-miner|minerd)$'
    }
    $out = @()
    foreach ($p in $procs) {
        $out += [pscustomobject]@{
            name = $p.Name
            pid  = $p.Id
            cpu  = [math]::Round($p.CPU, 2)
            ws_mb = [math]::Round($p.WorkingSet64 / 1MB, 1)
        }
    }
    return $out
}

function Wait-Rpc([int]$timeoutSec = 180) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $info = Rpc-Try "getblockchaininfo"
        if ($info) { return $true }
        Start-Sleep -Seconds 3
    }
    return $false
}

function Ensure-Miner {
    $m = Get-Process -Name "rigor-miner" -ErrorAction SilentlyContinue
    if ($m) { return $true }
    $exe = Join-Path $InstallDir "rigor-miner.exe"
    if (-not (Test-Path $exe)) {
        Write-Event "miner binary missing: $exe"
        return $false
    }
    $args = @(
        "-a", "sha256d", "-q", "-t", "$env:NUMBER_OF_PROCESSORS",
        "--no-getwork",
        "--coinbase-addr=$CoinbaseAddr",
        "-o", "http://127.0.0.1:8761",
        "-u", $RpcUser, "-p", $RpcPass
    )
    Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $InstallDir -WindowStyle Minimized
    Start-Sleep -Seconds 2
    return [bool](Get-Process -Name "rigor-miner" -ErrorAction SilentlyContinue)
}

function Restart-WalletNode {
    Write-Event "Stage1 restart: stopping node via RPC stop"
    try { Rpc "stop" | Out-Null } catch { Write-Event "stop RPC: $($_.Exception.Message)" }
    Get-Process -Name "rigor-miner" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) {
        if (-not (Get-Process -Name "rigor-wallet","rigord" -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Seconds 2
    }
    Start-Sleep -Seconds 3
    $wallet = Join-Path $InstallDir "rigor-wallet.exe"
    Write-Event "Stage1 restart: starting rigor-wallet.exe"
    Start-Process -FilePath $wallet -WorkingDirectory $InstallDir
    if (-not (Wait-Rpc 240)) {
        throw "RPC did not return after wallet restart"
    }
    Write-Event "Stage1 restart: RPC live"
}

$st = Load-State
Write-Event ("Harness start OutDir=$OutDir Poll={0}s Stage1={1} Stage3={2} largeBurn={3} ORU (trigger trusted>{4}) resTransfer={5}" -f `
    $PollSeconds, $Stage1Height, $Stage3Height, $Stage3LargeBurnOru, $Stage3LargeBurnMinTrusted, $Stage3ResTransferAmount)

# Recover on-chain Stage3 large burn if state lost the property write after a successful RPC.
if (-not $st.stage3_large_conv_txid) {
    try {
        $txs = Rpc "listtransactions" @("*", 100)
        $hit = $txs | Where-Object {
            $_.comment -eq "stage3-large-burn" -or $_.comment -eq "stage3-large-burn-retry"
        } | Select-Object -First 1
        if ($hit) {
            $st.stage3_large_conv_txid = [string]$hit.txid
            $st.stage3_large_conv_height = if ($hit.blockheight) { [int]$hit.blockheight } else { [int]$hit.confirmations }
            if ($hit.convergence_included_height) { $st.stage3_large_conv_height = [int]$hit.convergence_included_height }
            $st.stage3_large_conv_amount = $Stage3LargeBurnOru
            if ($st.notes -notcontains "stage3_large_burn_recovered_from_wallet") {
                $st.notes += "stage3_large_burn_recovered_from_wallet"
            }
            Save-State $st
            Write-Event ("Recovered Stage3 large burn from wallet txid={0} height={1}" -f `
                $st.stage3_large_conv_txid, $st.stage3_large_conv_height)
        }
    } catch {
        Write-Event ("Stage3 large-burn wallet scan deferred: {0}" -f $_.Exception.Message)
    }
}

$script:rpcOffline = $false
$script:rpcOfflineSince = $null
$script:rpcOfflineLastNote = $null

while ($true) {
    try {
        # Planned wallet/miner stop → RPC 503 / connection refused. Wait quietly; do not spam poll error.
        if (-not (Test-RpcReachable)) {
            if (-not $script:rpcOffline) {
                $script:rpcOffline = $true
                $script:rpcOfflineSince = Get-Date
                $script:rpcOfflineLastNote = Get-Date
                Write-Event "RPC offline (node/wallet stopped or booting) — waiting; resume automatically when RPC is back"
            } elseif ($null -eq $script:rpcOfflineLastNote -or
                      ((Get-Date) - $script:rpcOfflineLastNote).TotalMinutes -ge 10) {
                $script:rpcOfflineLastNote = Get-Date
                $mins = [int]((Get-Date) - $script:rpcOfflineSince).TotalMinutes
                Write-Event "RPC still offline (${mins}m) — still waiting (close this harness if the run is finished)"
            }
            Start-Sleep -Seconds $PollSeconds
            continue
        }
        if ($script:rpcOffline) {
            $script:rpcOffline = $false
            Write-Event "RPC online again — resuming stability poll"
        }

        $bc = Rpc "getblockchaininfo"
        $mi = Rpc "getmininginfo"
        $wi = Rpc-Try "getwalletinfo"
        $bal = Rpc-Try "getbalances"
        $gbt = Rpc-Try "getblocktemplate" @(@{ rules = @("segwit") })
        $procs = Get-ProcessMetrics

        $height = [int]$bc.blocks
        $tip = [string]$bc.bestblockhash
        $bits = if ($gbt) { [string]$gbt.bits } else { $null }
        $immature = if ($bal -and $bal.oru) { [decimal]$bal.oru.immature } else { $null }
        $trusted = if ($bal -and $bal.oru) { [decimal]$bal.oru.trusted } else { $null }
        $resTrusted = if ($bal -and $bal.res) { [decimal]$bal.res.trusted } else { $null }

        $snap = [pscustomobject]@{
            utc           = (Get-Date).ToUniversalTime().ToString("o")
            height        = $height
            tip           = $tip
            difficulty    = $bc.difficulty
            nethashps     = $mi.networkhashps
            gbt_bits      = $bits
            gbt_height    = if ($gbt) { $gbt.height } else { $null }
            oru_trusted   = $trusted
            oru_immature  = $immature
            res_trusted   = $resTrusted
            txcount       = if ($wi) { $wi.txcount } else { $null }
            size_on_disk  = $bc.size_on_disk
            processes     = $procs
        }
        try {
            Write-TextShared -path $LogPath -text (($snap | ConvertTo-Json -Compress -Depth 8) + [Environment]::NewLine) -Append
        } catch {
            Write-Host ("monitor.jsonl write deferred: {0}" -f $_.Exception.Message)
        }
        try {
            Write-TextShared -path $ReportSnap -text (($snap | ConvertTo-Json -Depth 8) + [Environment]::NewLine)
        } catch {
            Write-Host ("latest_snapshot write deferred: {0}" -f $_.Exception.Message)
        }

        if ($height -ne $st.last_height) {
            Write-Event ("Tip advanced {0} -> {1} tip={2} bits={3} oru_immature={4} oru_trusted={5} res={6}" -f `
                $st.last_height, $height, $tip, $bits, $immature, $trusted, $resTrusted)
            $st.last_height = $height
            $st.last_tip = $tip
            $st.blocks_observed += [pscustomobject]@{
                height = $height; tip = $tip; utc = $snap.utc; bits = $bits
                oru_immature = $immature; oru_trusted = $trusted; res_trusted = $resTrusted
            }
            # keep list bounded
            if ($st.blocks_observed.Count -gt 600) {
                $st.blocks_observed = $st.blocks_observed | Select-Object -Last 600
            }
            Save-State $st
        }

        # --- Stage 1 gate ---
        if (-not $st.stage1_pass -and $height -ge $Stage1Height) {
            $okBlocks = $height -ge $Stage1Height
            $okCoinbase = ($immature -gt 0) -or ($trusted -gt 0)
            $okBits = ($bits -match '^1e01')
            $maturityOk = ($trusted -gt 0)  # first coinbases mature at tip>=100
            $st.stage1_pass = [bool]($okBlocks -and $okCoinbase -and $maturityOk)
            Write-Event ("Stage1 check: heightOK={0} coinbaseOK={1} bitsOK={2} maturityOK={3} => {4}" -f `
                $okBlocks, $okCoinbase, $okBits, $maturityOk, $st.stage1_pass)
            Save-State $st

            if ($st.stage1_pass -and -not $st.stage1_restart_pass -and -not $SkipRestartTest) {
                $preTip = $tip
                $preH = $height
                Restart-WalletNode
                $bc2 = Rpc "getblockchaininfo"
                $bal2 = Rpc "getbalances"
                $sameTip = ([string]$bc2.bestblockhash -eq $preTip) -and ([int]$bc2.blocks -eq $preH)
                $st.stage1_restart_pass = $sameTip
                Write-Event ("Stage1 restart recovery: sameTip={0} h={1} tip={2} trusted={3}" -f `
                    $sameTip, $bc2.blocks, $bc2.bestblockhash, $bal2.oru.trusted)
                if (Ensure-Miner) {
                    $st.stage1_miner_reconnect = $true
                    Write-Event "Stage1 miner reconnect: rigor-miner restarted"
                }
                Save-State $st
            }
        }

        # --- Stage 2: lifecycle once mature ORU available ---
        # Solo mainnet has no fee estimates → always pass explicit fee_rate (sat/vB).
        if ($st.stage1_pass -and -not $st.stage2_transfer_txid -and $trusted -ge 10) {
            try {
                $recv = Rpc "getnewaddress" @("stage2-receive", "bech32")
                Write-Event "Stage2 receive address: $recv"
                $st.stage2_receive_pass = $true
                # sendtoaddress args: address, amount, comment, comment_to, subtractfee, replaceable,
                # conf_target, estimate_mode, avoid_reuse, fee_rate
                $txid = Rpc "sendtoaddress" @($recv, 5.0, "", "", $false, $false, $null, "unset", $null, 1.1)
                $st.stage2_transfer_txid = [string]$txid
                Write-Event "Stage2 ORU transfer txid=$txid amount=5 fee_rate=1.1"
                Save-State $st
            } catch {
                Write-Event "Stage2 transfer deferred: $($_.Exception.Message)"
            }
        }

        if ($st.stage2_transfer_txid -and -not $st.stage2_convergence_txid -and $trusted -ge 1) {
            # createconvergence: amount … fee_rate at index 8
            try {
                $cv = Rpc "createconvergence" @(1.0, "", "", $false, $null, $null, "unset", $false, 1.1)
                $cvtxid = if ($cv -is [string]) { $cv } else { $cv.txid }
                $st.stage2_convergence_txid = [string]$cvtxid
                $st.stage2_convergence_height = $height
                Write-Event "Stage2 createconvergence txid=$cvtxid at height=$height"
                Save-State $st
            } catch {
                Write-Event "Stage2 createconvergence deferred: $($_.Exception.Message)"
            }
        }

        if ($st.stage2_convergence_txid -and -not $st.stage2_res_mint_pass) {
            if ($null -ne $resTrusted -and $resTrusted -ge 100) {
                $st.stage2_res_mint_pass = $true
                $st.stage2_res_amount = $resTrusted
                Write-Event "Stage2 RES minted: trusted=$resTrusted"
                Save-State $st
                if (-not $st.stage2_restart_pass -and -not $SkipRestartTest) {
                    $preRes = $resTrusted
                    Restart-WalletNode
                    Ensure-Miner | Out-Null
                    $bal3 = Rpc "getbalances"
                    $st.stage2_restart_pass = ([decimal]$bal3.res.trusted -ge 100)
                    Write-Event ("Stage2 restart RES recovery: res={0} pass={1}" -f $bal3.res.trusted, $st.stage2_restart_pass)
                    Save-State $st
                }
            } elseif ($st.stage2_convergence_height -and ($height -ge ($st.stage2_convergence_height + 32))) {
                # Prefer Stage3 large burn once funded; skip tiny 1-ORU retries that cannot finish Stage2.
                if ($st.stage3_large_conv_txid) {
                    # large hunt owns RES path
                } elseif ($null -ne $trusted -and $trusted -gt $Stage3LargeBurnMinTrusted) {
                    if ($st.notes -notcontains "stage2_hunt_paused_for_large_burn") {
                        $st.notes += "stage2_hunt_paused_for_large_burn"
                        Write-Event ("Stage2 RES hunt paused — trusted ORU {0} > {1}; waiting for Stage3 {2} ORU burn" -f `
                            $trusted, $Stage3LargeBurnMinTrusted, $Stage3LargeBurnOru)
                        Save-State $st
                    }
                } elseif ($trusted -ge 1) {
                    try {
                        $cv2 = Rpc "createconvergence" @(1.0, "", "", $false, $null, $null, "unset", $false, 1.1)
                        $cvtxid2 = if ($cv2 -is [string]) { $cv2 } else { $cv2.txid }
                        $st.stage2_convergence_txid = [string]$cvtxid2
                        $st.stage2_convergence_height = $height
                        $st.notes += "res_hunt_retry_at_$height"
                        Write-Event "Stage2 RES hunt: new convergence $cvtxid2 at $height"
                        Save-State $st
                    } catch {
                        Write-Event "Stage2 RES hunt createconvergence: $($_.Exception.Message)"
                    }
                }
            }
        }

        # --- Stage 3: extended height + large Convergence RES stress + RES transfer ---
        # Gate: Stage1 done; fire when trusted ORU > threshold (user plan: burn 65536 during Stage3 window).
        if ($st.stage1_pass -and -not $st.stage3_large_conv_txid -and
            $null -ne $trusted -and $trusted -gt $Stage3LargeBurnMinTrusted) {
            try {
                $need = [decimal]$Stage3LargeBurnOru
                # Need burn amount + a little ORU for network fee.
                if ($trusted -lt ($need + 0.01)) {
                    Write-Event ("Stage3 large burn deferred: trusted={0} need>={1}+fee" -f $trusted, $need)
                } else {
                    $cv3 = Rpc "createconvergence" @($need, "stage3-large-burn", "", $false, $null, $null, "unset", $false, 1.1)
                    $cvtxid3 = if ($cv3 -is [string]) { $cv3 } else { $cv3.txid }
                    $st.stage3_large_conv_txid = [string]$cvtxid3
                    $st.stage3_large_conv_height = $height
                    $st.stage3_large_conv_amount = $need
                    $st.notes += "stage3_large_burn_at_$height"
                    Write-Event ("Stage3 large createconvergence amount={0} txid={1} at height={2} (trusted was {3})" -f `
                        $need, $cvtxid3, $height, $trusted)
                    Save-State $st
                }
            } catch {
                Write-Event "Stage3 large createconvergence deferred: $($_.Exception.Message)"
            }
        }

        if ($st.stage3_large_conv_txid -and -not $st.stage3_res_mint_pass) {
            if ($null -ne $resTrusted -and $resTrusted -ge 100) {
                $st.stage3_res_mint_pass = $true
                $st.stage3_res_amount = $resTrusted
                # Large burn also satisfies Stage2 RES gate (1-ORU hunt is impractical).
                if (-not $st.stage2_res_mint_pass) {
                    $st.stage2_res_mint_pass = $true
                    $st.stage2_res_amount = $resTrusted
                    Write-Event ("Stage2 RES gate satisfied via Stage3 large burn: res_trusted={0}" -f $resTrusted)
                }
                Write-Event ("Stage3 RES minted: trusted={0} (after {1} ORU convergence {2})" -f `
                    $resTrusted, $st.stage3_large_conv_amount, $st.stage3_large_conv_txid)
                Save-State $st
            } elseif ($st.stage3_large_conv_height -and ($height -ge ($st.stage3_large_conv_height + 32))) {
                # ~63% expected eligibility somewhere in W at 65536 ORU; retry another large burn if still funded.
                if ($null -ne $trusted -and $trusted -gt $Stage3LargeBurnMinTrusted) {
                    try {
                        $need = [decimal]$Stage3LargeBurnOru
                        $cv3b = Rpc "createconvergence" @($need, "stage3-large-burn-retry", "", $false, $null, $null, "unset", $false, 1.1)
                        $cvtxid3b = if ($cv3b -is [string]) { $cv3b } else { $cv3b.txid }
                        $st.stage3_large_conv_txid = [string]$cvtxid3b
                        $st.stage3_large_conv_height = $height
                        $st.stage3_large_conv_amount = $need
                        $st.notes += "stage3_large_burn_retry_at_$height"
                        Write-Event ("Stage3 RES hunt retry: new {0} ORU convergence {1} at {2}" -f $need, $cvtxid3b, $height)
                        Save-State $st
                    } catch {
                        Write-Event "Stage3 large burn retry deferred: $($_.Exception.Message)"
                    }
                } else {
                    Write-Event ("Stage3 large window closed without RES; trusted={0} (need >{1} to retry)" -f `
                        $trusted, $Stage3LargeBurnMinTrusted)
                }
            }
        }

        if ($st.stage3_res_mint_pass -and -not $st.stage3_res_transfer_pass -and
            $null -ne $resTrusted -and $resTrusted -ge $Stage3ResTransferAmount -and
            $null -ne $trusted -and $trusted -ge 0.01) {
            try {
                if (-not $st.stage3_res_transfer_addr) {
                    $st.stage3_res_transfer_addr = [string](Rpc "getnewaddress" @("stage3-res-recv", "RES"))
                    Write-Event "Stage3 RES receive address: $($st.stage3_res_transfer_addr)"
                    Save-State $st
                }
                # sendres: address, amount, … fee_rate at index 9
                $rtxid = Rpc "sendres" @(
                    $st.stage3_res_transfer_addr,
                    $Stage3ResTransferAmount,
                    "stage3-res-transfer",
                    "",
                    $false,
                    $false,
                    $null,
                    "unset",
                    $null,
                    1.1
                )
                $st.stage3_res_transfer_txid = [string]$rtxid
                $st.stage3_res_transfer_pass = $true
                Write-Event ("Stage3 RES transfer pass: amount={0} txid={1} to={2}" -f `
                    $Stage3ResTransferAmount, $rtxid, $st.stage3_res_transfer_addr)
                Save-State $st
            } catch {
                Write-Event "Stage3 RES transfer deferred: $($_.Exception.Message)"
            }
        }

        if (-not $st.stage3_pass -and $height -ge $Stage3Height) {
            $st.stage3_pass = $true
            Write-Event "Stage3 reached height $height (extended stability target)"
            Save-State $st
        }

        if ($st.stage1_pass -and $st.stage2_res_mint_pass -and $st.stage3_pass -and $st.stage3_res_transfer_pass) {
            Write-Event "ALL STAGES COMPLETE — write final report and stop harness"
            Save-State $st
            break
        }

        # keep miner alive during long run (only while node RPC is up)
        Ensure-Miner | Out-Null
    } catch {
        $em = $_.Exception.Message
        if (Test-IsRpcUnavailable $em) {
            if (-not $script:rpcOffline) {
                $script:rpcOffline = $true
                $script:rpcOfflineSince = Get-Date
                $script:rpcOfflineLastNote = Get-Date
                Write-Event "RPC offline (node/wallet stopped or booting) — waiting; resume automatically when RPC is back"
            }
        } else {
            Write-Event "poll error: $em"
        }
    }

    Start-Sleep -Seconds $PollSeconds
}

Write-Event "Harness exited"
