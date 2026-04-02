param([string]$changelog)

$manifestPath = Join-Path $PSScriptRoot "manifest.json"
$changelogPath = Join-Path $PSScriptRoot "CHANGELOG.json"

if (-not (Test-Path $manifestPath)) {
    Write-Error "manifest.json not found"
    exit 1
}

$content = Get-Content $manifestPath -Raw -Encoding UTF8
$pattern = '"name":\s*"HMK v(\d+)"'
$match = [regex]::Match($content, $pattern)

if ($match.Success) {
    $currentVersion = [int]$match.Groups[1].Value
    $newVersion = $currentVersion + 1
    
    $newContent = $content -replace $pattern, "`"name`": `"HMK v$newVersion`""
    [System.IO.File]::WriteAllText($manifestPath, $newContent, [System.Text.Encoding]::UTF8)
    
    if (-not $changelog) {
        $changelog = Read-Host "输入版本 $newVersion 的变更内容"
    }
    
    if (Test-Path $changelogPath) {
        $cl = Get-Content $changelogPath -Raw -Encoding UTF8 | ConvertFrom-Json
    } else {
        $cl = @{}
    }
    
    $cl | Add-Member -NotePropertyName "$newVersion" -NotePropertyValue $changelog -Force
    $cl | ConvertTo-Json | Set-Content $changelogPath -Encoding UTF8
    
    Write-Host "OK: v$currentVersion -> v$newVersion"
    Write-Host "Note: $changelog"
    exit 0
} else {
    Write-Error "Cannot find version"
    exit 1
}


