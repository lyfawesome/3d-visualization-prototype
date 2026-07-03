#include "MainWindow.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <vtkActor.h>
#include <vtkDoubleArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkScalarBarActor.h>

void MainWindow::applyContour()
{
    const QVector<const LoadingDevice *> contourDevices = activeContourDevices();
    if (modelData_) {
        modelData_->GetPointData()->SetScalars(nullptr);
        modelData_->Modified();
    }
    if (modelMapper_) {
        modelMapper_->ScalarVisibilityOff();
        modelMapper_->Update();
    }

    if (!meshBoundaryData_ || !meshBoundaryMapper_ || contourDevices.isEmpty()) {
        if (meshBoundaryData_) {
            meshBoundaryData_->GetPointData()->SetScalars(nullptr);
            meshBoundaryData_->Modified();
        }
        if (meshBoundaryMapper_) {
            meshBoundaryMapper_->ScalarVisibilityOff();
            meshBoundaryMapper_->Update();
        }
        renderWindow_->Render();
        return;
    }

    auto scalars = vtkSmartPointer<vtkDoubleArray>::New();
    scalars->SetName("InterpolatedLoad");
    scalars->SetNumberOfComponents(1);

    for (vtkIdType i = 0; i < meshBoundaryData_->GetNumberOfPoints(); ++i) {
        double point[3];
        meshBoundaryData_->GetPoint(i, point);
        scalars->InsertNextTuple1(interpolateLoad(point[0], point[1], point[2]));
    }

    meshBoundaryData_->GetPointData()->SetScalars(scalars);
    meshBoundaryData_->Modified();

    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();
    for (const LoadingDevice *device : contourDevices) {
        minValue = std::min(minValue, device->currentLoad);
        maxValue = std::max(maxValue, device->currentLoad);
    }
    if (maxValue <= minValue) {
        maxValue = minValue + 1.0;
    }
    if (scalarBar_) {
        scalarBar_->SetTitle("Load (kN)");
    }
    lookupTable_->SetRange(minValue, maxValue);
    meshBoundaryMapper_->SetLookupTable(lookupTable_);
    meshBoundaryMapper_->ScalarVisibilityOn();
    meshBoundaryMapper_->SetScalarRange(minValue, maxValue);
    meshBoundaryMapper_->Update();
    if (meshBoundaryActor_) {
        meshBoundaryActor_->SetVisibility(true);
    }

    renderWindow_->Render();
}
QVector<const LoadingDevice *> MainWindow::activeContourDevices() const
{
    QVector<const LoadingDevice *> devices;
    for (const LoadingDevice &device : loadingDevices_) {
        if (!device.enabled) {
            continue;
        }
        if (device.meshNodeId < 0 || device.meshNodeId >= volumeMesh_.nodes.size()) {
            continue;
        }
        devices.push_back(&device);
    }
    return devices;
}
double MainWindow::interpolateLoad(double x, double y, double z) const
{
    double weightedSum = 0.0;
    double weightTotal = 0.0;

    const QVector<const LoadingDevice *> contourDevices = activeContourDevices();
    if (!contourDevices.isEmpty()) {
        for (const LoadingDevice *device : contourDevices) {
            const VolumeNode &node = volumeMesh_.nodes.at(device->meshNodeId);
            const double dx = x - node.x;
            const double dy = y - node.y;
            const double dz = z - node.z;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < 1e-6) {
                return device->currentLoad;
            }
            const double weight = 1.0 / (distance * distance);
            weightedSum += device->currentLoad * weight;
            weightTotal += weight;
        }
        return weightTotal > 0.0 ? weightedSum / weightTotal : 0.0;
    }

    return 0.0;
}
