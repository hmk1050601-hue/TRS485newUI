$path='\\192.168.24.31\Public data\uploads\20260326_v11_newUIv1308\index.html'
$text = Get-Content -Path $path -Raw -Encoding UTF8
$old = '<img src="./android%20logo.png" alt="Android 圖示">'
$new = '<img src="./android-mascot.svg" alt="Android 圖示">'
if ($text -like "*$old*") {
  $text = $text.Replace($old,$new)
  Set-Content -Path $path -Value $text -Encoding UTF8
  Write-Output "Replaced"
} else {
  Write-Output "Pattern not found"
}
$script='\\192.168.24.31\Public data\uploads\20260326_v11_newUIv1308\scripts\replace-android-image.ps1'
if(Test-Path $script){ Remove-Item -Force $script; Write-Output "Removed old script" }
