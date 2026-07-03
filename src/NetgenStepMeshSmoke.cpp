#include "NetgenVolumeMesher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

#include <stdexcept>

namespace {

void validateMesh(const VolumeMesh &mesh, int constraintMarker, const QString &label)
{
    if (mesh.nodes.isEmpty()) {
        throw std::runtime_error(QString("%1 produced no nodes.").arg(label).toStdString());
    }
    if (mesh.tetrahedra.isEmpty()) {
        throw std::runtime_error(QString("%1 produced no tetrahedra.").arg(label).toStdString());
    }
    if (mesh.boundaryFaces.isEmpty()) {
        throw std::runtime_error(QString("%1 produced no boundary faces.").arg(label).toStdString());
    }
    if (!mesh.constraintNodeIdsByMarker.contains(constraintMarker)
        || mesh.constraintNodeIdsByMarker.value(constraintMarker).isEmpty()) {
        throw std::runtime_error(QString("%1 did not preserve the requested constraint marker.").arg(label).toStdString());
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QString stepPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("samples/solid.step");
    const QString workDir = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("solver_runs/netgen_smoke");
    const int constraintMarker = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 1;

    try {
        const QString netgenPath = NetgenVolumeMesher::findNetgenExecutable();
        if (netgenPath.isEmpty()) {
            out << "Netgen STEP mesh smoke skipped: no Netgen backend was found.\n";
            out << "Run scripts/download_netgen_windows.ps1, set NETGEN_EXE, or set NETGEN_PYTHON.\n";
            return 0;
        }

        QDir().mkpath(workDir);
        const VolumeMesh mesh = NetgenVolumeMesher::meshStepFile(stepPath, workDir, {constraintMarker});
        validateMesh(mesh, constraintMarker, QStringLiteral("Netgen"));
        out << "Netgen smoke passed\n";
        out << "Nodes: " << mesh.nodes.size() << "\n";
        out << "Tetrahedra: " << mesh.tetrahedra.size() << "\n";
        out << "Constraint nodes: " << mesh.constraintNodeIdsByMarker.value(constraintMarker).size() << "\n";
        out << "VTK: " << mesh.vtkPath << "\n";
        out << "BOUNDARY_VTK: " << mesh.boundaryVtkPath << "\n";

        return 0;
    } catch (const std::exception &exception) {
        err << "Netgen STEP mesh smoke failed: " << exception.what() << "\n";
        return 1;
    }
}
