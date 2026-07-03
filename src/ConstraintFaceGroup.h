#pragma once

#include <QString>
#include <QVector>

struct ConstraintFaceGroup {
    int id = -1;
    QString name;
    QVector<int> sourceFaceIds;
    bool enabled = true;
    double ux = 0.0;
    double uy = 0.0;
    double uz = 0.0;
};
