param(
    [int]$Jobs = 1,
    [int]$WaitMinutes = 15,
    [string]$BuildDirectory = 'build',
    [string[]]$Command,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)
if (-not $Command -or $Command.Count -eq 0) { $Command = $Rest }

$ErrorActionPreference = 'Stop'
if ($Jobs -lt 1) { $Jobs = 1 }
if ($Jobs -gt 2) {
    Write-Warning "Capping -Jobs from $Jobs to 2 (Bridge multi-agent safety). Pass after clearing peers if you truly need more."
    $Jobs = 2
}

$project = Split-Path -Parent $PSScriptRoot
Set-Location $project

$busy = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -in @('ninja', 'cc1plus', 'cc1', 'c++', 'g++', 'collect2')
})
if ($busy.Count -gt 0) {
    $summary = ($busy | Group-Object ProcessName | ForEach-Object { '{0}={1}' -f $_.Name, $_.Count }) -join ', '
    throw "Compile already running ($summary). Wait, or run .\scripts\stop-compiler-spam.ps1, then retry through this wrapper."
}

if (-not $Command -or $Command.Count -eq 0) {
    throw 'Pass a command after -- e.g. .\scripts\with-build-lock.ps1 -- ninja -C build -j1 Vector'
}
if ($Command[0] -eq '--') {
    if ($Command.Count -lt 2) { throw 'Missing command after --' }
    $Command = $Command[1..($Command.Count - 1)]
}

if (-not (Test-Path -LiteralPath $BuildDirectory)) {
    New-Item -ItemType Directory -Path $BuildDirectory | Out-Null
}
$lockPath = Join-Path $BuildDirectory '.vector-build.lock'
$lockStream = $null
$deadline = (Get-Date).AddMinutes($WaitMinutes)
while ($null -eq $lockStream) {
    try {
        $lockStream = [System.IO.File]::Open(
            $lockPath,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None
        )
        $who = "{0} pid={1} utc={2:o} cmd={3}`n" -f $env:COMPUTERNAME, $PID, (Get-Date).ToUniversalTime(), ($Command -join ' ')
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($who)
        $lockStream.SetLength(0)
        $lockStream.Write($bytes, 0, $bytes.Length)
        $lockStream.Flush()
    } catch {
        if ((Get-Date) -ge $deadline) {
            throw "Timed out waiting for build lock at $lockPath. Run .\scripts\stop-compiler-spam.ps1 then retry."
        }
        Write-Output 'waiting for build lock...'
        Start-Sleep -Seconds 2
    }
}

try {
    $env:CMAKE_BUILD_PARALLEL_LEVEL = "$Jobs"
    Write-Output ("locked build; running: {0}" -f ($Command -join ' '))
    & $Command[0] @($Command | Select-Object -Skip 1)
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    if ($null -ne $lockStream) {
        $lockStream.Close()
        $lockStream.Dispose()
    }
}
