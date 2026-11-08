param(
  [string]$Port = 'COM16',
  [int]$Baud = 460800,
  [switch]$Bench
)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
$Build = Join-Path $Repo ("v2/native/color_pop/device/" + $(if ($Bench) { 'build-bench' } else { 'build' }))
$Python = Join-Path $Repo '.venv/Scripts/python.exe'
$files = @(
  (Join-Path $Build 'bootloader/bootloader.bin'),
  (Join-Path $Build 'partition_table/partition-table.bin'),
  (Join-Path $Build 'color_pop_sprite.bin'),
  (Join-Path $Build 'color_pop.pack')
)
foreach ($file in $files) {
  if (-not (Test-Path $file)) { throw "Missing $file; run build.ps1 before flashing" }
}
if ((Get-Item $files[2]).Length -gt 0x400000) { throw 'Firmware exceeds factory app partition' }
if ((Get-Item $files[3]).Length -gt 0x5F0000) { throw 'Sprite pack exceeds assets partition' }
if (-not (Test-Path $Python)) {
  & pwsh "$Repo/tools/install_flash_tools.ps1"
  if ($LASTEXITCODE -ne 0) { throw 'Failed to install pinned esptool' }
}
# Refuse anything that is not an ESP32-S3 with 16 MB flash.
$id = & $Python -m esptool --chip esp32s3 -p $Port flash-id 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $id -notmatch 'ESP32-S3' -or $id -notmatch '16MB') {
  throw "Port $Port is not an ESP32-S3 with 16 MB flash:`n$id"
}
& $Python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset `
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m `
  0x0 $files[0] 0x8000 $files[1] 0x10000 $files[2] 0xA10000 $files[3]
if ($LASTEXITCODE -ne 0) { throw "Color Pop flash failed ($LASTEXITCODE)" }
