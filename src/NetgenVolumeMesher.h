#pragma once

#include "VolumeMesh.h"

#include <QString>
#include <QVector>

class NetgenVolumeMesher {
public:
    static QString findNetgenExecutable();

    static VolumeMesh meshStepFile(
        const QString &stepFilePath,
        const QString &workDirectory,
        const QVector<int> &constraintFaceIds = {},
        const QString &netgenExecutable = QString(),
        const QString &netgenOptions = QString());
};
