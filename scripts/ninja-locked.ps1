param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Targets,
    [string]$BuildDirectory = 'build',
    [int]$Jobs = 1
)
$ErrorActionPreference = 'Stop'
if (-not $Targets -or $Targets.Count -eq 0) {
    throw 'Pass at least one ninja target, e.g. .\scripts\ninja-locked.ps1 vector-tests'
}
if ($Jobs -gt 2) {
    Write-Output "capping -j from $Jobs to 2 (Bridge anti-storm)"
    $Jobs = 2
}
$wrapper = Join-Path $PSScriptRoot 'with-build-lock.ps1'
$cmd = @('ninja', '-C', $BuildDirectory, '-j', "$Jobs") + @($Targets)
& $wrapper -Jobs $Jobs -BuildDirectory $BuildDirectory -Command $cmd
exit $LASTEXITCODE
