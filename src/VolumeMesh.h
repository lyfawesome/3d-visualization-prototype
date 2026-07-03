#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>

struct VolumeNode {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct VolumeTetrahedron {
    int a = -1;
    int b = -1;
    int c = -1;
    int d = -1;
};

struct VolumeBoundaryFace {
    int a = -1;
    int b = -1;
    int c = -1;
    int marker = 0;
};

struct VolumeMesh {
    QVector<VolumeNode> nodes;
    QVector<VolumeTetrahedron> tetrahedra;
    QVector<VolumeBoundaryFace> boundaryFaces;
    QMap<int, QSet<int>> constraintNodeIdsByMarker;
    QString nodePath;
    QString elementPath;
    QString vtkPath;
    QString boundaryVtkPath;
};
