#include "MainWindow.h"

#include <QByteArray>
#include <QListWidget>
#include <QSet>
#include <QStatusBar>

#include <QVTKOpenGLNativeWidget.h>

#include <limits>

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCell.h>
#include <vtkCellPicker.h>
#include <vtkDataArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkObject.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSphereSource.h>
#include <vtkTextProperty.h>

int MainWindow::nearestModelPointId(double x, double y, double z) const
{
    if (!modelData_) {
        return -1;
    }

    int bestId = -1;
    double bestDistance2 = std::numeric_limits<double>::max();
    for (vtkIdType i = 0; i < modelData_->GetNumberOfPoints(); ++i) {
        double point[3];
        modelData_->GetPoint(i, point);
        const double dx = point[0] - x;
        const double dy = point[1] - y;
        const double dz = point[2] - z;
        const double distance2 = dx * dx + dy * dy + dz * dz;
        if (distance2 < bestDistance2) {
            bestDistance2 = distance2;
            bestId = int(i);
        }
    }
    return bestId;
}
int MainWindow::nearestMeshBoundaryNodeId(double x, double y, double z) const
{
    int bestId = -1;
    double bestDistance2 = std::numeric_limits<double>::max();

    if (!volumeMesh_.boundaryFaces.isEmpty()) {
        QSet<int> boundaryNodeIds;
        for (const VolumeBoundaryFace &face : volumeMesh_.boundaryFaces) {
            boundaryNodeIds.insert(face.a);
            boundaryNodeIds.insert(face.b);
            boundaryNodeIds.insert(face.c);
        }
        for (int nodeId : boundaryNodeIds) {
            if (nodeId < 0 || nodeId >= volumeMesh_.nodes.size()) {
                continue;
            }
            const VolumeNode &node = volumeMesh_.nodes.at(nodeId);
            const double dx = node.x - x;
            const double dy = node.y - y;
            const double dz = node.z - z;
            const double distance2 = dx * dx + dy * dy + dz * dz;
            if (distance2 < bestDistance2) {
                bestDistance2 = distance2;
                bestId = nodeId;
            }
        }
    }
    return bestId;
}
void MainWindow::refreshControlPointActors()
{
    for (const auto &actor : controlPointActors_) {
        renderer_->RemoveActor(actor);
    }
    for (const auto &label : controlPointLabelActors_) {
        renderer_->RemoveActor(label);
    }
    controlPointActors_.clear();
    controlPointLabelActors_.clear();

    for (int i = 0; i < controlPoints_.size(); ++i) {
        const QVector3D &point = controlPoints_.at(i);
        bool assigned = false;
        bool selectedDevicePoint = false;
        const int selectedDevice = loadingDeviceList_ ? loadingDeviceList_->currentRow() : -1;
        for (int j = 0; j < loadingDevices_.size(); ++j) {
            if (loadingDevices_.at(j).assignedControlPointIndex == i) {
                assigned = true;
                selectedDevicePoint = selectedDevicePoint || j == selectedDevice;
            }
        }

        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetCenter(point.x(), point.y(), point.z());
        sphere->SetRadius(selectedDevicePoint ? 0.15 : 0.11);
        sphere->SetThetaResolution(32);
        sphere->SetPhiResolution(16);

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        if (selectedDevicePoint) {
            actor->GetProperty()->SetColor(1.0, 0.55, 0.10);
        } else if (assigned) {
            actor->GetProperty()->SetColor(0.95, 0.18, 0.25);
        } else {
            actor->GetProperty()->SetColor(0.05, 0.85, 1.0);
        }
        actor->GetProperty()->SetAmbient(0.45);
        actor->GetProperty()->SetDiffuse(0.70);
        actor->GetProperty()->SetSpecular(0.35);
        renderer_->AddActor(actor);
        controlPointActors_.push_back(actor);

        auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        const QByteArray labelText = QString("P%1").arg(i + 1).toUtf8();
        label->SetInput(labelText.constData());
        label->SetPosition(point.x() + 0.10, point.y() + 0.10, point.z() + 0.14);
        label->GetTextProperty()->SetColor(0.82, 0.96, 1.0);
        label->GetTextProperty()->SetFontSize(18);
        renderer_->AddActor(label);
        controlPointLabelActors_.push_back(label);
    }

    renderWindow_->Render();
}
void MainWindow::refreshControlPointList()
{
    refreshLoadingDeviceList();
}
void MainWindow::addControlPoint(double x, double y, double z, const QVector3D &normal, int meshNodeId)
{
    const int deviceIndex = nextUnassignedLoadingDeviceIndex();
    if (deviceIndex < 0 || deviceIndex >= loadingDevices_.size()) {
        statusBar()->showMessage("All load points are already placed.");
        if (pickControlPointAction_) {
            pickControlPointAction_->setChecked(false);
        }
        return;
    }

    controlPoints_.push_back(QVector3D(float(x), float(y), float(z)));
    LoadingDevice &device = loadingDevices_[deviceIndex];
    device.assignedControlPointIndex = controlPoints_.size() - 1;
    device.position = controlPoints_.last();
    device.direction = normal.lengthSquared() > 1e-8f ? normal.normalized() : QVector3D(0.0f, 0.0f, 1.0f);
    device.meshNodeId = meshNodeId;

    refreshControlPointActors();
    refreshControlPointList();
    refreshLoadingDeviceActors();
    applyContour();
    refreshWorkflowStatus();

    const int remaining = nextUnassignedLoadingDeviceIndex() >= 0 ? loadingDevices_.size() - controlPoints_.size() : 0;
    if (remaining == 0 && pickControlPointAction_) {
        pickControlPointAction_->setChecked(false);
    }
    statusBar()->showMessage(QString("Placed %1 at (%2, %3, %4). %5 remaining.")
        .arg(device.id)
        .arg(x, 0, 'f', 3)
        .arg(y, 0, 'f', 3)
        .arg(z, 0, 'f', 3)
        .arg(remaining));
}
void MainWindow::clearControlPoints()
{
    controlPoints_.clear();
    clearControlPointAssignments();
    refreshControlPointActors();
    refreshConstraintFaceList();
    refreshControlPointList();
    refreshLoadingDeviceList();
    refreshLoadingDeviceActors();
    applyContour();
    refreshWorkflowStatus();
    statusBar()->showMessage("Cleared load point positions.");
}
int MainWindow::nextUnassignedLoadingDeviceIndex() const
{
    for (int i = 0; i < loadingDevices_.size(); ++i) {
        const LoadingDevice &device = loadingDevices_.at(i);
        if (device.assignedControlPointIndex < 0 || device.assignedControlPointIndex >= controlPoints_.size()) {
            return i;
        }
    }
    return -1;
}
void MainWindow::clearControlPointAssignments()
{
    for (LoadingDevice &device : loadingDevices_) {
        device.assignedControlPointIndex = -1;
        device.meshNodeId = -1;
    }
}
bool MainWindow::handleLeftButtonPress()
{
    const bool pickLoads = pickControlPointAction_ && pickControlPointAction_->isChecked();
    const bool pickConstraintFace = pickConstraintFaceAction_ && pickConstraintFaceAction_->isChecked();
    if (!pickLoads && !pickConstraintFace) {
        return false;
    }
    if (!modelActor_ || !modelPicker_ || !vtkWidget_ || !vtkWidget_->interactor()) {
        statusBar()->showMessage("Load a model before picking nodes.");
        return true;
    }
    if (pickLoads && volumeMesh_.nodes.isEmpty()) {
        statusBar()->showMessage("Generate a Netgen volume mesh before picking load points.");
        return true;
    }
    if (pickLoads && loadingDevices_.isEmpty()) {
        statusBar()->showMessage("Load a load-point file before picking positions.");
        return true;
    }
    if (pickLoads && nextUnassignedLoadingDeviceIndex() < 0) {
        statusBar()->showMessage("All load points are already placed.");
        pickControlPointAction_->setChecked(false);
        return true;
    }

    int *eventPosition = vtkWidget_->interactor()->GetEventPosition();
    const int picked = modelPicker_->Pick(eventPosition[0], eventPosition[1], 0.0, renderer_);
    vtkActor *pickedActor = modelPicker_->GetActor();
    const bool pickedModel = pickedActor == modelActor_;
    const bool pickedMeshBoundary = meshBoundaryActor_ && pickedActor == meshBoundaryActor_;
    if (!picked || (!pickedModel && !pickedMeshBoundary)) {
        statusBar()->showMessage("Pick mode: click on the model surface.");
        return true;
    }
    if (pickLoads && !pickedMeshBoundary) {
        statusBar()->showMessage("Pick load points on the generated boundary surface.");
        return true;
    }

    const vtkIdType cellId = modelPicker_->GetCellId();
    if (pickConstraintFace) {
        if (!pickedModel) {
            statusBar()->showMessage("Constraint faces must be picked on the imported STEP model.");
            return true;
        }
        if (pickControlPointAction_) {
            pickControlPointAction_->setChecked(false);
        }
        addConstraintFaceFromCell(cellId);
        return true;
    }

    double pickPosition[3] = {0.0, 0.0, 0.0};
    modelPicker_->GetPickPosition(pickPosition);
    double nodePosition[3] = {pickPosition[0], pickPosition[1], pickPosition[2]};

    QVector3D normal(0.0f, 0.0f, 1.0f);
    int pickedVolumeNodeId = -1;
    if (pickLoads && pickedMeshBoundary && meshBoundaryData_ && cellId >= 0) {
        vtkCell *cell = meshBoundaryData_->GetCell(cellId);
        if (cell && cell->GetNumberOfPoints() >= 3) {
            double bestDistance2 = std::numeric_limits<double>::max();
            for (vtkIdType i = 0; i < cell->GetNumberOfPoints(); ++i) {
                double point[3];
                cell->GetPoints()->GetPoint(i, point);
                const double dx = point[0] - pickPosition[0];
                const double dy = point[1] - pickPosition[1];
                const double dz = point[2] - pickPosition[2];
                const double distance2 = dx * dx + dy * dy + dz * dz;
                if (distance2 < bestDistance2) {
                    bestDistance2 = distance2;
                    pickedVolumeNodeId = int(cell->GetPointId(i));
                    nodePosition[0] = point[0];
                    nodePosition[1] = point[1];
                    nodePosition[2] = point[2];
                }
            }

            double p0[3];
            double p1[3];
            double p2[3];
            cell->GetPoints()->GetPoint(0, p0);
            cell->GetPoints()->GetPoint(1, p1);
            cell->GetPoints()->GetPoint(2, p2);
            QVector3D v1(float(p1[0] - p0[0]), float(p1[1] - p0[1]), float(p1[2] - p0[2]));
            QVector3D v2(float(p2[0] - p0[0]), float(p2[1] - p0[1]), float(p2[2] - p0[2]));
            QVector3D computed = QVector3D::crossProduct(v1, v2);
            if (computed.lengthSquared() > 1e-8f) {
                normal = computed.normalized();
            }
        }
    }

    auto *camera = renderer_->GetActiveCamera();
    if (camera) {
        double cameraPosition[3];
        camera->GetPosition(cameraPosition);
        QVector3D toCamera(
            float(cameraPosition[0] - nodePosition[0]),
            float(cameraPosition[1] - nodePosition[1]),
            float(cameraPosition[2] - nodePosition[2]));
        if (toCamera.lengthSquared() > 1e-8f && QVector3D::dotProduct(normal, toCamera.normalized()) < 0.0f) {
            normal = -normal;
        }
    }

    int meshNodeId = pickedVolumeNodeId >= 0 ? pickedVolumeNodeId : nearestMeshBoundaryNodeId(nodePosition[0], nodePosition[1], nodePosition[2]);
    if (meshNodeId >= 0 && meshNodeId < volumeMesh_.nodes.size()) {
        const VolumeNode &node = volumeMesh_.nodes.at(meshNodeId);
        nodePosition[0] = node.x;
        nodePosition[1] = node.y;
        nodePosition[2] = node.z;
    }
    addControlPoint(nodePosition[0], nodePosition[1], nodePosition[2], normal, meshNodeId);
    return true;
}
void MainWindow::onVtkLeftButtonPress(vtkObject *caller, unsigned long eventId, void *clientData, void *callData)
{
    Q_UNUSED(caller);
    Q_UNUSED(eventId);
    Q_UNUSED(callData);

    auto *window = static_cast<MainWindow *>(clientData);
    if (!window) {
        return;
    }

    if (window->leftButtonPressCallback_) {
        window->leftButtonPressCallback_->SetAbortFlag(0);
    }
    if (window->handleLeftButtonPress() && window->leftButtonPressCallback_) {
        window->leftButtonPressCallback_->SetAbortFlag(1);
    }
}
