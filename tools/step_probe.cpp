#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: step_probe <file.step>\n";
        return 2;
    }

    STEPControl_Reader reader;
    const IFSelect_ReturnStatus status = reader.ReadFile(argv[1]);
    if (status != IFSelect_RetDone) {
        std::cerr << "read failed\n";
        return 1;
    }

    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) {
        std::cerr << "empty shape\n";
        return 1;
    }

    BRepMesh_IncrementalMesh mesh(shape, 0.25, false, 0.5, true);
    mesh.Perform();
    if (!mesh.IsDone()) {
        std::cerr << "mesh failed\n";
        return 1;
    }

    int faces = 0;
    int meshedFaces = 0;
    int triangles = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        ++faces;
        TopLoc_Location location;
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (!triangulation.IsNull()) {
            ++meshedFaces;
            triangles += triangulation->NbTriangles();
        }
    }

    std::cout << "faces=" << faces << "\n";
    std::cout << "meshed_faces=" << meshedFaces << "\n";
    std::cout << "triangles=" << triangles << "\n";
    return triangles > 0 ? 0 : 1;
}
