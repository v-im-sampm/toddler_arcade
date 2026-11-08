param([string]$Port = 'COM16', [int]$Baud = 460800)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
$Build = Join-Path $PSScriptRoot 'build'
$Python = Join-Path $Repo '.venv/Scripts/python.exe'
$files = @(
  (Join-Path $Build 'bootloader/bootloader.bin'),
  (Join-Path $Build 'partition_table/partition-table.bin'),
  (Join-Path $Build 'hatch_a_pet_sprite.bin'),
  (Join-Path $Build 'hatch_a_pet.pack')
)
foreach ($file in $files) {
  if (-not (Test-Path $file)) { throw "Missing $file; build before flashing" }
}
if ((Get-Item $files[2]).Length -gt 0x400000) { throw 'App exceeds factory partition' }
if ((Get-Item $files[3]).Length -gt 0x5F0000) { throw 'Pack exceeds assets partition' }
if (-not (Test-Path $Python)) {
  & pwsh "$Repo/tools/install_flash_tools.ps1"
  if ($LASTEXITCODE -ne 0) { throw 'Failed to install pinned esptool' }
}
$id = & $Python -m esptool --chip esp32s3 -p $Port flash-id 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $id -notmatch 'ESP32-S3' -or $id -notmatch '16MB') {
  throw "Port $Port is not a verified ESP32-S3 16MB board:`n$id"
}
& $Python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset `
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m `
  0x0 $files[0] 0x8000 $files[1] 0x10000 $files[2] 0xA10000 $files[3]
if ($LASTEXITCODE -ne 0) { throw "Hatch flash failed ($LASTEXITCODE)" }
