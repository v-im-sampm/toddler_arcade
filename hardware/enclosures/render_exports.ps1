# Regenerates the PNG renders in exports/ from yellow_handle.scad. Run from the repo root.
# Requires an OpenSCAD development snapshot (winget install OpenSCAD.OpenSCAD.Nightly).
param(
    [string]$Scad = "C:\Program Files\OpenSCAD (Nightly)\openscad.com",
    [string]$OutDir = "hardware\enclosures\exports"
)
$ErrorActionPreference = "Stop"
$views = "hardware\enclosures\yellow_handle_views.scad"
$common = @("--backend=manifold", "--colorscheme=Tomorrow", "-D", 'part="none"')

# name, view, camera (translate x,y,z, rotate x,y,z, distance), size, extra args
$renders = @(
    @("assembled",      "assembled",    "0,0,0,75,0,-20,0",  "1400,1050", @()),
    @("assembled_back", "assembled",    "0,0,0,75,0,160,0",  "1400,1050", @()),
    @("front_outside",  "front_out",    "0,0,0,75,0,-20,0",  "1400,1050", @()),
    @("front_inside",   "front_in",     "0,0,0,70,0,165,0",  "1400,1050", @()),
    @("back_outside",   "back_out",     "0,0,0,75,0,160,0",  "1400,1050", @()),
    @("back_inside",    "back_in",      "0,0,0,70,0,-15,0",  "1400,1050", @()),
    @("layout_back",    "layout_iso",   "0,0,0,70,0,-20,0",  "1400,1050", @()),
    @("layout_front",   "front_layout", "0,0,0,70,0,165,0",  "1400,1050", @()),
    @("layout_plan",    "plan",         "0,0,0,0,180,0,0",   "1400,1200", @("--projection=o")),
    @("section_y0",     "section_y",    "0,0,0,90,0,180,0",  "1400,700",  @("--projection=o", "--render")),
    @("section_x0",     "section_x",    "0,0,0,90,0,90,0",   "1400,700",  @("--projection=o", "--render"))
)

foreach ($r in $renders) {
    $out = Join-Path $OutDir "yellow_handle_$($r[0]).png"
    $log = & $Scad @common -D "view=`"$($r[1])`"" "--camera=$($r[2])" --viewall --autocenter "--imgsize=$($r[3])" @($r[4]) -o $out $views 2>&1 | Out-String
    if ($log -cmatch "ERROR:") { throw "Render of $($r[0]) failed:`n$log" }
    Write-Host "wrote $out"
}
