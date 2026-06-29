#pragma once

#include <QString>
#include <QVector3D>

struct Sensor {
    QString id;
    QString name;
    QVector3D position;
    QString type;
    QString unit;
    double value = 0.0;
};
