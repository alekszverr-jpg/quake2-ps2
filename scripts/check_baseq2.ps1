param()

$repoRoot = Split-Path -Parent $PSScriptRoot
$baseq2Dir = Join-Path $repoRoot 'baseq2'
$probeFile = Join-Path $baseq2Dir 'pak0.pak'

if (Test-Path $probeFile) {
    Write-Host "Found Quake II game data at: $probeFile"
    Write-Host 'The port should be able to use this directory as baseq2.'
    exit 0
}

Write-Host 'Quake II game data was not found.'
Write-Host ''
Write-Host 'Expected layout:'
Write-Host '  baseq2/pak0.pak'
Write-Host '  baseq2/pak1.pak (optional)'
Write-Host '  baseq2/video/'
Write-Host '  baseq2/players/'
Write-Host ''
Write-Host 'Place your original Quake II data files in:'
Write-Host "  $baseq2Dir"
exit 1
