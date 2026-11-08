param()
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc required; Docker is not supported'
}
node "$Repo/v2/native/hatch_a_pet/tools/export_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Hatch asset export failed' }
python "$Repo/tools/gen_firmware_headers.py" --check
if ($LASTEXITCODE -ne 0) { throw 'Generated board headers are stale' }
$Image = 'little-arcade/idf:5.5.6'
$ImageId = & wslc images --filter "reference=$Image" --quiet
if ($LASTEXITCODE -ne 0) { throw 'Cannot query wslc images' }
if (-not $ImageId) {
  & pwsh "$Repo/tools/idf_wslc_build.ps1" -Prepare
  if ($LASTEXITCODE -ne 0) { throw 'Cannot prepare ESP-IDF image' }
}
$build = @'
set -e
git config --global --add safe.directory '*'
cd /project/v2/native/hatch_a_pet/device
idf.py -B build build
cp ../assets/generated/hatch_a_pet.pack build/hatch_a_pet.pack
idf.py -B build size | tee build/size.txt
idf.py -B build size-components | tee build/size-components.txt
'@ -replace "`r", ''
& wslc run --rm -m 3G -v "${Repo}:/project" $Image bash -c $build
if ($LASTEXITCODE -ne 0) { throw "Hatch device build failed ($LASTEXITCODE)" }
