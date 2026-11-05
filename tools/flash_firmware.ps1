param(
  [Parameter(Mandatory = $true)]
  [string]$Port,
  [int]$Baud = 460800
)

$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/..").Path
$Python = Join-Path $Repo '.venv/Scripts/python.exe'
$Requirements = Join-Path $PSScriptRoot 'requirements-flash.txt'

if (-not (Test-Path $Python)) {
  py -m venv (Join-Path $Repo '.venv')
  if ($LASTEXITCODE -ne 0) { throw 'Failed to create the project flash-tool virtual environment' }
}

& $Python -m pip install --disable-pip-version-check -r $Requirements
if ($LASTEXITCODE -ne 0) { throw 'Failed to install the pinned flash-tool requirements' }

$Images = @(
  'firmware/build/bootloader/bootloader.bin',
  'firmware/build/partition_table/partition-table.bin',
  'firmware/build/little_arcade.bin',
  'firmware/build/srmodels/srmodels.bin'
)
foreach ($ImagePath in $Images) {
  if (-not (Test-Path (Join-Path $Repo $ImagePath))) {
    throw "Missing $ImagePath; build the firmware before flashing"
  }
}

$FlashArgs = @(
  '--chip', 'esp32s3',
  '-p', $Port,
  '-b', "$Baud",
  '--before', 'default-reset',
  '--after', 'hard-reset',
  'write-flash',
  '--flash-mode', 'dio',
  '--flash-size', '16MB',
  '--flash-freq', '80m',
  '0x0', (Join-Path $Repo $Images[0]),
  '0x8000', (Join-Path $Repo $Images[1]),
  '0x10000', (Join-Path $Repo $Images[2]),
  '0x410000', (Join-Path $Repo $Images[3])
)

& $Python -m esptool @FlashArgs
if ($LASTEXITCODE -ne 0) { throw "Firmware flash failed ($LASTEXITCODE)" }
