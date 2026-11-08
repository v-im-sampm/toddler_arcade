param(
  [string]$Port = 'COM16',
  [int]$Baud = 460800
)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
$Device = Join-Path $Repo 'v2/native/bubble_aquarium/device'
$Python = Join-Path $Repo '.venv/Scripts/python.exe'
$files = @(
  (Join-Path $Device 'build/bootloader/bootloader.bin'),
  (Join-Path $Device 'build/partition_table/partition-table.bin'),
  (Join-Path $Device 'build/bubble_aquarium_sprite.bin'),
  (Join-Path $Device 'build/sprite_assets.bin')
)
foreach ($file in $files) {
  if (-not (Test-Path $file)) { throw "Missing $file; build sprite firmware before flashing" }
}
if ((Get-Item $files[2]).Length -gt 0x400000) { throw 'Firmware exceeds factory app partition' }
if ((Get-Item $files[3]).Length -gt 0x5F0000) { throw 'Sprites exceed assets partition' }
if (-not (Test-Path $Python)) {
  & pwsh "$Repo/tools/install_flash_tools.ps1"
  if ($LASTEXITCODE -ne 0) { throw 'Failed to install pinned esptool' }
}
& $Python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset `
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m `
  0x0 $files[0] 0x8000 $files[1] 0x10000 $files[2] 0xA10000 $files[3]
if ($LASTEXITCODE -ne 0) { throw "Sprite firmware flash failed ($LASTEXITCODE)" }
