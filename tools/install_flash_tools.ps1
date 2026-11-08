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
