param([string]$BuildDirectory = 'build', [string]$DependencyBuildDirectory = '', [string]$DestinationDirectory = 'dist/Vector')
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$build = Join-Path $project $BuildDirectory
$destination = Join-Path $project $DestinationDirectory
$dependencyBuild = if ($DependencyBuildDirectory) { Join-Path $project $DependencyBuildDirectory } else { $build }
$packagedExe = [System.IO.Path]::GetFullPath((Join-Path $destination 'Vector.exe'))
$running = @(Get-Process -Name Vector -ErrorAction SilentlyContinue | Where-Object { $_.Path -and [System.IO.Path]::GetFullPath($_.Path) -eq $packagedExe })
if ($running.Count -gt 0) { throw 'Close the running packaged Vector app before packaging, then run this script again.' }
New-Item -ItemType Directory -Force -Path $destination, (Join-Path $destination 'licenses'), (Join-Path $destination 'docs') | Out-Null
foreach ($exe in @('Vector.exe', 'vector-cli.exe')) {
    Copy-Item -LiteralPath (Join-Path $build "bin/$exe") -Destination $destination
}
Copy-Item -LiteralPath (Join-Path $project 'README.md') -Destination $destination
Copy-Item -LiteralPath (Join-Path $project 'docs/vector-desktop.png') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/report-export.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/proxy-settings.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/dns-infrastructure.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/frontend-evidence.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/web-signatures.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/research.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/verification.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/ui-review.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/vendors.md') -Destination (Join-Path $destination 'docs')
Copy-Item -LiteralPath (Join-Path $project 'docs/vendor-verification.md') -Destination (Join-Path $destination 'docs')
if (Test-Path -LiteralPath (Join-Path $project 'docs/vendor-ui-review.md')) {
    Copy-Item -LiteralPath (Join-Path $project 'docs/vendor-ui-review.md') -Destination (Join-Path $destination 'docs')
}
if (Test-Path -LiteralPath (Join-Path $project 'docs/vendor-design-verification.md')) {
    Copy-Item -LiteralPath (Join-Path $project 'docs/vendor-design-verification.md') -Destination (Join-Path $destination 'docs')
}
foreach ($document in @('interface-redesign.md', 'interface-redesign-review.md', 'inspection-activity.md', 'inspection-activity-design-check.md', 'library-identification.md', 'library-verification.md', 'browser-runtime.md', 'browser-verification.md', 'web-evidence-cap.md', 'detection-coverage.md', 'stackfull-research.md', 'published-providers.md', 'published-providers-verification.md', 'x-inspection-diagnosis.md', 'claude-ai-gap.md', 'stackfull-parity.md', 'tag-evidence.md', 'ten-site-validation.md', 'api-endpoints.md', 'public-release.md')) {
    $source = Join-Path $project "docs/$document"
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $destination 'docs') }
}
$licenses = @{
    'SDL.txt' = '_deps/sdl3-src/LICENSE.txt'
    'Dear-ImGui.txt' = '_deps/imgui-src/LICENSE.txt'
    'curl.txt' = '_deps/curl-src/COPYING'
    'nlohmann-json.txt' = '_deps/json-src/LICENSE.MIT'
}
foreach ($entry in $licenses.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $dependencyBuild $entry.Value) -Destination (Join-Path $destination "licenses/$($entry.Key)")
}
foreach ($file in @('Simple-Icons-LICENSE.md','Simple-Icons-DISCLAIMER.md','Devicon-LICENSE','SVG-Logos-LICENSE.txt','manifest.json')) {
    Copy-Item -LiteralPath (Join-Path $project "src/assets/technology/$file") -Destination (Join-Path $destination "licenses/technology-$file")
}
Copy-Item -LiteralPath (Join-Path $project 'src/third_party/nanosvg/LICENSE.txt') -Destination (Join-Path $destination 'licenses/NanoSVG.txt')
foreach ($notice in Get-ChildItem -LiteralPath (Join-Path $project 'docs/licenses') -Filter '*.txt' -File) {
    Copy-Item -LiteralPath $notice.FullName -Destination (Join-Path $destination 'licenses')
}
Copy-Item -LiteralPath (Join-Path $project 'docs/technology-icons.md') -Destination (Join-Path $destination 'docs')
$notices = @('Vector third-party notices', '', 'The executable embeds the dependencies and technology icons covered by the notices below.', '')
foreach ($notice in Get-ChildItem -LiteralPath (Join-Path $destination 'licenses') -File | Sort-Object Name) {
    $notices += @(("===== {0} =====" -f $notice.Name), '', (Get-Content -LiteralPath $notice.FullName -Raw -Encoding UTF8), '')
}
$notices | Set-Content -LiteralPath (Join-Path $destination 'THIRD-PARTY-NOTICES.txt') -Encoding UTF8
Write-Output "Packaged Vector in $destination"
