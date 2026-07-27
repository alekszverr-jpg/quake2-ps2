param(
    [Parameter(Position=0)]
    [string]$SourcePath = '',

    [Parameter(Position=1)]
    [string]$TargetPath = ''
)

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $SourcePath) {
    $SourcePath = Join-Path $repoRoot 'Quake2Game\BASEQ2'
} elseif (-not [System.IO.Path]::IsPathRooted($SourcePath)) {
    $SourcePath = Join-Path $repoRoot $SourcePath
}

if (-not $TargetPath) {
    $TargetPath = Join-Path $repoRoot 'baseq2'
} elseif (-not [System.IO.Path]::IsPathRooted($TargetPath)) {
    $TargetPath = Join-Path $repoRoot $TargetPath
}

if (-not (Test-Path $SourcePath -PathType Container)) {
    Write-Host "Source game data directory not found: $SourcePath"
    exit 1
}

if (-not (Test-Path $TargetPath)) {
    New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
}

Write-Host "Copying Quake II game data from: $SourcePath"
Write-Host "Destination: $TargetPath"

$entries = Get-ChildItem -Path $SourcePath -Force -ErrorAction Stop
if ($entries.Count -eq 0) {
    Write-Host "No game files were found in the source directory."
    exit 1
}

foreach ($entry in $entries) {
    $dst = Join-Path $TargetPath $entry.Name
    Copy-Item $entry.FullName $dst -Recurse -Force -ErrorAction Stop
    Write-Host "Copied: $entry.Name"
}

Write-Host "Quake II game data is ready in: $TargetPath"
