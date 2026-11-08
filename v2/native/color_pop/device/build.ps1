param(
  # Bench firmware auto-taps targets so worst-case frames can be measured without a child;
  # it builds into build-bench/ and is never the default image.
  [switch]$Bench
)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc required: this project builds in WSL containers, never Docker'
}
node "$Repo/v2/native/color_pop/tools/export_assets.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Canvas artwork export failed' }
python "$Repo/tools/gen_firmware_headers.py" --check
if ($LASTEXITCODE -ne 0) { throw 'Generated board headers are stale' }
$Image = 'little-arcade/idf:5.5.6'
$ImageId = & wslc images --filter "reference=$Image" --quiet
if ($LASTEXITCODE -ne 0) { throw 'Cannot query wslc images' }
if (-not $ImageId) {
  & pwsh "$Repo/tools/idf_wslc_build.ps1" -Prepare
  if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF container preparation failed' }
}
$BuildDir = if ($Bench) { 'build-bench' } else { 'build' }
$Extra = if ($Bench) { '-DCOLOR_POP_BENCH=1' } else { '' }
$build = @"
set -e
git config --global --add safe.directory '*'
cd /project/v2/native/color_pop/device
idf.py -B $BuildDir $Extra build
cp ../assets/generated/color_pop.pack $BuildDir/color_pop.pack
idf.py -B $BuildDir size | tee $BuildDir/size.txt
idf.py -B $BuildDir size-components | tee $BuildDir/size-components.txt
"@ -replace "`r", ''
& wslc run --rm -m 3G -v "${Repo}:/project" $Image bash -c $build
if ($LASTEXITCODE -ne 0) { throw "Color Pop ESP-IDF build failed ($LASTEXITCODE)" }
