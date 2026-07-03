#include "MainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>

#include <vtkActor.h>
#include <vtkCellPicker.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>

namespace {

QString projectFileName()
{
    return QStringLiteral("project.vpproj");
}

bool sameFilePath(const QString &a, const QString &b)
{
    const QFileInfo aInfo(a);
    const QFileInfo bInfo(b);
    const QString aCanonical = aInfo.canonicalFilePath();
    const QString bCanonical = bInfo.canonicalFilePath();
    if (!aCanonical.isEmpty() && !bCanonical.isEmpty()) {
        return QDir::cleanPath(aCanonical).compare(QDir::cleanPath(bCanonical), Qt::CaseInsensitive) == 0;
    }
    return QDir::cleanPath(aInfo.absoluteFilePath()).compare(QDir::cleanPath(bInfo.absoluteFilePath()), Qt::CaseInsensitive) == 0;
}

} // namespace

bool MainWindow::ensureProjectReady(const QString &operation) const
{
    if (!projectDir_.isEmpty()) {
        return true;
    }

    QMessageBox::warning(
        const_cast<MainWindow *>(this),
        "Project",
        QString("Create or open a project before %1.").arg(operation));
    return false;
}

void MainWindow::newProject()
{
    const QString root = QFileDialog::getExistingDirectory(
        this,
        "Select or create project folder",
        QDir::homePath());
    if (root.isEmpty()) {
        return;
    }

    initializeProject(root);
    clearProjectState();
    if (!saveProjectFile()) {
        return;
    }

    updateProjectUi();
    statusBar()->showMessage("Project created: " + projectDir_);
}

void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open project",
        QDir::homePath(),
        "Visualization project (*.vpproj)");
    if (path.isEmpty()) {
        return;
    }

    if (loadProjectFile(path)) {
        statusBar()->showMessage("Project opened: " + projectDir_);
    }
}

void MainWindow::initializeProject(const QString &projectDir)
{
    QDir dir(projectDir);
    dir.mkpath(QStringLiteral("."));
    projectDir_ = QDir::cleanPath(dir.absolutePath());
    projectFilePath_ = dir.absoluteFilePath(projectFileName());
    projectName_ = QFileInfo(projectDir_).fileName();
    if (projectName_.isEmpty()) {
        projectName_ = QStringLiteral("Project");
    }
    projectModelRelativePath_.clear();
    projectLoadingDevicesRelativePath_.clear();

    projectSubdir(QStringLiteral("models"));
    projectSubdir(QStringLiteral("inputs"));
    projectSubdir(QStringLiteral("meshes/netgen"));
    projectSubdir(QStringLiteral("solver"));
    projectSubdir(QStringLiteral("results"));
    projectSubdir(QStringLiteral("exports"));
}

bool MainWindow::loadProjectFile(const QString &projectFilePath)
{
    QFile file(projectFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Project", "Could not open project file.");
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        QMessageBox::warning(this, "Project", "Project file must contain a JSON object.");
        return false;
    }

    const QJsonObject object = document.object();
    const QFileInfo projectInfo(projectFilePath);
    initializeProject(projectInfo.absolutePath());
    projectFilePath_ = projectInfo.absoluteFilePath();
    projectName_ = object.value(QStringLiteral("name")).toString(projectName_);
    projectModelRelativePath_ = object.value(QStringLiteral("modelPath")).toString();
    projectLoadingDevicesRelativePath_ = object.value(QStringLiteral("loadingDevicesPath")).toString();

    clearProjectState();

    if (!projectModelRelativePath_.isEmpty()) {
        const QString modelPath = QDir(projectDir_).absoluteFilePath(projectModelRelativePath_);
        if (QFile::exists(modelPath)) {
            loadModel(modelPath);
        }
    }
    if (!projectLoadingDevicesRelativePath_.isEmpty()) {
        const QString loadingPath = QDir(projectDir_).absoluteFilePath(projectLoadingDevicesRelativePath_);
        if (QFile::exists(loadingPath)) {
            loadLoadingDevices(loadingPath);
        }
    }

    updateProjectUi();
    return true;
}

bool MainWindow::saveProjectFile() const
{
    if (projectDir_.isEmpty() || projectFilePath_.isEmpty()) {
        return false;
    }

    QFile file(projectFilePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(const_cast<MainWindow *>(this), "Project", "Could not write project file.");
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("format"), QStringLiteral("VisualizationPrototypeProject"));
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("name"), projectName_);
    object.insert(QStringLiteral("modelPath"), projectModelRelativePath_);
    object.insert(QStringLiteral("loadingDevicesPath"), projectLoadingDevicesRelativePath_);
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

QString MainWindow::projectSubdir(const QString &relativePath) const
{
    if (projectDir_.isEmpty()) {
        return QString();
    }

    QDir dir(projectDir_);
    if (!dir.mkpath(relativePath)) {
        return QString();
    }
    return dir.absoluteFilePath(relativePath);
}

QString MainWindow::copyFileIntoProject(const QString &sourcePath, const QString &relativeSubdir) const
{
    const QString targetDir = projectSubdir(relativeSubdir);
    if (targetDir.isEmpty()) {
        return QString();
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString targetPath = QDir(targetDir).absoluteFilePath(sourceInfo.fileName());
    if (sameFilePath(sourcePath, targetPath)) {
        return targetPath;
    }

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        QMessageBox::warning(const_cast<MainWindow *>(this), "Project", "Could not replace project file: " + targetPath);
        return QString();
    }
    if (!QFile::copy(sourcePath, targetPath)) {
        QMessageBox::warning(const_cast<MainWindow *>(this), "Project", "Could not copy file into project: " + sourcePath);
        return QString();
    }
    return targetPath;
}

QString MainWindow::projectRelativePath(const QString &absolutePath) const
{
    if (projectDir_.isEmpty() || absolutePath.isEmpty()) {
        return QString();
    }
    return QDir(projectDir_).relativeFilePath(QFileInfo(absolutePath).absoluteFilePath());
}

void MainWindow::updateProjectUi()
{
    if (projectLabel_) {
        projectLabel_->setText(projectDir_.isEmpty()
            ? QStringLiteral("Project: not created")
            : QString("Project: %1\nFolder: %2").arg(projectName_, projectDir_));
    }
    setWindowTitle(projectDir_.isEmpty()
        ? QStringLiteral("3D Specimen Visualization Prototype")
        : QString("3D Specimen Visualization Prototype - %1").arg(projectName_));
    refreshWorkflowStatus();
}

void MainWindow::clearProjectState()
{
    if (modelActor_) {
        renderer_->RemoveActor(modelActor_);
        modelActor_ = nullptr;
        modelMapper_ = nullptr;
        modelData_ = nullptr;
    }
    if (meshBoundaryActor_) {
        renderer_->RemoveActor(meshBoundaryActor_);
        meshBoundaryActor_ = nullptr;
        meshBoundaryData_ = nullptr;
        meshBoundaryMapper_ = nullptr;
    }
    if (modelPicker_) {
        modelPicker_->InitializePickList();
    }

    modelPath_.clear();
    volumeMesh_ = VolumeMesh();
    loadingDevices_.clear();
    controlPoints_.clear();
    clearConstraintFaceGroups();
    clearControlPointAssignments();

    refreshControlPointActors();
    refreshConstraintFaceActors();
    refreshLoadingDeviceActors();
    refreshConstraintFaceList();
    refreshControlPointList();
    refreshLoadingDeviceList();

    if (modelLabel_) {
        modelLabel_->setText(QStringLiteral("Model: not loaded"));
    }
    if (workflowLabel_) {
        workflowLabel_->clear();
    }
    if (valueLabel_) {
        valueLabel_->setText(QStringLiteral("Load range: --"));
    }
    refreshWorkflowStatus();
    renderWindow_->Render();
}
