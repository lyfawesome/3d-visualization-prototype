#include "MainWindow.h"

#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkCleanPolyData.h>
#include <vtkIdTypeArray.h>
#include <vtkIntArray.h>
#include <vtkLookupTable.h>
#include <vtkOBJReader.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSTLReader.h>
#include <vtkTriangleFilter.h>

void MainWindow::loadModel(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    QString displayName = QFileInfo(filePath).fileName();
    vtkSmartPointer<vtkPolyData> polyData;

    if (suffix == "step" || suffix == "stp") {
        try {
            polyData = loadStepModel(filePath);
            displayName += " (STEP via OCCT)";
        } catch (const std::exception &ex) {
            QMessageBox::warning(this, "STEP import failed", ex.what());
            return;
        }
    } else if (suffix == "obj") {
        auto objReader = vtkSmartPointer<vtkOBJReader>::New();
        objReader->SetFileName(filePath.toLocal8Bit().constData());
        auto triangle = vtkSmartPointer<vtkTriangleFilter>::New();
        triangle->SetInputConnection(objReader->GetOutputPort());

        auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
        cleaner->SetInputConnection(triangle->GetOutputPort());
        cleaner->Update();

        polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->DeepCopy(cleaner->GetOutput());
    } else if (suffix == "stl") {
        auto stlReader = vtkSmartPointer<vtkSTLReader>::New();
        stlReader->SetFileName(filePath.toLocal8Bit().constData());
        auto triangle = vtkSmartPointer<vtkTriangleFilter>::New();
        triangle->SetInputConnection(stlReader->GetOutputPort());

        auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
        cleaner->SetInputConnection(triangle->GetOutputPort());
        cleaner->Update();

        polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->DeepCopy(cleaner->GetOutput());
    } else {
        QMessageBox::warning(this, "Unsupported model", "Unsupported model format: " + suffix);
        return;
    }

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        QMessageBox::warning(this, "Model error", "The model has no renderable points.");
        return;
    }

    if (modelActor_) {
        renderer_->RemoveActor(modelActor_);
    }
    if (meshBoundaryActor_) {
        renderer_->RemoveActor(meshBoundaryActor_);
        meshBoundaryActor_ = nullptr;
        meshBoundaryData_ = nullptr;
        meshBoundaryMapper_ = nullptr;
    }
    if (modelPicker_) {
        modelPicker_->InitializePickList();
    }
    controlPoints_.clear();
    volumeMesh_ = VolumeMesh();
    clearConstraintFaceGroups();
    clearControlPointAssignments();
    refreshControlPointActors();
    refreshConstraintFaceActors();
    refreshConstraintFaceList();
    refreshControlPointList();
    refreshLoadingDeviceList();
    refreshLoadingDeviceActors();

    modelPath_ = filePath;
    modelData_ = polyData;
    modelMapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    modelMapper_->SetInputData(modelData_);
    modelMapper_->SetLookupTable(lookupTable_);
    modelMapper_->ScalarVisibilityOn();

    modelActor_ = vtkSmartPointer<vtkActor>::New();
    modelActor_->SetMapper(modelMapper_);
    modelActor_->GetProperty()->SetInterpolationToPhong();
    modelActor_->GetProperty()->SetSpecular(0.25);
    modelActor_->GetProperty()->SetSpecularPower(18);
    renderer_->AddActor(modelActor_);
    if (modelPicker_) {
        modelPicker_->AddPickList(modelActor_);
    }

    showImportedModelOnly();
    modelLabel_->setText(QString("Model: %1\nPoints: %2").arg(displayName).arg(modelData_->GetNumberOfPoints()));
    applyContour();
    resetView();
    refreshWorkflowStatus();
    statusBar()->showMessage("Loaded model: " + displayName);
}
vtkSmartPointer<vtkPolyData> MainWindow::loadStepModel(const QString &filePath)
{
    STEPControl_Reader reader;
    const IFSelect_ReturnStatus status = reader.ReadFile(filePath.toLocal8Bit().constData());
    if (status != IFSelect_RetDone) {
        throw std::runtime_error("OCCT could not read the STEP/STP file.");
    }

    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) {
        throw std::runtime_error("The STEP/STP file did not contain a valid shape.");
    }

    const double linearDeflection = 0.25;
    const double angularDeflection = 0.5;
    BRepMesh_IncrementalMesh mesh(shape, linearDeflection, false, angularDeflection, true);
    mesh.Perform();
    if (!mesh.IsDone()) {
        throw std::runtime_error("OCCT failed to create a surface mesh from the STEP/STP shape.");
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto triangles = vtkSmartPointer<vtkCellArray>::New();
    auto sourceFaceIds = vtkSmartPointer<vtkIntArray>::New();
    sourceFaceIds->SetName("SourceFaceId");
    std::vector<vtkIdType> triangleCells;

    int sourceFaceId = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        ++sourceFaceId;
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }

        const gp_Trsf transform = location.Transformation();
        for (int i = 1; i <= triangulation->NbTriangles(); ++i) {
            int n1 = 0;
            int n2 = 0;
            int n3 = 0;
            triangulation->Triangle(i).Get(n1, n2, n3);
            if (face.Orientation() == TopAbs_REVERSED) {
                std::swap(n2, n3);
            }

            gp_Pnt p1 = triangulation->Node(n1);
            gp_Pnt p2 = triangulation->Node(n2);
            gp_Pnt p3 = triangulation->Node(n3);
            p1.Transform(transform);
            p2.Transform(transform);
            p3.Transform(transform);

            const vtkIdType ids[3] = {
                points->InsertNextPoint(p1.X(), p1.Y(), p1.Z()),
                points->InsertNextPoint(p2.X(), p2.Y(), p2.Z()),
                points->InsertNextPoint(p3.X(), p3.Y(), p3.Z())
            };
            triangleCells.push_back(3);
            triangleCells.push_back(ids[0]);
            triangleCells.push_back(ids[1]);
            triangleCells.push_back(ids[2]);
            sourceFaceIds->InsertNextValue(sourceFaceId);
        }
    }

    auto triangleArray = vtkSmartPointer<vtkIdTypeArray>::New();
    vtkIdType *cellData = triangleArray->WritePointer(0, static_cast<vtkIdType>(triangleCells.size()));
    std::copy(triangleCells.begin(), triangleCells.end(), cellData);
    triangles->ImportLegacyFormat(triangleArray);

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetPolys(triangles);
    polyData->GetCellData()->AddArray(sourceFaceIds);

    auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputData(polyData);
    cleaner->Update();

    auto cleaned = vtkSmartPointer<vtkPolyData>::New();
    cleaned->DeepCopy(cleaner->GetOutput());
    if (cleaned->GetNumberOfPoints() == 0 || cleaned->GetNumberOfCells() == 0) {
        throw std::runtime_error("The STEP/STP shape did not produce renderable mesh triangles.");
    }
    return cleaned;
}
