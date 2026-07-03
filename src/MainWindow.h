#pragma once

#include "LoadingDevice.h"
#include "ConstraintFaceGroup.h"
#include "VolumeMesh.h"

#include <QMainWindow>
#include <QVector>
#include <QVector3D>

#include <memory>

#include <vtkSmartPointer.h>

class QLabel;
class QListWidget;
class QVTKOpenGLNativeWidget;
class QAction;
class QPushButton;
class QProgressBar;
class QProcess;
class QThread;

class vtkActor;
class vtkBillboardTextActor3D;
class vtkCallbackCommand;
class vtkCellPicker;
class vtkGenericOpenGLRenderWindow;
class vtkLookupTable;
class vtkOrientationMarkerWidget;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkRenderer;
class vtkScalarBarActor;
class vtkObject;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void newProject();
    void openProject();
    void openModel();
    void openLoadingDevices();
    void generateVolumeMesh();
    void exportCalculixInput();
    void runCalculixSolver();
    void openCalculixResult();
    void resetView();
    void exportPng();
    void clearControlPoints();
    void onLoadingDeviceSelectionChanged();

private:
    void buildUi();
    void buildScene();
    void refreshWorkflowStatus();
    void startBackgroundProgress(const QString &message);
    void finishBackgroundProgress(const QString &message);
    bool ensureProjectReady(const QString &operation) const;
    void initializeProject(const QString &projectDir);
    bool loadProjectFile(const QString &projectFilePath);
    bool saveProjectFile() const;
    QString projectSubdir(const QString &relativePath) const;
    QString copyFileIntoProject(const QString &sourcePath, const QString &relativeSubdir) const;
    QString projectRelativePath(const QString &absolutePath) const;
    void updateProjectUi();
    void clearProjectState();
    void loadModel(const QString &filePath);
    vtkSmartPointer<vtkPolyData> loadStepModel(const QString &filePath);
    void loadLoadingDevices(const QString &filePath);
    void showVolumeMesh(const QString &sourceName);
    void showImportedModelOnly();
    void showGeneratedMeshOnly();
    void refreshLoadingDeviceActors();
    void refreshLoadingDeviceList();
    void refreshConstraintFaceActors();
    void refreshConstraintFaceList();
    void refreshControlPointActors();
    void refreshControlPointList();
    void applyContour();
    QVector<const LoadingDevice *> activeContourDevices() const;
    bool handleLeftButtonPress();
    void addConstraintFaceFromCell(vtkIdType cellId);
    void addControlPoint(double x, double y, double z, const QVector3D &normal, int meshNodeId = -1);
    bool writeCalculixInput(const QString &filePath) const;
    bool applyCalculixResult(const QString &filePath);
    int nearestModelPointId(double x, double y, double z) const;
    int nearestMeshBoundaryNodeId(double x, double y, double z) const;
    int sourceFaceIdForCell(vtkIdType cellId) const;
    int nextUnassignedLoadingDeviceIndex() const;
    void clearControlPointAssignments();
    void clearConstraintFaceGroups();
    QVector<int> selectedConstraintFaceIds() const;
    double interpolateLoad(double x, double y, double z) const;
    QString samplePath(const QString &fileName) const;
    static void onVtkLeftButtonPress(vtkObject *caller, unsigned long eventId, void *clientData, void *callData);

    QVTKOpenGLNativeWidget *vtkWidget_ = nullptr;
    QAction *pickControlPointAction_ = nullptr;
    QAction *pickConstraintFaceAction_ = nullptr;
    QLabel *modelLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *workflowLabel_ = nullptr;
    QLabel *configStatusLabel_ = nullptr;
    QLabel *projectStepStatusLabel_ = nullptr;
    QLabel *modelStepStatusLabel_ = nullptr;
    QLabel *constraintStepStatusLabel_ = nullptr;
    QLabel *meshStepStatusLabel_ = nullptr;
    QLabel *loadStepStatusLabel_ = nullptr;
    QLabel *exportStepStatusLabel_ = nullptr;
    QLabel *resultStepStatusLabel_ = nullptr;
    QLabel *valueLabel_ = nullptr;
    QListWidget *loadingDeviceList_ = nullptr;
    QListWidget *fixedNodeList_ = nullptr;
    QPushButton *openModelButton_ = nullptr;
    QPushButton *generateMeshButton_ = nullptr;
    QPushButton *loadPointsButton_ = nullptr;
    QPushButton *exportInpButton_ = nullptr;
    QPushButton *runCalculixButton_ = nullptr;
    QPushButton *openResultButton_ = nullptr;
    QLabel *backgroundProgressLabel_ = nullptr;
    QProgressBar *backgroundProgressBar_ = nullptr;

    QString projectDir_;
    QString projectFilePath_;
    QString projectName_;
    QString projectModelRelativePath_;
    QString projectLoadingDevicesRelativePath_;
    QString modelPath_;
    QString solverOutput_;
    QVector<LoadingDevice> loadingDevices_;
    QVector<ConstraintFaceGroup> constraintFaceGroups_;
    VolumeMesh volumeMesh_;
    QVector<QVector3D> controlPoints_;

    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow_;
    vtkSmartPointer<vtkRenderer> renderer_;
    vtkSmartPointer<vtkLookupTable> lookupTable_;
    vtkSmartPointer<vtkPolyData> modelData_;
    vtkSmartPointer<vtkPolyDataMapper> modelMapper_;
    vtkSmartPointer<vtkActor> modelActor_;
    vtkSmartPointer<vtkPolyData> meshBoundaryData_;
    vtkSmartPointer<vtkPolyDataMapper> meshBoundaryMapper_;
    vtkSmartPointer<vtkActor> meshBoundaryActor_;
    vtkSmartPointer<vtkScalarBarActor> scalarBar_;
    vtkSmartPointer<vtkCellPicker> modelPicker_;
    vtkSmartPointer<vtkCallbackCommand> leftButtonPressCallback_;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationMarker_;
    QVector<vtkSmartPointer<vtkActor>> loadingDeviceActors_;
    QVector<vtkSmartPointer<vtkActor>> loadingDirectionActors_;
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> loadingDeviceLabelActors_;
    QVector<vtkSmartPointer<vtkActor>> constraintFaceActors_;
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> constraintFaceLabelActors_;
    QVector<vtkSmartPointer<vtkActor>> controlPointActors_;
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> controlPointLabelActors_;
    QThread *meshWorker_ = nullptr;
    QProcess *solverProcess_ = nullptr;
    bool meshJobRunning_ = false;
    bool solverJobRunning_ = false;
};
