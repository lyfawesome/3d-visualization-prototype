#include "MainWindow.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QSet>
#include <QStatusBar>

#include <algorithm>
#include <cmath>
#include <limits>

#include <vtkActor.h>
#include <vtkArrowSource.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLookupTable.h>
#include <vtkMath.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkTextProperty.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

void MainWindow::loadLoadingDevices(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Loading devices", "Could not open loading device file.");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        QMessageBox::warning(this, "Loading devices", "Loading device file must contain a JSON object.");
        return;
    }

    QJsonArray deviceArray = document.object().value("loading_points").toArray();
    if (deviceArray.isEmpty()) {
        deviceArray = document.object().value("loading_devices").toArray();
    }
    if (deviceArray.isEmpty()) {
        QMessageBox::warning(this, "Load points", "JSON must contain a non-empty loading_points array.");
        return;
    }

    QSet<QString> ids;
    QVector<LoadingDevice> devices;
    for (const QJsonValue &value : deviceArray) {
        const QJsonObject object = value.toObject();
        const QString id = object.value("id").toString().trimmed();
        if (id.isEmpty()) {
            QMessageBox::warning(this, "Load points", "Each load point must have a non-empty id.");
            return;
        }
        if (ids.contains(id)) {
            QMessageBox::warning(this, "Load points", "Duplicate load point id: " + id);
            return;
        }

        const QJsonArray position = object.value("position").toArray();
        LoadingDevice device;
        device.id = id;
        device.name = object.value("name").toString(id);
        if (position.size() >= 3) {
            device.position = QVector3D(
                float(position.at(0).toDouble()),
                float(position.at(1).toDouble()),
                float(position.at(2).toDouble()));
        }

        device.currentLoad = object.contains("load") ? object.value("load").toDouble(0.0) : object.value("currentLoad").toDouble(0.0);
        device.unit = object.value("unit").toString("kN");
        device.enabled = object.value("enabled").toBool(true);
        device.assignedControlPointIndex = -1;
        device.meshNodeId = -1;

        ids.insert(id);
        devices.push_back(device);
    }

    loadingDevices_ = devices;
    controlPoints_.clear();
    clearControlPointAssignments();
    if (pickControlPointAction_) {
        pickControlPointAction_->setChecked(true);
    }
    refreshLoadingDeviceList();
    refreshControlPointList();
    refreshControlPointActors();
    refreshLoadingDeviceActors();
    applyContour();
    refreshWorkflowStatus();
    statusBar()->showMessage(QString("Loaded %1 load points. Pick their positions on the generated boundary mesh.").arg(loadingDevices_.size()));
}
void MainWindow::refreshLoadingDeviceActors()
{
    for (const auto &actor : loadingDeviceActors_) {
        renderer_->RemoveActor(actor);
    }
    for (const auto &actor : loadingDirectionActors_) {
        renderer_->RemoveActor(actor);
    }
    for (const auto &label : loadingDeviceLabelActors_) {
        renderer_->RemoveActor(label);
    }
    loadingDeviceActors_.clear();
    loadingDirectionActors_.clear();
    loadingDeviceLabelActors_.clear();

    double arrowLength = 1.0;
    if (modelData_) {
        double bounds[6];
        modelData_->GetBounds(bounds);
        const double dx = bounds[1] - bounds[0];
        const double dy = bounds[3] - bounds[2];
        const double dz = bounds[5] - bounds[4];
        arrowLength = std::max(0.4, std::sqrt(dx * dx + dy * dy + dz * dz) * 0.10);
    }

    double minLoad = std::numeric_limits<double>::max();
    double maxLoad = std::numeric_limits<double>::lowest();
    double maxAbsLoad = 0.0;
    for (const LoadingDevice &device : loadingDevices_) {
        if (!device.enabled) {
            continue;
        }
        minLoad = std::min(minLoad, device.currentLoad);
        maxLoad = std::max(maxLoad, device.currentLoad);
        maxAbsLoad = std::max(maxAbsLoad, std::abs(device.currentLoad));
    }
    if (maxLoad < minLoad) {
        minLoad = 0.0;
        maxLoad = 1.0;
    }
    if (maxLoad <= minLoad) {
        maxLoad = minLoad + 1.0;
    }
    if (maxAbsLoad < 1e-6) {
        maxAbsLoad = 1.0;
    }
    if (lookupTable_) {
        lookupTable_->SetRange(minLoad, maxLoad);
    }

    const int selectedRow = loadingDeviceList_ ? loadingDeviceList_->currentRow() : -1;
    for (int i = 0; i < loadingDevices_.size(); ++i) {
        const LoadingDevice &device = loadingDevices_.at(i);
        if (device.assignedControlPointIndex < 0 || device.assignedControlPointIndex >= controlPoints_.size()) {
            continue;
        }

        const QVector3D position = controlPoints_.at(device.assignedControlPointIndex);
        const bool selected = i == selectedRow;
        const double loadRatio = std::clamp(std::abs(device.currentLoad) / maxAbsLoad, 0.0, 1.0);
        const double markerRadius = (selected ? 0.14 : 0.10) + 0.18 * loadRatio;
        double loadColor[3] = {0.35, 0.72, 1.0};
        if (lookupTable_) {
            lookupTable_->GetColor(device.currentLoad, loadColor);
        }

        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetCenter(position.x(), position.y(), position.z());
        sphere->SetRadius(markerRadius);
        sphere->SetThetaResolution(32);
        sphere->SetPhiResolution(16);

        auto sphereMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        sphereMapper->SetInputConnection(sphere->GetOutputPort());

        auto markerActor = vtkSmartPointer<vtkActor>::New();
        markerActor->SetMapper(sphereMapper);
        if (!device.enabled) {
            markerActor->GetProperty()->SetColor(0.42, 0.45, 0.48);
        } else {
            markerActor->GetProperty()->SetColor(loadColor);
        }
        markerActor->GetProperty()->SetAmbient(selected ? 0.55 : 0.35);
        markerActor->GetProperty()->SetDiffuse(0.75);
        markerActor->GetProperty()->SetSpecular(selected ? 0.55 : 0.25);
        markerActor->GetProperty()->SetSpecularPower(22);
        renderer_->AddActor(markerActor);
        loadingDeviceActors_.push_back(markerActor);

        QVector3D direction = device.direction;
        if (direction.lengthSquared() < 1e-8f) {
            direction = QVector3D(0.0f, 0.0f, -1.0f);
        }
        direction.normalize();

        double axis[3] = {0.0, 0.0, 1.0};
        const double xAxis[3] = {1.0, 0.0, 0.0};
        const double target[3] = {direction.x(), direction.y(), direction.z()};
        vtkMath::Cross(xAxis, target, axis);
        double angle = vtkMath::DegreesFromRadians(std::acos(std::clamp(vtkMath::Dot(xAxis, target), -1.0, 1.0)));
        if (vtkMath::Norm(axis) < 1e-8) {
            axis[0] = 0.0;
            axis[1] = 0.0;
            axis[2] = 1.0;
            if (target[0] < 0.0) {
                angle = 180.0;
            } else {
                angle = 0.0;
            }
        }

        auto arrow = vtkSmartPointer<vtkArrowSource>::New();
        arrow->SetShaftRadius(0.035);
        arrow->SetTipRadius(0.12);
        arrow->SetTipLength(0.28);

        auto transform = vtkSmartPointer<vtkTransform>::New();
        transform->Translate(position.x(), position.y(), position.z());
        transform->RotateWXYZ(angle, axis);
        transform->Scale(arrowLength, arrowLength, arrowLength);

        auto arrowFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        arrowFilter->SetTransform(transform);
        arrowFilter->SetInputConnection(arrow->GetOutputPort());

        auto arrowMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        arrowMapper->SetInputConnection(arrowFilter->GetOutputPort());

        auto arrowActor = vtkSmartPointer<vtkActor>::New();
        arrowActor->SetMapper(arrowMapper);
        if (!device.enabled) {
            arrowActor->GetProperty()->SetColor(0.35, 0.35, 0.35);
        } else {
            arrowActor->GetProperty()->SetColor(loadColor);
        }
        arrowActor->GetProperty()->SetAmbient(0.30);
        arrowActor->GetProperty()->SetDiffuse(0.80);
        renderer_->AddActor(arrowActor);
        loadingDirectionActors_.push_back(arrowActor);

        auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        label->SetInput(device.id.toUtf8().constData());
        label->SetPosition(position.x() + 0.16, position.y() + 0.16, position.z() + 0.20);
        label->GetTextProperty()->SetColor(selected ? 1.0 : 0.95, selected ? 0.72 : 0.90, selected ? 0.30 : 0.90);
        label->GetTextProperty()->SetFontSize(18);
        renderer_->AddActor(label);
        loadingDeviceLabelActors_.push_back(label);
    }

    renderWindow_->Render();
}
void MainWindow::refreshLoadingDeviceList()
{
    const int selectedRow = loadingDeviceList_ ? loadingDeviceList_->currentRow() : -1;
    loadingDeviceList_->clear();
    int placedCount = 0;
    for (int i = 0; i < loadingDevices_.size(); ++i) {
        const LoadingDevice &device = loadingDevices_.at(i);
        const bool placed = device.assignedControlPointIndex >= 0 && device.assignedControlPointIndex < controlPoints_.size();
        if (placed) {
            ++placedCount;
        }
        const QString state = placed
            ? QString("placed P%1  mesh node %2").arg(device.assignedControlPointIndex + 1).arg(device.meshNodeId >= 0 ? device.meshNodeId + 1 : -1)
            : QString("pending");
        loadingDeviceList_->addItem(QString("%1  %2 %3  %4")
            .arg(device.id)
            .arg(device.currentLoad, 0, 'f', 2)
            .arg(device.unit)
            .arg(state));
    }
    if (selectedRow >= 0 && selectedRow < loadingDeviceList_->count()) {
        loadingDeviceList_->setCurrentRow(selectedRow);
    }
    Q_UNUSED(placedCount);
    refreshWorkflowStatus();
}
void MainWindow::onLoadingDeviceSelectionChanged()
{
    refreshControlPointActors();
    refreshLoadingDeviceActors();
}
