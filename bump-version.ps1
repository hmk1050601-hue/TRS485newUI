param([string]$changelog)

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$manifestPath  = Join-Path $root 'manifest.json'
$changelogPath = Join-Path $root 'CHANGELOG.json'
$versionJsPath = Join-Path $root 'version.js'

if (-not (Test-Path $manifestPath)) {
    Write-Error 'manifest.json not found'
    exit 1
}

$content = Get-Content $manifestPath -Raw -Encoding UTF8
$pat = '"name":\s*"HMK v(\d+)"'
$m = [regex]::Match($content, $pat)

if (-not $m.Success) {
    Write-Error 'Cannot find version in manifest.json'
    exit 1
}

$old = [int]$m.Groups[1].Value
$new = $old + 1

if (-not $changelog) {
    $stdinText = [Console]::In.ReadToEnd()
    if ($stdinText) {
        $changelog = $stdinText.Trim()
    }
}

if (-not $changelog) {
    $changelog = Read-Host 'Enter changelog'
}
if (-not $changelog) {
    $changelog = 'no description'
}

$timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
$changelogWithTime = "$timestamp | $changelog"

$rep = '"name": "HMK v' + $new + '"'
$newContent = $content -replace $pat, $rep
[System.IO.File]::WriteAllText($manifestPath, $newContent, [System.Text.Encoding]::UTF8)
Write-Host "manifest.json: v$old -> v$new"

if (Test-Path $changelogPath) {
    $cl = Get-Content $changelogPath -Raw -Encoding UTF8 | ConvertFrom-Json
} else {
    $cl = [PSCustomObject]@{}
}
$cl | Add-Member -NotePropertyName "$new" -NotePropertyValue $changelogWithTime -Force
$cl | ConvertTo-Json | Set-Content $changelogPath -Encoding UTF8
Write-Host 'CHANGELOG.json: OK'

if (Test-Path $versionJsPath) {
    $lines = [System.IO.File]::ReadAllLines($versionJsPath, [System.Text.Encoding]::UTF8)
    $out = [System.Collections.Generic.List[string]]::new()
    $inserted = $false
    foreach ($ln in $lines) {
        if ((-not $inserted) -and ($ln -match '^  // v' + $old + ':')) {
            $out.Add('  // v' + $new + ': ' + $changelogWithTime)
            $inserted = $true
        }
        $out.Add($ln)
    }
    $txt = $out -join "`r`n"
    $txt = $txt.Replace("'$old'", "'$new'")
    [System.IO.File]::WriteAllText($versionJsPath, $txt, [System.Text.Encoding]::UTF8)
    Write-Host 'version.js: OK'
} else {
    Write-Warning 'version.js not found'
}

Write-Host ''
Write-Host "Done: v$old -> v$new"


