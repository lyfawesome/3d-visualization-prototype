#pragma once

#include <QString>
#include <QVector3D>

struct LoadingDevice {
    QString id;
    QString name;
    QVector3D position;
    QVector3D direction = QVector3D(0.0f, 0.0f, -1.0f);
    double currentLoad = 0.0;
    QString unit = "kN";
    bool enabled = true;
    int assignedControlPointIndex = -1;
    int meshNodeId = -1;
};
