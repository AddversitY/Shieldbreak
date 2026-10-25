# Launches the game, then clicks / presses keys / screenshots the screen on a timed script (for menus the self-test
# cannot reach). Crop with -CropW/-CropH (fractions of the screen) so only the game window is captured.
# Example: -Exe "<UE>\Engine\Binaries\Win64\UnrealEditor.exe" -GameArgs "<proj> -game -windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=0"
#   -OutDir out -CropW 0.8889 -CropH 0.75 -Steps "wait:25;click:0.067,0.607;wait:2;shot:settings"
#   (on this laptop the 1280x720 window at 0,0 covers 0.8889 x 0.75 of the 1440x960 desktop)
param([string]$Exe, [string]$GameArgs = "", [string]$OutDir, [string]$Steps, [double]$CropW = 1.0, [double]$CropH = 1.0)
# Steps: ";"-separated: "wait:N", "shot:name", "click:X,Y" (fractions 0-1 of the screen), "key:{ESC}"
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class U {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
[U]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$p = Start-Process -FilePath $Exe -ArgumentList $GameArgs -PassThru
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
foreach ($s in $Steps.Split(";")) {
  $k, $v = $s.Split(":", 2)
  switch ($k) {
    "wait" { Start-Sleep -Seconds ([double]$v) }
    "shot" {
      $cw = [int]($b.Width * $CropW); $ch = [int]($b.Height * $CropH); $bmp = New-Object System.Drawing.Bitmap $cw, $ch
      $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, (New-Object System.Drawing.Size $cw, $ch))
      $bmp.Save((Join-Path $OutDir "$v.png"), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
    }
    "click" {
      $x, $y = $v.Split(","); [U]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
      [U]::SetCursorPos([int]([double]$x * $b.Width), [int]([double]$y * $b.Height)) | Out-Null
      Start-Sleep -Milliseconds 150; [U]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 80; [U]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
    }
    "key" { [U]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; [System.Windows.Forms.SendKeys]::SendWait($v) }
  }
}
if (-not $p.HasExited) { $p.Kill() }
"screen $($b.Width)x$($b.Height)"
