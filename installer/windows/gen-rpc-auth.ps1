param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Datadir
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $Datadir)) {
    New-Item -ItemType Directory -Path $Datadir | Out-Null
}

function Write-Sidecar([string]$user, [string]$pass) {
    $ascii = [Text.Encoding]::ASCII
    [IO.File]::WriteAllText((Join-Path $Datadir "rpc-user.txt"), $user, $ascii)
    [IO.File]::WriteAllText((Join-Path $Datadir "rpc-pass.txt"), $pass, $ascii)
    $conf = Join-Path $Datadir "rpc-auth.conf"
    $lines = @(
        "# Generated RPC credentials. Do not share.",
        "rpcuser=$user",
        "rpcpassword=$pass"
    )
    [IO.File]::WriteAllLines($conf, $lines, $ascii)
}

function Try-ParseConf([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    $user = $null
    $pass = $null
    foreach ($line in (Get-Content -LiteralPath $path -ErrorAction SilentlyContinue)) {
        $t = [string]$line
        if ($t.StartsWith("rpcuser=")) { $user = $t.Substring(8).Trim() }
        if ($t.StartsWith("rpcpassword=")) { $pass = $t.Substring(12).Trim() }
    }
    if ($user -and $pass) { return @{ User = $user; Pass = $pass } }
    return $null
}

$existing = Try-ParseConf (Join-Path $Datadir "rpc-auth.conf")
if ($existing -eq $null) {
    $bytes = New-Object byte[] 40
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    $hex = ([BitConverter]::ToString($bytes) -replace "-", "").ToLowerInvariant()
    $existing = @{
        User = "rigor_" + $hex.Substring(0, 16)
        Pass = $hex.Substring(16)
    }
    Write-Output "generated $(Join-Path $Datadir 'rpc-auth.conf')"
}
Write-Sidecar $existing.User $existing.Pass
exit 0
