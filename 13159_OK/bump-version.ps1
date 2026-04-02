param([string]$changelog)

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$manifestPath  = Join-Path $root 'manifest.json'
$changelogPath = Join-Path $root 'CHANGELOG.json'
$versionJsPath = Join-Path $root 'version.js'
$indexPath = Join-Path $root 'index.html'
$latestCommitPath = Join-Path $root '.latest_commit_msg.txt'

function Get-FirstMeaningfulLine([string]$text) {
    if (-not $text) { return '' }
    $lines = ($text -split "`r?`n") | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
    if ($lines.Count -gt 0) { return $lines[0] }
    return ''
}

function Normalize-Changelog([string]$raw) {
    $x = Get-FirstMeaningfulLine $raw
    if (-not $x) { return '' }

    if ($x -match '^\$\(cat\s+\.latest_commit_msg\.txt\)$') {
        return ''
    }

    $m = [regex]::Match($x, "\$msg\s*=\s*'([^']+)'")
    if ($m.Success) {
        return $m.Groups[1].Value.Trim()
    }

    return $x
}

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

$changelog = Normalize-Changelog $changelog
if (-not $changelog -and (Test-Path $latestCommitPath)) {
    try {
        $rawCommit = Get-Content $latestCommitPath -Raw
        $changelog = Normalize-Changelog $rawCommit
    } catch {}
}

if (-not $changelog) {
    $changelog = '自動版更與備註更新'
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
        if ((-not $inserted) -and ($ln -match ('^  // v' + $old + ':'))) {
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

if (Test-Path $indexPath) {
    $idx = Get-Content $indexPath -Raw -Encoding UTF8
    $idx = [regex]::Replace($idx, '(<link\s+rel="manifest"\s+href="\./manifest\.json\?v=)\d+("\s*>)', ('$1' + $new + '$2'))
    $idx = [regex]::Replace($idx, "(const\s+ROOT_BUILD\s*=\s*')\d+(';)", ('$1' + $new + '$2'))
    [System.IO.File]::WriteAllText($indexPath, $idx, [System.Text.Encoding]::UTF8)
    Write-Host 'index.html: version hooks OK'
} else {
    Write-Warning 'index.html not found'
}

# ── 同步 sw.js 版本戳（觸發瀏覽器偵測 SW 變更並重新安裝）──
$swPath = Join-Path $root 'sw.js'
if (Test-Path $swPath) {
    $sw = Get-Content $swPath -Raw -Encoding UTF8
    $sw = [regex]::Replace($sw, "(const\s+SW_VERSION\s*=\s*')\d+(';)", ('$1' + $new + '$2'))
    $sw = [regex]::Replace($sw, "(// SW_VERSION=)\d+", ('$1' + $new))
    [System.IO.File]::WriteAllText($swPath, $sw, [System.Text.Encoding]::UTF8)
    Write-Host 'sw.js: version stamp OK'
} else {
    Write-Warning 'sw.js not found'
}

# ── 同步子頁面 version.js 引用版本 ──
foreach ($subPage in @('remote.html','schedule.html','status.html','powerlog.html','user.html','setup.html')) {
    $subPath = Join-Path $root $subPage
    if (Test-Path $subPath) {
        $sub = Get-Content $subPath -Raw -Encoding UTF8
        $sub = [regex]::Replace($sub, '(version\.js\?v=)\d+', ('$1' + $new))
        [System.IO.File]::WriteAllText($subPath, $sub, [System.Text.Encoding]::UTF8)
    }
}
Write-Host 'subpages: version.js refs OK'

Write-Host ''
Write-Host "Done: v$old -> v$new"
