param(
    [int]$Max = 3,
    [int]$IntervalMs = 750,
    [int]$Seconds = 0,
    [string]$LogPath = '',
    [switch]$Once
)

$ErrorActionPreference = 'Continue'
$stopScript = Join-Path $PSScriptRoot 'stop-compiler-spam.ps1'
if (-not (Test-Path -LiteralPath $stopScript)) {
    throw "Missing $stopScript"
}
if ($Max -lt 1) { $Max = 1 }

$names = @('ninja', 'cc1plus', 'cc1', 'c++', 'g++', 'collect2')
function Get-CompilerCount {
    return @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -in $names }).Count
}
function Write-Guard([string]$msg) {
    Write-Output $msg
    if ($LogPath) {
        Add-Content -LiteralPath $LogPath -Value $msg -ErrorAction SilentlyContinue
    }
}

if ($LogPath) {
    $dir = Split-Path -Parent $LogPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }
    Set-Content -LiteralPath $LogPath -Value ("guard start utc={0:o} pid={1}" -f (Get-Date).ToUniversalTime(), $PID)
}

$deadline = if ($Seconds -gt 0) { (Get-Date).AddSeconds($Seconds) } else { [datetime]::MaxValue }
Write-Guard ("guard on: Max={0} IntervalMs={1} stop={2}" -f $Max, $IntervalMs, $stopScript)

do {
    $count = Get-CompilerCount
    if ($count -gt $Max) {
        $summary = (
            Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -in $names } |
            Group-Object ProcessName |
            ForEach-Object { '{0}={1}' -f $_.Name, $_.Count }
        ) -join ', '
        Write-Guard ("{0:HH:mm:ss} STORM count={1} ({2}) - stopping" -f (Get-Date), $count, $summary)
        & $stopScript -Watch 2 | ForEach-Object { Write-Guard $_ }
    } elseif ($Once) {
        Write-Guard ("ok count={0}" -f $count)
        break
    }
    if ($Once) { break }
    Start-Sleep -Milliseconds $IntervalMs
} while ((Get-Date) -lt $deadline)

Write-Guard ("guard exit remaining={0}" -f (Get-CompilerCount))
