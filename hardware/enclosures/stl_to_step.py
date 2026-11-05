"""Convert rendered STL meshes to STEP solids with OpenCascade (pip install cadquery-ocp).

Usage: python stl_to_step.py in.stl out.step [in2.stl out2.step ...]
The result is faceted (round features stay polygons); edit yellow_handle.scad, not the STEP.
"""
import sys

from OCP.BRep import BRep_Builder
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeSolid, BRepBuilderAPI_Sewing
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.IFSelect import IFSelect_RetDone
from OCP.ShapeFix import ShapeFix_Solid
from OCP.ShapeUpgrade import ShapeUpgrade_UnifySameDomain
from OCP.StlAPI import StlAPI_Reader
from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
from OCP.TopAbs import TopAbs_SHELL, TopAbs_SOLID
from OCP.TopExp import TopExp_Explorer
from OCP.TopoDS import TopoDS, TopoDS_Shape


def _volume(shape):
    props = GProp_GProps()
    BRepGProp.VolumeProperties_s(shape, props)
    return props.Mass()


def convert(stl_path, step_path):
    mesh = TopoDS_Shape()
    if not StlAPI_Reader().Read(mesh, stl_path):
        raise RuntimeError(f"cannot read {stl_path}")
    sewing = BRepBuilderAPI_Sewing(1e-4)
    sewing.Add(mesh)
    sewing.Perform()
    sewn = sewing.SewedShape()

    to_shell = getattr(TopoDS, "Shell_s", None) or TopoDS.Shell
    maker = BRepBuilderAPI_MakeSolid()
    shells = TopExp_Explorer(sewn, TopAbs_SHELL)
    while shells.More():
        maker.Add(to_shell(shells.Current()))
        shells.Next()
    fixer = ShapeFix_Solid(maker.Solid())
    fixer.Perform()
    unify = ShapeUpgrade_UnifySameDomain(fixer.Solid(), True, True, True)
    unify.Build()
    solid = unify.Shape()

    writer = STEPControl_Writer()
    writer.Transfer(solid, STEPControl_AsIs)
    if writer.Write(step_path) != IFSelect_RetDone:
        raise RuntimeError(f"cannot write {step_path}")

    # Strip trailing whitespace so the file passes git diff --check.
    with open(step_path, encoding="latin-1") as f:
        lines = [line.rstrip() for line in f]
    with open(step_path, "w", encoding="latin-1", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    reader = STEPControl_Reader()
    if reader.ReadFile(step_path) != IFSelect_RetDone:
        raise RuntimeError(f"cannot re-read {step_path}")
    reader.TransferRoots()
    shape = reader.OneShape()
    solids = 0
    explorer = TopExp_Explorer(shape, TopAbs_SOLID)
    while explorer.More():
        solids += 1
        explorer.Next()
    valid = BRepCheck_Analyzer(shape).IsValid()
    print(f"{step_path}: solids={solids} valid={valid} volume={_volume(shape):.0f} mm^3")
    if solids != 1 or not valid:
        raise RuntimeError(f"{step_path} is not one valid solid")


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or len(args) % 2:
        sys.exit(__doc__)
    for i in range(0, len(args), 2):
        convert(args[i], args[i + 1])
