#include "MainWindow.h"

#include <QListWidget>
#include <QMap>
#include <QSet>
#include <QStatusBar>

#include <algorithm>
#include <cmath>

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCamera.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkIdList.h>
#include <vtkMapper.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

namespace {

struct OutlineEdge {
    vtkIdType a = -1;
    vtkIdType b = -1;
    int count = 0;
    double normal[3] = {0.0, 0.0, 1.0};
};

bool normalizedCellNormal(vtkPolyData *data, vtkCell *cell, double normal[3], double centroid[3])
{
    normal[0] = 0.0;
    normal[1] = 0.0;
    normal[2] = 1.0;
    centroid[0] = 0.0;
    centroid[1] = 0.0;
    centroid[2] = 0.0;

    if (!data || !cell || cell->GetNumberOfPoints() < 3) {
        return false;
    }

    double p0[3];
    double p1[3];
    double p2[3];
    data->GetPoint(cell->GetPointId(0), p0);
    data->GetPoint(cell->GetPointId(1), p1);
    data->GetPoint(cell->GetPointId(2), p2);

    const double ux = p1[0] - p0[0];
    const double uy = p1[1] - p0[1];
    const double uz = p1[2] - p0[2];
    const double vx = p2[0] - p0[0];
    const double vy = p2[1] - p0[1];
    const double vz = p2[2] - p0[2];
    normal[0] = uy * vz - uz * vy;
    normal[1] = uz * vx - ux * vz;
    normal[2] = ux * vy - uy * vx;

    const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length <= 1e-12) {
        normal[0] = 0.0;
        normal[1] = 0.0;
        normal[2] = 1.0;
        return false;
    }
    normal[0] /= length;
    normal[1] /= length;
    normal[2] /= length;

    for (vtkIdType i = 0; i < cell->GetNumberOfPoints(); ++i) {
        double point[3];
        data->GetPoint(cell->GetPointId(i), point);
        centroid[0] += point[0];
        centroid[1] += point[1];
        centroid[2] += point[2];
    }
    const double count = double(cell->GetNumberOfPoints());
    centroid[0] /= count;
    centroid[1] /= count;
    centroid[2] /= count;
    return true;
}

void flipNormalTowardCamera(vtkRenderer *renderer, const double centroid[3], double normal[3])
{
    if (!renderer || !renderer->GetActiveCamera()) {
        return;
    }
    double cameraPosition[3];
    renderer->GetActiveCamera()->GetPosition(cameraPosition);
    const double toCamera[3] = {
        cameraPosition[0] - centroid[0],
        cameraPosition[1] - centroid[1],
        cameraPosition[2] - centroid[2]
    };
    const double dot = normal[0] * toCamera[0] + normal[1] * toCamera[1] + normal[2] * toCamera[2];
    if (dot < 0.0) {
        normal[0] = -normal[0];
        normal[1] = -normal[1];
        normal[2] = -normal[2];
    }
}

QString edgeKey(vtkIdType a, vtkIdType b)
{
    const vtkIdType first = std::min(a, b);
    const vtkIdType second = std::max(a, b);
    return QString("%1:%2").arg(first).arg(second);
}

bool normalizedTriangleNormal(const VolumeNode &a, const VolumeNode &b, const VolumeNode &c, double normal[3], double centroid[3])
{
    centroid[0] = (a.x + b.x + c.x) / 3.0;
    centroid[1] = (a.y + b.y + c.y) / 3.0;
    centroid[2] = (a.z + b.z + c.z) / 3.0;

    const double ux = b.x - a.x;
    const double uy = b.y - a.y;
    const double uz = b.z - a.z;
    const double vx = c.x - a.x;
    const double vy = c.y - a.y;
    const double vz = c.z - a.z;
    normal[0] = uy * vz - uz * vy;
    normal[1] = uz * vx - ux * vz;
    normal[2] = ux * vy - uy * vx;

    const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length <= 1e-12) {
        normal[0] = 0.0;
        normal[1] = 0.0;
        normal[2] = 1.0;
        return false;
    }
    normal[0] /= length;
    normal[1] /= length;
    normal[2] /= length;
    return true;
}

} // namespace

int MainWindow::sourceFaceIdForCell(vtkIdType cellId) const
{
    if (!modelData_ || cellId < 0 || cellId >= modelData_->GetNumberOfCells()) {
        return -1;
    }

    vtkDataArray *sourceFaceIds = modelData_->GetCellData()->GetArray("SourceFaceId");
    if (!sourceFaceIds) {
        return -1;
    }
    return int(sourceFaceIds->GetTuple1(cellId));
}

void MainWindow::addConstraintFaceFromCell(vtkIdType cellId)
{
    const int sourceFaceId = sourceFaceIdForCell(cellId);
    if (sourceFaceId <= 0) {
        statusBar()->showMessage("Constraint face picking is only available on STEP/STP models.");
        return;
    }

    for (const ConstraintFaceGroup &group : constraintFaceGroups_) {
        if (group.sourceFaceIds.contains(sourceFaceId)) {
            statusBar()->showMessage(QString("Constraint face F%1 is already selected.").arg(sourceFaceId));
            return;
        }
    }

    ConstraintFaceGroup group;
    group.id = sourceFaceId;
    group.name = QString("Face %1").arg(sourceFaceId);
    group.sourceFaceIds.push_back(sourceFaceId);
    constraintFaceGroups_.push_back(group);

    volumeMesh_ = VolumeMesh();
    refreshConstraintFaceActors();
    refreshConstraintFaceList();
    refreshWorkflowStatus();
    statusBar()->showMessage(QString("Selected constraint face F%1. Regenerate the Netgen mesh before solving.").arg(sourceFaceId));
}

void MainWindow::refreshConstraintFaceActors()
{
    for (const auto &actor : constraintFaceActors_) {
        renderer_->RemoveActor(actor);
    }
    for (const auto &label : constraintFaceLabelActors_) {
        renderer_->RemoveActor(label);
    }
    constraintFaceActors_.clear();
    constraintFaceLabelActors_.clear();

    if (constraintFaceGroups_.isEmpty()) {
        renderWindow_->Render();
        return;
    }

    if (!volumeMesh_.nodes.isEmpty() && !volumeMesh_.boundaryFaces.isEmpty()) {
        double bounds[6] = {
            volumeMesh_.nodes.first().x,
            volumeMesh_.nodes.first().x,
            volumeMesh_.nodes.first().y,
            volumeMesh_.nodes.first().y,
            volumeMesh_.nodes.first().z,
            volumeMesh_.nodes.first().z
        };
        for (const VolumeNode &node : volumeMesh_.nodes) {
            bounds[0] = std::min(bounds[0], node.x);
            bounds[1] = std::max(bounds[1], node.x);
            bounds[2] = std::min(bounds[2], node.y);
            bounds[3] = std::max(bounds[3], node.y);
            bounds[4] = std::min(bounds[4], node.z);
            bounds[5] = std::max(bounds[5], node.z);
        }
        const double dx = bounds[1] - bounds[0];
        const double dy = bounds[3] - bounds[2];
        const double dz = bounds[5] - bounds[4];
        const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double faceOffset = std::max(diagonal * 0.0012, 1e-4);
        const double wireOffset = std::max(diagonal * 0.0018, 1.5e-4);
        const double labelOffset = std::max(diagonal * 0.018, 0.08);

        vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
        vtkMapper::SetResolveCoincidentTopologyPolygonOffsetParameters(-1.0, -2.0);

        for (const ConstraintFaceGroup &group : constraintFaceGroups_) {
            if (!group.enabled || group.sourceFaceIds.isEmpty()) {
                continue;
            }

            const QSet<int> selectedIds(group.sourceFaceIds.begin(), group.sourceFaceIds.end());
            auto fillPoints = vtkSmartPointer<vtkPoints>::New();
            auto fillCells = vtkSmartPointer<vtkCellArray>::New();
            auto wirePoints = vtkSmartPointer<vtkPoints>::New();
            auto wireCells = vtkSmartPointer<vtkCellArray>::New();
            QMap<QString, OutlineEdge> outlineEdges;
            double labelPosition[3] = {0.0, 0.0, 0.0};
            double labelNormal[3] = {0.0, 0.0, 0.0};
            int labelPointCount = 0;
            int labelNormalCount = 0;

            for (const VolumeBoundaryFace &face : volumeMesh_.boundaryFaces) {
                if (!selectedIds.contains(face.marker)
                    || face.a < 0 || face.a >= volumeMesh_.nodes.size()
                    || face.b < 0 || face.b >= volumeMesh_.nodes.size()
                    || face.c < 0 || face.c >= volumeMesh_.nodes.size()) {
                    continue;
                }

                const VolumeNode &a = volumeMesh_.nodes.at(face.a);
                const VolumeNode &b = volumeMesh_.nodes.at(face.b);
                const VolumeNode &c = volumeMesh_.nodes.at(face.c);
                double normal[3];
                double centroid[3];
                normalizedTriangleNormal(a, b, c, normal, centroid);
                flipNormalTowardCamera(renderer_, centroid, normal);
                labelNormal[0] += normal[0];
                labelNormal[1] += normal[1];
                labelNormal[2] += normal[2];
                ++labelNormalCount;

                const int nodeIds[3] = {face.a, face.b, face.c};
                vtkIdType fillIds[3];
                for (int i = 0; i < 3; ++i) {
                    const VolumeNode &node = volumeMesh_.nodes.at(nodeIds[i]);
                    fillIds[i] = fillPoints->InsertNextPoint(
                        node.x + normal[0] * faceOffset,
                        node.y + normal[1] * faceOffset,
                        node.z + normal[2] * faceOffset);
                    labelPosition[0] += node.x;
                    labelPosition[1] += node.y;
                    labelPosition[2] += node.z;
                    ++labelPointCount;
                }
                fillCells->InsertNextCell(3, fillIds);

                for (int i = 0; i < 3; ++i) {
                    const vtkIdType edgeA = nodeIds[i];
                    const vtkIdType edgeB = nodeIds[(i + 1) % 3];
                    const QString key = edgeKey(edgeA, edgeB);
                    OutlineEdge edge = outlineEdges.value(key);
                    if (edge.count == 0) {
                        edge.a = std::min(edgeA, edgeB);
                        edge.b = std::max(edgeA, edgeB);
                        edge.normal[0] = normal[0];
                        edge.normal[1] = normal[1];
                        edge.normal[2] = normal[2];
                    }
                    ++edge.count;
                    outlineEdges.insert(key, edge);
                }
            }

            if (fillCells->GetNumberOfCells() == 0) {
                continue;
            }

            for (auto it = outlineEdges.constBegin(); it != outlineEdges.constEnd(); ++it) {
                const OutlineEdge &edge = it.value();
                if (edge.count != 1 || edge.a < 0 || edge.b < 0
                    || edge.a >= volumeMesh_.nodes.size() || edge.b >= volumeMesh_.nodes.size()) {
                    continue;
                }
                const VolumeNode &a = volumeMesh_.nodes.at(int(edge.a));
                const VolumeNode &b = volumeMesh_.nodes.at(int(edge.b));
                const vtkIdType lineIds[2] = {
                    wirePoints->InsertNextPoint(
                        a.x + edge.normal[0] * wireOffset,
                        a.y + edge.normal[1] * wireOffset,
                        a.z + edge.normal[2] * wireOffset),
                    wirePoints->InsertNextPoint(
                        b.x + edge.normal[0] * wireOffset,
                        b.y + edge.normal[1] * wireOffset,
                        b.z + edge.normal[2] * wireOffset)
                };
                wireCells->InsertNextCell(2, lineIds);
            }

            auto faceData = vtkSmartPointer<vtkPolyData>::New();
            faceData->SetPoints(fillPoints);
            faceData->SetPolys(fillCells);

            auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputData(faceData);
            mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(-2.0, -8.0);

            auto actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            actor->GetProperty()->SetColor(0.08, 0.85, 0.45);
            actor->GetProperty()->SetOpacity(0.30);
            actor->GetProperty()->SetAmbient(0.65);
            actor->GetProperty()->SetDiffuse(0.40);
            actor->GetProperty()->SetSpecular(0.15);
            actor->PickableOff();
            renderer_->AddActor(actor);
            constraintFaceActors_.push_back(actor);

            auto wireData = vtkSmartPointer<vtkPolyData>::New();
            wireData->SetPoints(wirePoints);
            wireData->SetLines(wireCells);

            auto wireMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            wireMapper->SetInputData(wireData);
            wireMapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(-4.0, -12.0);

            auto wireActor = vtkSmartPointer<vtkActor>::New();
            wireActor->SetMapper(wireMapper);
            wireActor->GetProperty()->SetColor(0.0, 1.0, 0.62);
            wireActor->GetProperty()->SetOpacity(0.95);
            wireActor->GetProperty()->SetLineWidth(2.8);
            wireActor->GetProperty()->SetAmbient(0.85);
            wireActor->GetProperty()->SetDiffuse(0.25);
            wireActor->PickableOff();
            renderer_->AddActor(wireActor);
            constraintFaceActors_.push_back(wireActor);

            if (labelPointCount > 0) {
                labelPosition[0] /= double(labelPointCount);
                labelPosition[1] /= double(labelPointCount);
                labelPosition[2] /= double(labelPointCount);
            }
            if (labelNormalCount > 0) {
                labelNormal[0] /= double(labelNormalCount);
                labelNormal[1] /= double(labelNormalCount);
                labelNormal[2] /= double(labelNormalCount);
                const double normalLength = std::sqrt(
                    labelNormal[0] * labelNormal[0]
                    + labelNormal[1] * labelNormal[1]
                    + labelNormal[2] * labelNormal[2]);
                if (normalLength > 1e-12) {
                    labelNormal[0] /= normalLength;
                    labelNormal[1] /= normalLength;
                    labelNormal[2] /= normalLength;
                    labelPosition[0] += labelNormal[0] * labelOffset;
                    labelPosition[1] += labelNormal[1] * labelOffset;
                    labelPosition[2] += labelNormal[2] * labelOffset;
                }
            }

            auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
            const QByteArray labelText = QString("F%1").arg(group.id).toUtf8();
            label->SetInput(labelText.constData());
            label->SetPosition(labelPosition[0], labelPosition[1], labelPosition[2]);
            label->GetTextProperty()->SetColor(0.55, 1.0, 0.72);
            label->GetTextProperty()->SetFontSize(20);
            label->GetTextProperty()->BoldOn();
            renderer_->AddActor(label);
            constraintFaceLabelActors_.push_back(label);
        }

        renderWindow_->Render();
        return;
    }

    if (!modelData_) {
        renderWindow_->Render();
        return;
    }

    vtkDataArray *sourceFaceIds = modelData_->GetCellData()->GetArray("SourceFaceId");
    if (!sourceFaceIds) {
        renderWindow_->Render();
        return;
    }

    double bounds[6];
    modelData_->GetBounds(bounds);
    const double dx = bounds[1] - bounds[0];
    const double dy = bounds[3] - bounds[2];
    const double dz = bounds[5] - bounds[4];
    const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double faceOffset = std::max(diagonal * 0.0012, 1e-4);
    const double wireOffset = std::max(diagonal * 0.0018, 1.5e-4);
    const double labelOffset = std::max(diagonal * 0.018, 0.08);

    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
    vtkMapper::SetResolveCoincidentTopologyPolygonOffsetParameters(-1.0, -2.0);

    for (const ConstraintFaceGroup &group : constraintFaceGroups_) {
        if (!group.enabled || group.sourceFaceIds.isEmpty()) {
            continue;
        }

        const QSet<int> selectedIds(group.sourceFaceIds.begin(), group.sourceFaceIds.end());
        auto fillPoints = vtkSmartPointer<vtkPoints>::New();
        auto fillCells = vtkSmartPointer<vtkCellArray>::New();
        auto wirePoints = vtkSmartPointer<vtkPoints>::New();
        auto wireCells = vtkSmartPointer<vtkCellArray>::New();
        QMap<QString, OutlineEdge> outlineEdges;
        double labelPosition[3] = {0.0, 0.0, 0.0};
        double labelNormal[3] = {0.0, 0.0, 0.0};
        int labelPointCount = 0;
        int labelNormalCount = 0;

        for (vtkIdType cellId = 0; cellId < modelData_->GetNumberOfCells(); ++cellId) {
            const int sourceFaceId = int(sourceFaceIds->GetTuple1(cellId));
            if (!selectedIds.contains(sourceFaceId)) {
                continue;
            }

            vtkCell *cell = modelData_->GetCell(cellId);
            if (!cell || cell->GetNumberOfPoints() < 3) {
                continue;
            }

            double normal[3];
            double centroid[3];
            normalizedCellNormal(modelData_, cell, normal, centroid);
            flipNormalTowardCamera(renderer_, centroid, normal);
            labelNormal[0] += normal[0];
            labelNormal[1] += normal[1];
            labelNormal[2] += normal[2];
            ++labelNormalCount;

            QVector<vtkIdType> fillIds;
            fillIds.reserve(int(cell->GetNumberOfPoints()));
            QVector<vtkIdType> originalIds;
            originalIds.reserve(int(cell->GetNumberOfPoints()));
            for (vtkIdType i = 0; i < cell->GetNumberOfPoints(); ++i) {
                const vtkIdType pointId = cell->GetPointId(i);
                double point[3];
                modelData_->GetPoint(pointId, point);
                fillIds.push_back(fillPoints->InsertNextPoint(
                    point[0] + normal[0] * faceOffset,
                    point[1] + normal[1] * faceOffset,
                    point[2] + normal[2] * faceOffset));
                originalIds.push_back(pointId);
                labelPosition[0] += point[0];
                labelPosition[1] += point[1];
                labelPosition[2] += point[2];
                ++labelPointCount;
            }
            fillCells->InsertNextCell(fillIds.size(), fillIds.data());
            for (int i = 0; i < originalIds.size(); ++i) {
                const vtkIdType a = originalIds.at(i);
                const vtkIdType b = originalIds.at((i + 1) % originalIds.size());
                const QString key = edgeKey(a, b);
                OutlineEdge edge = outlineEdges.value(key);
                if (edge.count == 0) {
                    edge.a = std::min(a, b);
                    edge.b = std::max(a, b);
                    edge.normal[0] = normal[0];
                    edge.normal[1] = normal[1];
                    edge.normal[2] = normal[2];
                }
                ++edge.count;
                outlineEdges.insert(key, edge);
            }
        }

        if (fillCells->GetNumberOfCells() == 0) {
            continue;
        }

        for (auto it = outlineEdges.constBegin(); it != outlineEdges.constEnd(); ++it) {
            const OutlineEdge &edge = it.value();
            if (edge.count != 1 || edge.a < 0 || edge.b < 0) {
                continue;
            }
            double a[3];
            double b[3];
            modelData_->GetPoint(edge.a, a);
            modelData_->GetPoint(edge.b, b);
            const vtkIdType lineIds[2] = {
                wirePoints->InsertNextPoint(
                    a[0] + edge.normal[0] * wireOffset,
                    a[1] + edge.normal[1] * wireOffset,
                    a[2] + edge.normal[2] * wireOffset),
                wirePoints->InsertNextPoint(
                    b[0] + edge.normal[0] * wireOffset,
                    b[1] + edge.normal[1] * wireOffset,
                    b[2] + edge.normal[2] * wireOffset)
            };
            wireCells->InsertNextCell(2, lineIds);
        }

        auto faceData = vtkSmartPointer<vtkPolyData>::New();
        faceData->SetPoints(fillPoints);
        faceData->SetPolys(fillCells);

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(faceData);
        mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(-2.0, -8.0);

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.08, 0.85, 0.45);
        actor->GetProperty()->SetOpacity(0.22);
        actor->GetProperty()->SetAmbient(0.60);
        actor->GetProperty()->SetDiffuse(0.45);
        actor->GetProperty()->SetSpecular(0.15);
        actor->PickableOff();
        renderer_->AddActor(actor);
        constraintFaceActors_.push_back(actor);

        auto wireData = vtkSmartPointer<vtkPolyData>::New();
        wireData->SetPoints(wirePoints);
        wireData->SetLines(wireCells);

        auto wireMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        wireMapper->SetInputData(wireData);
        wireMapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(-4.0, -12.0);

        auto wireActor = vtkSmartPointer<vtkActor>::New();
        wireActor->SetMapper(wireMapper);
        wireActor->GetProperty()->SetColor(0.0, 1.0, 0.62);
        wireActor->GetProperty()->SetOpacity(0.95);
        wireActor->GetProperty()->SetLineWidth(2.4);
        wireActor->GetProperty()->SetAmbient(0.85);
        wireActor->GetProperty()->SetDiffuse(0.25);
        wireActor->PickableOff();
        renderer_->AddActor(wireActor);
        constraintFaceActors_.push_back(wireActor);

        if (labelPointCount > 0) {
            labelPosition[0] /= double(labelPointCount);
            labelPosition[1] /= double(labelPointCount);
            labelPosition[2] /= double(labelPointCount);
        }
        if (labelNormalCount > 0) {
            labelNormal[0] /= double(labelNormalCount);
            labelNormal[1] /= double(labelNormalCount);
            labelNormal[2] /= double(labelNormalCount);
            const double normalLength = std::sqrt(
                labelNormal[0] * labelNormal[0]
                + labelNormal[1] * labelNormal[1]
                + labelNormal[2] * labelNormal[2]);
            if (normalLength > 1e-12) {
                labelNormal[0] /= normalLength;
                labelNormal[1] /= normalLength;
                labelNormal[2] /= normalLength;
                labelPosition[0] += labelNormal[0] * labelOffset;
                labelPosition[1] += labelNormal[1] * labelOffset;
                labelPosition[2] += labelNormal[2] * labelOffset;
            }
        }

        auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        const QByteArray labelText = QString("F%1").arg(group.id).toUtf8();
        label->SetInput(labelText.constData());
        label->SetPosition(labelPosition[0], labelPosition[1], labelPosition[2]);
        label->GetTextProperty()->SetColor(0.55, 1.0, 0.72);
        label->GetTextProperty()->SetFontSize(20);
        label->GetTextProperty()->BoldOn();
        renderer_->AddActor(label);
        constraintFaceLabelActors_.push_back(label);
    }

    renderWindow_->Render();
}

void MainWindow::refreshConstraintFaceList()
{
    if (!fixedNodeList_) {
        return;
    }

    fixedNodeList_->clear();
    for (const ConstraintFaceGroup &group : constraintFaceGroups_) {
        fixedNodeList_->addItem(QString("F%1  fixed UX/UY/UZ=0").arg(group.id));
    }
    refreshWorkflowStatus();
}

void MainWindow::clearConstraintFaceGroups()
{
    constraintFaceGroups_.clear();
}

QVector<int> MainWindow::selectedConstraintFaceIds() const
{
    QVector<int> ids;
    for (const ConstraintFaceGroup &group : constraintFaceGroups_) {
        if (!group.enabled) {
            continue;
        }
        for (int sourceFaceId : group.sourceFaceIds) {
            if (!ids.contains(sourceFaceId)) {
                ids.push_back(sourceFaceId);
            }
        }
    }
    return ids;
}
