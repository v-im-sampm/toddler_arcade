# Reproducible ESP-IDF build for Little Arcade using a reusable WSL containers (wslc) image.
# Usage: pwsh tools/idf_wslc_build.ps1 [-Prepare] [-RebuildImage] [-Clean] [-Memory 3G] [-Jobs 2]
# -Memory caps the container (wslc run -m); use it when the Windows host is low on
# commit memory and containers are killed with exit code 137. -Jobs limits ninja
# parallelism for an already-configured build (lower peak RAM).
# -Prepare builds the project image once without building firmware. The first regular
# invocation also builds the image if it is missing. -RebuildImage refreshes that image.
# Produces firmware/build/ plus size reports. Builds only: never flashes.
param(
  [switch]$Prepare,
  [switch]$RebuildImage,
  [switch]$Clean,
  [string]$Memory,
  [int]$Jobs = 0
)
$ErrorActionPreference = 'Stop'
$Image = 'little-arcade/idf:5.5.6'
$Repo = (Resolve-Path "$PSScriptRoot/..").Path

if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc (WSL containers CLI) not found. Install/update WSL; Docker is not supported for this project.'
}

$ImageId = & wslc images --filter "reference=$Image" --quiet
if ($LASTEXITCODE -ne 0) { throw 'Could not query local WSL container images' }
if ($RebuildImage -or -not $ImageId) {
  & wslc build -f "$PSScriptRoot/Containerfile.idf" -t $Image "$PSScriptRoot"
  if ($LASTEXITCODE -ne 0) { throw "Failed to prepare build image $Image" }
} else {
  Write-Host "Using cached build image $Image"
}
if ($Prepare) { return }

python "$Repo/tools/gen_firmware_headers.py" --check
if ($LASTEXITCODE -ne 0) { throw 'Generated headers are stale' }

$script = @'
set -e
git config --global --add safe.directory '*'
cd /project/firmware
if [ "$CLEAN" = "1" ]; then rm -rf build sdkconfig managed_components; fi
if [ "$JOBS" -gt 0 ] && [ -f build/build.ninja ]; then ninja -C build -j "$JOBS" all; fi
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

$limit = if ($Memory) { @('-m', $Memory) } else { @() }
wslc run --rm @limit -e CLEAN=$([int]$Clean.IsPresent) -e JOBS=$Jobs -v "${Repo}:/project" $Image bash -c $script
if ($LASTEXITCODE -ne 0) { throw "Container build failed ($LASTEXITCODE)" }
