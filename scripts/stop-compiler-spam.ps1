param(
    [switch]$WhatIf,
    [int]$Watch = 0
)

$ErrorActionPreference = 'Continue'
$names = @(
    'ninja',
    'cmake',
    'cc1plus',
    'cc1',
    'c++',
    'g++',
    'collect2',
    'ld'
)

function Stop-CompilerSpam {
    $killed = [System.Collections.Generic.List[string]]::new()
    foreach ($name in @('ninja', 'cmake')) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
            if ($WhatIf) {
                Write-Output "WHATIF stop $($_.ProcessName) pid=$($_.Id)"
            } else {
                Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
                $killed.Add("$($_.ProcessName):$($_.Id)")
            }
        }
    }
    Start-Sleep -Milliseconds 200
    foreach ($name in $names) {
        if ($name -in @('ninja', 'cmake')) { continue }
        Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
            $path = ''
            try { $path = $_.Path } catch {}
            $ok = $false
            if ($path) {
                $ok = $path -match '(?i)\\mingw|\\winlibs|\\msys|\\gcc|\\bin\\(cc1plus|cc1|c\+\+|g\+\+|collect2|ld)\.exe$'
            } else {
                $ok = $true
            }
            if (-not $ok) { return }
            if ($WhatIf) {
                Write-Output "WHATIF stop $($_.ProcessName) pid=$($_.Id) $path"
            } else {
                Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
                $killed.Add("$($_.ProcessName):$($_.Id)")
            }
        }
    }
    return $killed
}

$deadline = if ($Watch -gt 0) { (Get-Date).AddSeconds($Watch) } else { Get-Date }
do {
    $batch = Stop-CompilerSpam
    if ($batch.Count -gt 0) {
        Write-Output ("killed={0} {1}" -f $batch.Count, ($batch -join ', '))
    } else {
        Write-Output 'cleared'
    }
    if ($Watch -le 0) { break }
    Start-Sleep -Milliseconds 400
} while ((Get-Date) -lt $deadline)

$left = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -in $names })
Write-Output ("remaining={0}" -f $left.Count)
if ($left.Count -gt 0) {
    $left | Group-Object ProcessName | ForEach-Object { Write-Output ("  {0}={1}" -f $_.Name, $_.Count) }
}
