$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc required: this project builds in WSL containers, never Docker'
}
# Both games' Canvas artwork: Color Pop's mmapped pack (+ pack_signature.hpp) and the Aquarium
# BGRA partition image (+ sprite_pack.hpp).
node "$Repo/v2/native/color_pop/tools/export_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Color Pop artwork export failed' }
node "$Repo/v2/native/bubble_aquarium/tools/export_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Aquarium artwork export failed' }
node "$Repo/v2/native/bubble_aquarium/tools/pack_device_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Aquarium sprite partition packing failed' }
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
cd /project/v2/native/launcher/device
idf.py build
cp ../../color_pop/assets/generated/color_pop.pack build/color_pop.pack
cp ../../bubble_aquarium/device/build/sprite_assets.bin build/sprite_assets.bin
idf.py size | tee build/size.txt
idf.py size-components | tee build/size-components.txt
'@ -replace "`r", ''
& wslc run --rm -m 3G -v "${Repo}:/project" $Image bash -c $build
if ($LASTEXITCODE -ne 0) { throw "Launcher ESP-IDF build failed ($LASTEXITCODE)" }
