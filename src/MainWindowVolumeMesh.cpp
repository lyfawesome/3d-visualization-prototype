#include "MainWindow.h"
#include "NetgenVolumeMesher.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMetaObject>
#include <QMessageBox>
#include <QPointer>
#include <QStatusBar>
#include <QThread>

#include <stdexcept>

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCellArray.h>
#include <vtkCellPicker.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

void MainWindow::showImportedModelOnly()
{
    if (modelActor_) {
        modelActor_->SetVisibility(true);
        modelActor_->GetProperty()->SetOpacity(1.0);
    }
    if (meshBoundaryActor_) {
        meshBoundaryActor_->SetVisibility(false);
    }
    if (modelPicker_) {
        modelPicker_->InitializePickList();
        if (modelActor_) {
            modelPicker_->AddPickList(modelActor_);
        }
    }
}

void MainWindow::showGeneratedMeshOnly()
{
    if (modelActor_) {
        modelActor_->SetVisibility(false);
    }

    if (meshBoundaryActor_) {
        meshBoundaryActor_->SetVisibility(true);
        meshBoundaryActor_->GetProperty()->SetOpacity(1.0);
    }
    if (modelPicker_) {
        modelPicker_->InitializePickList();
        if (meshBoundaryActor_) {
            modelPicker_->AddPickList(meshBoundaryActor_);
        }
    }
}

void MainWindow::showVolumeMesh(const QString &sourceName)
{
    if (meshBoundaryActor_) {
        renderer_->RemoveActor(meshBoundaryActor_);
        meshBoundaryActor_ = nullptr;
    }
    meshBoundaryData_ = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    for (const VolumeNode &node : volumeMesh_.nodes) {
        points->InsertNextPoint(node.x, node.y, node.z);
    }
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (const VolumeBoundaryFace &face : volumeMesh_.boundaryFaces) {
        const vtkIdType ids[3] = {face.a, face.b, face.c};
        cells->InsertNextCell(3, ids);
    }
    meshBoundaryData_->SetPoints(points);
    meshBoundaryData_->SetPolys(cells);

    meshBoundaryMapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    meshBoundaryMapper_->SetInputData(meshBoundaryData_);
    meshBoundaryActor_ = vtkSmartPointer<vtkActor>::New();
    meshBoundaryActor_->SetMapper(meshBoundaryMapper_);
    meshBoundaryActor_->GetProperty()->SetColor(0.25, 0.70, 1.0);
    renderer_->AddActor(meshBoundaryActor_);
    showGeneratedMeshOnly();
    refreshConstraintFaceActors();
    renderWindow_->Render();

    const QString message = QString("%1 volume mesh: %2 nodes, %3 tetrahedra.")
        .arg(sourceName)
        .arg(volumeMesh_.nodes.size())
        .arg(volumeMesh_.tetrahedra.size());
    refreshWorkflowStatus();
    statusBar()->showMessage(message);
    QMessageBox::information(this, sourceName, message);
}

void MainWindow::generateVolumeMesh()
{
    if (meshJobRunning_ || solverJobRunning_) {
        QMessageBox::information(this, "Netgen", "A background task is already running. Wait for it to finish before starting meshing.");
        return;
    }
    if (!ensureProjectReady(QStringLiteral("generating a Netgen volume mesh"))) {
        return;
    }
    if (modelPath_.isEmpty()) {
        QMessageBox::warning(this, "Netgen", "Load a STEP/STP model before generating a volume mesh.");
        return;
    }

    const QString suffix = QFileInfo(modelPath_).suffix().toLower();
    if (suffix != "step" && suffix != "stp") {
        QMessageBox::warning(this, "Netgen", "Netgen volume meshing is currently available for STEP/STP models only.");
        return;
    }
    const QVector<int> constraintFaceIds = selectedConstraintFaceIds();
    if (constraintFaceIds.isEmpty()) {
        QMessageBox::warning(this, "Netgen", "Pick at least one constraint face before generating a Netgen mesh.");
        return;
    }

    const QString workDir = projectSubdir(QStringLiteral("meshes/netgen"));
    if (workDir.isEmpty()) {
        QMessageBox::warning(this, "Netgen", "Could not create the project Netgen mesh directory.");
        return;
    }

    const QString stepPath = modelPath_;
    meshJobRunning_ = true;
    startBackgroundProgress(QStringLiteral("Generating Netgen volume mesh..."));

    QPointer<MainWindow> self(this);
    QThread *worker = QThread::create([self, stepPath, workDir, constraintFaceIds]() {
        VolumeMesh mesh;
        QString errorMessage;
        try {
            mesh = NetgenVolumeMesher::meshStepFile(stepPath, workDir, constraintFaceIds);
        } catch (const std::exception &exception) {
            errorMessage = QString::fromLocal8Bit(exception.what());
        } catch (...) {
            errorMessage = QStringLiteral("Unknown Netgen meshing error.");
        }

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, mesh, errorMessage]() {
            if (!self) {
                return;
            }
            self->meshJobRunning_ = false;
            self->meshWorker_ = nullptr;
            if (!errorMessage.isEmpty()) {
                self->finishBackgroundProgress(QStringLiteral("Netgen meshing failed."));
                QMessageBox::warning(self, "Netgen", errorMessage);
                return;
            }

            self->volumeMesh_ = mesh;
            self->finishBackgroundProgress(QStringLiteral("Netgen volume mesh generated."));
            self->showVolumeMesh(QStringLiteral("Netgen"));
        }, Qt::QueuedConnection);
    });

    meshWorker_ = worker;
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}
