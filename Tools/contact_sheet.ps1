# Builds a labelled contact sheet JPG from the PNGs in a folder (file name in the corner of each cell).
# Usage: powershell -ExecutionPolicy Bypass -File Tools\contact_sheet.ps1 -Src <folder> -Filter "*.png" -Out <file.jpg> [-Cols 3]
param([string]$Src, [string]$Filter, [string]$Out, [int]$Cols = 3)
Add-Type -AssemblyName System.Drawing
$files = Get-ChildItem $Src -Filter $Filter | Sort-Object Name
$w = 640; $h = 360; $rows = [math]::Ceiling($files.Count / $Cols)
$sheet = New-Object System.Drawing.Bitmap ($Cols * $w), ($rows * $h)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$font = New-Object System.Drawing.Font "Segoe UI", 12
$i = 0
foreach ($f in $files) {
    $img = [System.Drawing.Image]::FromFile($f.FullName)
    $x = ($i % $Cols) * $w; $y = [math]::Floor($i / $Cols) * $h
    $g.DrawImage($img, $x, $y, $w, $h)
    $g.DrawString($f.BaseName, $font, [System.Drawing.Brushes]::Yellow, $x + 4, $y + 4)
    $img.Dispose(); $i++
}
$sheet.Save($Out, [System.Drawing.Imaging.ImageFormat]::Jpeg)
