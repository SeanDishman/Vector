param([string]$BuildDirectory = 'build', [int]$Jobs = 2)
$ErrorActionPreference = 'Stop'
if ($Jobs -gt 2) {
    Write-Warning "Capping -Jobs from $Jobs to 2 for Bridge multi-agent safety."
    $Jobs = 2
}
$project = Split-Path -Parent $PSScriptRoot
Push-Location $project
$lockStream = $null
try {
    $busy = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -in @('ninja', 'cc1plus', 'cc1', 'c++', 'g++', 'collect2')
    })
    if ($busy.Count -gt 0) {
        $summary = ($busy | Group-Object ProcessName | ForEach-Object { '{0}={1}' -f $_.Name, $_.Count }) -join ', '
        throw "Compile already running ($summary). Run .\scripts\stop-compiler-spam.ps1, wait for peers, then retry."
    }

    if (-not (Test-Path -LiteralPath $BuildDirectory)) {
        New-Item -ItemType Directory -Path $BuildDirectory | Out-Null
    }
    $lockPath = Join-Path $BuildDirectory '.vector-build.lock'
    $deadline = (Get-Date).AddMinutes(15)
    while ($null -eq $lockStream) {
        try {
            $lockStream = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
            $who = "{0} pid={1} utc={2:o} build.ps1`n" -f $env:COMPUTERNAME, $PID, (Get-Date).ToUniversalTime()
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($who)
            $lockStream.SetLength(0)
            $lockStream.Write($bytes, 0, $bytes.Length)
            $lockStream.Flush()
        } catch {
            if ((Get-Date) -ge $deadline) {
                throw "Timed out waiting for build lock at $lockPath. Another agent is compiling, or a dead lock remains. Run .\scripts\stop-compiler-spam.ps1 then retry."
            }
            Write-Output "waiting for build lock..."
            Start-Sleep -Seconds 2
        }
    }

    $configure = @('-S', '.', '-B', $BuildDirectory, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DVECTOR_BUILD_GUI=ON')
    $bundles = @('C:/Program Files/Git/usr/ssl/certs/ca-bundle.crt', 'C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt')
    foreach ($bundle in $bundles) {
        if (Test-Path -LiteralPath $bundle) { $configure += "-DCMAKE_TLS_CAINFO=$bundle"; break }
    }
    & cmake @configure
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    $env:CMAKE_BUILD_PARALLEL_LEVEL = "$Jobs"
    & cmake --build $BuildDirectory -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    & ctest --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    & (Join-Path $PSScriptRoot 'package.ps1') -BuildDirectory $BuildDirectory
} finally {
    if ($null -ne $lockStream) {
        $lockStream.Close()
        $lockStream.Dispose()
    }
    Pop-Location
}
