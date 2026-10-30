$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Downloads the free Standard edition of Quaternius' Modular Sci-Fi MegaKit (CC0) from itch.io into Saved\Quaternius\kit,
# for Art/blender/build_scifi_kit.py and Tools/import_scifi_kit.py. Log: Saved\Logs\Q-download.log, file list Q-files.log.
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'Saved\Quaternius'
$log = Join-Path $root 'Saved\Logs\Q-download.log'
New-Item -ItemType Directory -Force -Path $out | Out-Null
function Log($m) { Add-Content -Path $log -Value $m }
Set-Content -Path $log -Value "start $(Get-Date -Format o)"
try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  $base = 'https://quaternius.itch.io/modular-sci-fi-megakit'
  $s = New-Object Microsoft.PowerShell.Commands.WebRequestSession
  $p = Invoke-WebRequest $base -WebSession $s -UseBasicParsing
  $csrf = [regex]::Match($p.Content, 'name="csrf_token" value="([^"]+)"').Groups[1].Value
  Log "page ok, csrf length $($csrf.Length)"
  $r = Invoke-RestMethod "$base/download_url" -Method Post -Body @{ csrf_token = $csrf } -WebSession $s
  Log "download page: $($r.url)"
  $d = Invoke-WebRequest $r.url -WebSession $s -UseBasicParsing
  Log "got download page $(Get-Date -Format o)"
  $csrf2 = [regex]::Match($d.Content, 'name="csrf_token" value="([^"]+)"').Groups[1].Value
  if (-not $csrf2) { $csrf2 = $csrf }
  $key = [regex]::Match($r.url, '/download/([^/?#]+)').Groups[1].Value
  $uploads = [regex]::Matches($d.Content, 'data-upload_id="(\d+)"[\s\S]{0,600}?class="upload_name"[\s\S]{0,200}?title="([^"]+)"')
  foreach ($u in $uploads) { Log "upload $($u.Groups[1].Value) $($u.Groups[2].Value)" }
  if ($uploads.Count -eq 0) {
    Log "no uploads matched; ids seen: $(([regex]::Matches($d.Content, 'data-upload_id=\"(\d+)\"') | ForEach-Object { $_.Groups[1].Value }) -join ',')"
    Set-Content -Path (Join-Path $out 'download_page.html') -Value $d.Content
    throw 'no uploads'
  }
  $pick = $uploads | Where-Object { $_.Groups[2].Value -match 'Standard' } | Select-Object -First 1
  if (-not $pick) { $pick = $uploads[0] }
  $id = $pick.Groups[1].Value
  $name = $pick.Groups[2].Value
  Log "picked $id $name"
  $keyDec = [uri]::UnescapeDataString($key)
  $f = $null
  $tries = @(
    @{ u = "$base/file/$($id)"; b = @{ csrf_token = $csrf2; source = 'game_download'; key = $keyDec } },
    @{ u = "$base/file/$($id)?source=game_download&key=$keyDec"; b = @{ csrf_token = $csrf2 } },
    @{ u = "$base/file/$($id)?source=game_download"; b = @{ csrf_token = $csrf2 } },
    @{ u = "$base/file/$($id)"; b = @{ csrf_token = $csrf2 } }
  )
  foreach ($t in $tries) {
    try {
      $resp = Invoke-WebRequest $t.u -Method Post -Body $t.b -WebSession $s -UseBasicParsing -Headers @{ 'X-Requested-With' = 'XMLHttpRequest'; 'Referer' = $r.url }
      $txt = $resp.Content
      Log "try $($t.u) [$(($t.b.Keys) -join ',')] -> $($txt.Substring(0, [Math]::Min(300, $txt.Length)))"
      $j = $txt | ConvertFrom-Json
      if ($j.url) { $f = $j; break }
    } catch { Log "try failed: $($_.Exception.Message)" }
  }
  if (-not $f) { Set-Content -Path (Join-Path $out 'download_page.html') -Value $d.Content; throw 'no file url' }
  Log "file url ok: True"
  $zip = Join-Path $out 'MegaKit-Standard.zip'
  Invoke-WebRequest $f.url -OutFile $zip -UseBasicParsing
  Log "downloaded $((Get-Item -LiteralPath $zip).Length) bytes to $zip"
  Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $out 'kit') -Force
  Get-ChildItem -Recurse (Join-Path $out 'kit') | Where-Object { -not $_.PSIsContainer } | ForEach-Object { $_.FullName.Substring($out.Length) + "`t" + $_.Length } | Set-Content (Join-Path $root 'Saved\Logs\Q-files.log')
  Log 'RESULT OK'
} catch {
  Log "ERROR $($_.Exception.Message)"
}
