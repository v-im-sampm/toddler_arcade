$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc required: this project builds in WSL containers, never Docker'
}
node "$Repo/v2/native/bubble_aquarium/tools/export_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Canvas artwork export failed' }
node "$Repo/v2/native/bubble_aquarium/tools/pack_device_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Sprite partition packing failed' }
python "$Repo/tools/gen_firmware_headers.py" --check
if ($LASTEXITCODE -ne 0) { throw 'Generated board headers are stale' }
$Image = 'little-arcade/idf:5.5.6'
$ImageId = & wslc images --filter "reference=$Image" --quiet
if ($LASTEXITCODE -ne 0) { throw 'Cannot query wslc images' }
if (-not $ImageId) {
  & pwsh "$Repo/tools/idf_wslc_build.ps1" -Prepare
  if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF container preparation failed' }
}
$build = @'
set -e
git config --global --add safe.directory '*'
cd /project/v2/native/bubble_aquarium/device
idf.py build
idf.py size | tee build/size.txt
idf.py size-components | tee build/size-components.txt
'@ -replace "`r", ''
& wslc run --rm -m 3G -v "${Repo}:/project" $Image bash -c $build
if ($LASTEXITCODE -ne 0) { throw "Aquarium ESP-IDF build failed ($LASTEXITCODE)" }
