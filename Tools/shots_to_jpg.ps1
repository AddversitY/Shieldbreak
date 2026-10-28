# Converts Saved\Screenshots\Shots\*.png to JPG in an output folder (default docs\playtests\<today>_visual_pass) and
# builds contact_sheet_before_after.jpg: one row per map view, "before" on the left, "after" on the right.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\shots_to_jpg.ps1 [-Out <folder>] [-Quality 85]
param([string]$Out = "", [int]$Quality = 85)
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root "Saved\Screenshots\Shots"
if (-not $Out) { $Out = Join-Path $root ("docs\playtests\" + (Get-Date -Format "yyyy-MM-dd") + "_visual_pass") }
New-Item -ItemType Directory -Force $Out | Out-Null
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
function Save-Jpg($image, $path) {
    $params = New-Object System.Drawing.Imaging.EncoderParameters 1
    $params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)
    $image.Save($path, $codec, $params)
}
foreach ($png in Get-ChildItem $src -Filter *.png) {
    $image = [System.Drawing.Image]::FromFile($png.FullName)
    Save-Jpg $image (Join-Path $Out ($png.BaseName + ".jpg"))
    $image.Dispose()
}
# Contact sheet: pairs <map>_before_<view> / <map>_after_<view>, scaled to 480 px wide each.
$pairs = Get-ChildItem $src -Filter *_after_*.png | Sort-Object Name | Where-Object { Test-Path ($_.FullName -replace "_after_", "_before_") }
if ($pairs.Count -gt 0) {
    $w = 480; $h = 270; $label = 22
    $sheet = New-Object System.Drawing.Bitmap (2 * $w), ($pairs.Count * ($h + $label))
    $g = [System.Drawing.Graphics]::FromImage($sheet)
    $g.Clear([System.Drawing.Color]::Black)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $font = New-Object System.Drawing.Font "Segoe UI", 10
    $y = 0
    foreach ($after in $pairs) {
        $before = $after.FullName -replace "_after_", "_before_"
        $name = $after.BaseName -replace "_after_", " "
        $g.DrawString("$name   before | after", $font, [System.Drawing.Brushes]::White, 4, $y + 2)
        $i = 0
        foreach ($file in @($before, $after.FullName)) {
            $image = [System.Drawing.Image]::FromFile($file)
            $g.DrawImage($image, $i * $w, $y + $label, $w, $h)
            $image.Dispose()
            $i++
        }
        $y += $h + $label
    }
    Save-Jpg $sheet (Join-Path $Out "contact_sheet_before_after.jpg")
    $g.Dispose(); $sheet.Dispose()
}
Get-ChildItem $Out -Filter *.jpg | Select-Object Name, Length
