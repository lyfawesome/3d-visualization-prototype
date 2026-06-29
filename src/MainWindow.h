#pragma once

#include "Sensor.h"

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QVector>

#include <memory>

#include <vtkSmartPointer.h>

class QLabel;
class QListWidget;
class QVTKOpenGLNativeWidget;

class vtkActor;
class vtkBillboardTextActor3D;
class vtkGenericOpenGLRenderWindow;
class vtkLookupTable;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkRenderer;
class vtkScalarBarActor;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openModel();
    void openSensors();
    void openLoadData();
    void togglePlayback();
    void resetView();
    void exportPng();
    void advanceFrame();

private:
    void buildUi();
    void buildScene();
    void loadDemoFiles();
    void loadModel(const QString &filePath);
    vtkSmartPointer<vtkPolyData> loadStepModel(const QString &filePath);
    void loadSensors(const QString &filePath);
    void loadLoadData(const QString &filePath);
    void refreshSensorActors();
    void refreshSensorList();
    void applyFrame(int index);
    void applyContour();
    double interpolateLoad(double x, double y, double z) const;
    QString samplePath(const QString &fileName) const;

    QVTKOpenGLNativeWidget *vtkWidget_ = nullptr;
    QLabel *modelLabel_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QLabel *valueLabel_ = nullptr;
    QListWidget *sensorList_ = nullptr;

    QTimer playbackTimer_;
    bool isPlaying_ = false;
    int currentRow_ = 0;

    QString modelPath_;
    QVector<Sensor> sensors_;
    QVector<QMap<QString, double>> loadRows_;

    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow_;
    vtkSmartPointer<vtkRenderer> renderer_;
    vtkSmartPointer<vtkLookupTable> lookupTable_;
    vtkSmartPointer<vtkPolyData> modelData_;
    vtkSmartPointer<vtkPolyDataMapper> modelMapper_;
    vtkSmartPointer<vtkActor> modelActor_;
    vtkSmartPointer<vtkScalarBarActor> scalarBar_;
    QVector<vtkSmartPointer<vtkActor>> sensorActors_;
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> labelActors_;
};
