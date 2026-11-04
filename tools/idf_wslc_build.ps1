# Reproducible ESP-IDF build for Little Arcade using the pinned Espressif container under WSL containers (wslc).
# Usage: pwsh tools/idf_wslc_build.ps1 [-Clean]
# Produces firmware/build/ plus size reports. Builds only: never flashes.
param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$Image = 'espressif/idf:v5.5.6'
$Repo = (Resolve-Path "$PSScriptRoot/..").Path

if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc (WSL containers CLI) not found. Install/update WSL; Docker is not supported for this project.'
}

python "$Repo/tools/gen_firmware_headers.py" --check
if ($LASTEXITCODE -ne 0) { throw 'Generated headers are stale' }

$script = @'
set -e
git config --global --add safe.directory '*'
cd /project/firmware
if [ "$CLEAN" = "1" ]; then rm -rf build sdkconfig managed_components; fi
idf.py build
idf.py size | tee build/size.txt
idf.py size-components | tee build/size-components.txt
if command -v c++ >/dev/null; then
  cmake -S host_test -B /tmp/host-build -DCMAKE_BUILD_TYPE=Debug >/dev/null
  cmake --build /tmp/host-build
  ctest --test-dir /tmp/host-build --output-on-failure
else
  echo "SKIP: no host C++ compiler in image"
fi
'@ -replace "`r", ''

wslc run --rm -e CLEAN=$([int]$Clean.IsPresent) -v "${Repo}:/project" $Image bash -c $script
if ($LASTEXITCODE -ne 0) { throw "Container build failed ($LASTEXITCODE)" }
