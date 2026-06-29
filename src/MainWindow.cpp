#include "MainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include <QVTKOpenGLNativeWidget.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkAlgorithm.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkDoubleArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLookupTable.h>
#include <vtkOBJReader.h>
#include <vtkPNGWriter.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkSphereSource.h>
#include <vtkSTLReader.h>
#include <vtkTextProperty.h>
#include <vtkTriangleFilter.h>
#include <vtkWindowToImageFilter.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("3D Specimen Visualization Prototype");

    playbackTimer_.setInterval(450);
    connect(&playbackTimer_, &QTimer::timeout, this, &MainWindow::advanceFrame);

    buildUi();
    buildScene();
    loadDemoFiles();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    toolbar->addAction("Open Model", this, &MainWindow::openModel);
    toolbar->addAction("Load Sensors", this, &MainWindow::openSensors);
    toolbar->addAction("Load Data", this, &MainWindow::openLoadData);
    toolbar->addAction("Play/Pause", this, &MainWindow::togglePlayback);
    toolbar->addAction("Reset View", this, &MainWindow::resetView);
    toolbar->addAction("Export PNG", this, &MainWindow::exportPng);

    auto *root = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *sidePanel = new QWidget(root);
    sidePanel->setFixedWidth(330);
    auto *sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(14, 14, 14, 14);

    modelLabel_ = new QLabel("Model: not loaded", sidePanel);
    modelLabel_->setWordWrap(true);
    timeLabel_ = new QLabel("Time: 0.00 s", sidePanel);
    valueLabel_ = new QLabel("Load range: --", sidePanel);
    sensorList_ = new QListWidget(sidePanel);

    auto *playButton = new QPushButton("Play / Pause", sidePanel);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    auto *resetButton = new QPushButton("Reset View", sidePanel);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetView);

    sideLayout->addWidget(new QLabel("Model", sidePanel));
    sideLayout->addWidget(modelLabel_);
    sideLayout->addSpacing(10);
    sideLayout->addWidget(new QLabel("Simulation", sidePanel));
    sideLayout->addWidget(timeLabel_);
    sideLayout->addWidget(valueLabel_);
    sideLayout->addWidget(playButton);
    sideLayout->addWidget(resetButton);
    sideLayout->addSpacing(10);
    sideLayout->addWidget(new QLabel("Sensors", sidePanel));
    sideLayout->addWidget(sensorList_, 1);

    vtkWidget_ = new QVTKOpenGLNativeWidget(root);
    rootLayout->addWidget(sidePanel);
    rootLayout->addWidget(vtkWidget_, 1);

    setCentralWidget(root);
    setStatusBar(new QStatusBar(this));
}

void MainWindow::buildScene()
{
    renderWindow_ = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    renderer_ = vtkSmartPointer<vtkRenderer>::New();
    lookupTable_ = vtkSmartPointer<vtkLookupTable>::New();

    lookupTable_->SetNumberOfTableValues(256);
    lookupTable_->SetHueRange(0.66, 0.0);
    lookupTable_->SetSaturationRange(0.95, 0.95);
    lookupTable_->SetValueRange(0.95, 0.95);
    lookupTable_->Build();

    renderWindow_->AddRenderer(renderer_);
    vtkWidget_->setRenderWindow(renderWindow_);
    renderer_->SetBackground(0.08, 0.09, 0.10);

    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(1.2, 1.2, 1.2);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.015);
    renderer_->AddActor(axes);

    scalarBar_ = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar_->SetLookupTable(lookupTable_);
    scalarBar_->SetTitle("Load (kN)");
    scalarBar_->SetNumberOfLabels(5);
    scalarBar_->SetWidth(0.10);
    scalarBar_->SetHeight(0.46);
    scalarBar_->SetPosition(0.88, 0.10);
    renderer_->AddActor2D(scalarBar_);
}

void MainWindow::loadDemoFiles()
{
    loadModel(samplePath("specimen.stl"));
    loadSensors(samplePath("sensors.json"));
    loadLoadData(samplePath("load_data.csv"));
    statusBar()->showMessage("Loaded sample model, sensors, and load data.");
}

QString MainWindow::samplePath(const QString &fileName) const
{
    const QString appSample = QCoreApplication::applicationDirPath() + "/samples/" + fileName;
    if (QFile::exists(appSample)) {
        return appSample;
    }
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../samples/" + fileName);
}

void MainWindow::openModel()
{
    QFileDialog dialog(this, "Open model", samplePath(QString()));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setNameFilters({
        "All supported models (*.step *.STEP *.stp *.STP *.stl *.STL *.obj *.OBJ)",
        "STEP/STP CAD models (*.step *.STEP *.stp *.STP)",
        "STL meshes (*.stl *.STL)",
        "OBJ meshes (*.obj *.OBJ)",
        "All files (*.*)"
    });

    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
        loadModel(dialog.selectedFiles().first());
    }
}

void MainWindow::openSensors()
{
    const QString path = QFileDialog::getOpenFileName(this, "Load sensors", QCoreApplication::applicationDirPath(), "JSON (*.json)");
    if (!path.isEmpty()) {
        loadSensors(path);
    }
}

void MainWindow::openLoadData()
{
    const QString path = QFileDialog::getOpenFileName(this, "Load data", QCoreApplication::applicationDirPath(), "CSV (*.csv)");
    if (!path.isEmpty()) {
        loadLoadData(path);
    }
}

void MainWindow::loadModel(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    QString displayName = QFileInfo(filePath).fileName();
    vtkSmartPointer<vtkPolyData> polyData;

    if (suffix == "step" || suffix == "stp") {
        try {
            polyData = loadStepModel(filePath);
            displayName += " (STEP via OCCT)";
        } catch (const std::exception &ex) {
            QMessageBox::warning(this, "STEP import failed", ex.what());
            return;
        }
    } else if (suffix == "obj") {
        auto objReader = vtkSmartPointer<vtkOBJReader>::New();
        objReader->SetFileName(filePath.toLocal8Bit().constData());
        auto triangle = vtkSmartPointer<vtkTriangleFilter>::New();
        triangle->SetInputConnection(objReader->GetOutputPort());

        auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
        cleaner->SetInputConnection(triangle->GetOutputPort());
        cleaner->Update();

        polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->DeepCopy(cleaner->GetOutput());
    } else if (suffix == "stl") {
        auto stlReader = vtkSmartPointer<vtkSTLReader>::New();
        stlReader->SetFileName(filePath.toLocal8Bit().constData());
        auto triangle = vtkSmartPointer<vtkTriangleFilter>::New();
        triangle->SetInputConnection(stlReader->GetOutputPort());

        auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
        cleaner->SetInputConnection(triangle->GetOutputPort());
        cleaner->Update();

        polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->DeepCopy(cleaner->GetOutput());
    } else {
        QMessageBox::warning(this, "Unsupported model", "Unsupported model format: " + suffix);
        return;
    }

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        QMessageBox::warning(this, "Model error", "The model has no renderable points.");
        return;
    }

    if (modelActor_) {
        renderer_->RemoveActor(modelActor_);
    }

    modelPath_ = filePath;
    modelData_ = polyData;
    modelMapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    modelMapper_->SetInputData(modelData_);
    modelMapper_->SetLookupTable(lookupTable_);
    modelMapper_->ScalarVisibilityOn();

    modelActor_ = vtkSmartPointer<vtkActor>::New();
    modelActor_->SetMapper(modelMapper_);
    modelActor_->GetProperty()->SetInterpolationToPhong();
    modelActor_->GetProperty()->SetSpecular(0.25);
    modelActor_->GetProperty()->SetSpecularPower(18);
    renderer_->AddActor(modelActor_);

    modelLabel_->setText(QString("Model: %1\nPoints: %2").arg(displayName).arg(modelData_->GetNumberOfPoints()));
    applyContour();
    resetView();
    statusBar()->showMessage("Loaded model: " + displayName);
}

vtkSmartPointer<vtkPolyData> MainWindow::loadStepModel(const QString &filePath)
{
    STEPControl_Reader reader;
    const IFSelect_ReturnStatus status = reader.ReadFile(filePath.toLocal8Bit().constData());
    if (status != IFSelect_RetDone) {
        throw std::runtime_error("OCCT could not read the STEP/STP file.");
    }

    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) {
        throw std::runtime_error("The STEP/STP file did not contain a valid shape.");
    }

    const double linearDeflection = 0.25;
    const double angularDeflection = 0.5;
    BRepMesh_IncrementalMesh mesh(shape, linearDeflection, false, angularDeflection, true);
    mesh.Perform();
    if (!mesh.IsDone()) {
        throw std::runtime_error("OCCT failed to create a surface mesh from the STEP/STP shape.");
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto triangles = vtkSmartPointer<vtkCellArray>::New();

    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }

        const gp_Trsf transform = location.Transformation();
        for (int i = 1; i <= triangulation->NbTriangles(); ++i) {
            int n1 = 0;
            int n2 = 0;
            int n3 = 0;
            triangulation->Triangle(i).Get(n1, n2, n3);
            if (face.Orientation() == TopAbs_REVERSED) {
                std::swap(n2, n3);
            }

            gp_Pnt p1 = triangulation->Node(n1);
            gp_Pnt p2 = triangulation->Node(n2);
            gp_Pnt p3 = triangulation->Node(n3);
            p1.Transform(transform);
            p2.Transform(transform);
            p3.Transform(transform);

            const vtkIdType ids[3] = {
                points->InsertNextPoint(p1.X(), p1.Y(), p1.Z()),
                points->InsertNextPoint(p2.X(), p2.Y(), p2.Z()),
                points->InsertNextPoint(p3.X(), p3.Y(), p3.Z())
            };
            triangles->InsertNextCell(3, ids);
        }
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetPolys(triangles);

    auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputData(polyData);
    cleaner->Update();

    auto cleaned = vtkSmartPointer<vtkPolyData>::New();
    cleaned->DeepCopy(cleaner->GetOutput());
    if (cleaned->GetNumberOfPoints() == 0 || cleaned->GetNumberOfCells() == 0) {
        throw std::runtime_error("The STEP/STP shape did not produce renderable mesh triangles.");
    }
    return cleaned;
}

void MainWindow::loadSensors(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Sensors", "Could not open sensor file.");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        QMessageBox::warning(this, "Sensors", "Sensor file must contain a JSON array.");
        return;
    }

    sensors_.clear();
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        const QJsonArray position = object.value("position").toArray();
        if (position.size() < 3) {
            continue;
        }
        Sensor sensor;
        sensor.id = object.value("id").toString();
        sensor.name = object.value("name").toString(sensor.id);
        sensor.position = QVector3D(
            float(position.at(0).toDouble()),
            float(position.at(1).toDouble()),
            float(position.at(2).toDouble()));
        sensor.type = object.value("type").toString("load");
        sensor.unit = object.value("unit").toString("kN");
        sensors_.push_back(sensor);
    }

    refreshSensorActors();
    refreshSensorList();
    applyContour();
    statusBar()->showMessage(QString("Loaded %1 sensors.").arg(sensors_.size()));
}

void MainWindow::loadLoadData(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Load data", "Could not open CSV file.");
        return;
    }

    QTextStream stream(&file);
    const QString headerLine = stream.readLine();
    const QStringList headers = headerLine.split(',', Qt::SkipEmptyParts);

    QVector<QMap<QString, double>> rows;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList values = line.split(',');
        QMap<QString, double> row;
        for (int i = 0; i < headers.size() && i < values.size(); ++i) {
            row.insert(headers.at(i).trimmed(), values.at(i).trimmed().toDouble());
        }
        rows.push_back(row);
    }

    if (rows.isEmpty()) {
        QMessageBox::warning(this, "Load data", "CSV file has no data rows.");
        return;
    }

    loadRows_ = rows;
    currentRow_ = 0;
    applyFrame(0);
    statusBar()->showMessage(QString("Loaded %1 load frames.").arg(loadRows_.size()));
}

void MainWindow::refreshSensorActors()
{
    for (const auto &actor : sensorActors_) {
        renderer_->RemoveActor(actor);
    }
    for (const auto &label : labelActors_) {
        renderer_->RemoveActor(label);
    }
    sensorActors_.clear();
    labelActors_.clear();

    for (const Sensor &sensor : sensors_) {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetCenter(sensor.position.x(), sensor.position.y(), sensor.position.z());
        sphere->SetRadius(0.08);
        sphere->SetThetaResolution(24);
        sphere->SetPhiResolution(16);

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(1.0, 0.82, 0.18);
        actor->GetProperty()->SetAmbient(0.35);
        actor->GetProperty()->SetDiffuse(0.75);
        renderer_->AddActor(actor);
        sensorActors_.push_back(actor);

        auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        label->SetInput(sensor.id.toUtf8().constData());
        label->SetPosition(sensor.position.x() + 0.08, sensor.position.y() + 0.08, sensor.position.z() + 0.12);
        label->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
        label->GetTextProperty()->SetFontSize(18);
        renderer_->AddActor(label);
        labelActors_.push_back(label);
    }

    renderWindow_->Render();
}

void MainWindow::refreshSensorList()
{
    sensorList_->clear();
    for (const Sensor &sensor : sensors_) {
        sensorList_->addItem(QString("%1  %2 %3  (%4, %5, %6)")
            .arg(sensor.id)
            .arg(sensor.value, 0, 'f', 2)
            .arg(sensor.unit)
            .arg(sensor.position.x(), 0, 'f', 2)
            .arg(sensor.position.y(), 0, 'f', 2)
            .arg(sensor.position.z(), 0, 'f', 2));
    }
}

void MainWindow::applyFrame(int index)
{
    if (loadRows_.isEmpty()) {
        return;
    }

    currentRow_ = index % loadRows_.size();
    const QMap<QString, double> &row = loadRows_.at(currentRow_);
    for (Sensor &sensor : sensors_) {
        if (row.contains(sensor.id)) {
            sensor.value = row.value(sensor.id);
        }
    }

    const double timestamp = row.value("timestamp", currentRow_);
    timeLabel_->setText(QString("Time: %1 s").arg(timestamp, 0, 'f', 2));

    if (!sensors_.isEmpty()) {
        auto [minIt, maxIt] = std::minmax_element(sensors_.begin(), sensors_.end(), [](const Sensor &a, const Sensor &b) {
            return a.value < b.value;
        });
        valueLabel_->setText(QString("Load range: %1 - %2 kN").arg(minIt->value, 0, 'f', 2).arg(maxIt->value, 0, 'f', 2));
        const double upper = maxIt->value > minIt->value ? maxIt->value : minIt->value + 1.0;
        lookupTable_->SetRange(minIt->value, upper);
    }

    refreshSensorList();
    applyContour();
}

void MainWindow::applyContour()
{
    if (!modelData_ || sensors_.isEmpty()) {
        renderWindow_->Render();
        return;
    }

    auto scalars = vtkSmartPointer<vtkDoubleArray>::New();
    scalars->SetName("InterpolatedLoad");
    scalars->SetNumberOfComponents(1);
    scalars->SetNumberOfTuples(modelData_->GetNumberOfPoints());

    for (vtkIdType i = 0; i < modelData_->GetNumberOfPoints(); ++i) {
        double point[3];
        modelData_->GetPoint(i, point);
        scalars->SetValue(i, interpolateLoad(point[0], point[1], point[2]));
    }

    modelData_->GetPointData()->SetScalars(scalars);
    modelData_->Modified();

    if (modelMapper_) {
        double minValue = std::numeric_limits<double>::max();
        double maxValue = std::numeric_limits<double>::lowest();
        for (const Sensor &sensor : sensors_) {
            minValue = std::min(minValue, sensor.value);
            maxValue = std::max(maxValue, sensor.value);
        }
        if (maxValue <= minValue) {
            maxValue = minValue + 1.0;
        }
        modelMapper_->SetScalarRange(minValue, maxValue);
        modelMapper_->Update();
    }

    renderWindow_->Render();
}

double MainWindow::interpolateLoad(double x, double y, double z) const
{
    double weightedSum = 0.0;
    double weightTotal = 0.0;

    for (const Sensor &sensor : sensors_) {
        const double dx = x - sensor.position.x();
        const double dy = y - sensor.position.y();
        const double dz = z - sensor.position.z();
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance < 1e-6) {
            return sensor.value;
        }
        const double weight = 1.0 / (distance * distance);
        weightedSum += sensor.value * weight;
        weightTotal += weight;
    }

    return weightTotal > 0.0 ? weightedSum / weightTotal : 0.0;
}

void MainWindow::advanceFrame()
{
    if (!loadRows_.isEmpty()) {
        applyFrame(currentRow_ + 1);
    }
}

void MainWindow::togglePlayback()
{
    isPlaying_ = !isPlaying_;
    if (isPlaying_) {
        playbackTimer_.start();
        statusBar()->showMessage("Playback running.");
    } else {
        playbackTimer_.stop();
        statusBar()->showMessage("Playback paused.");
    }
}

void MainWindow::resetView()
{
    renderer_->ResetCamera();
    auto *camera = renderer_->GetActiveCamera();
    camera->Azimuth(35);
    camera->Elevation(22);
    renderer_->ResetCameraClippingRange();
    renderWindow_->Render();
}

void MainWindow::exportPng()
{
    const QString defaultPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("visualization.png");
    const QString path = QFileDialog::getSaveFileName(this, "Export PNG", defaultPath, "PNG (*.png)");
    if (path.isEmpty()) {
        return;
    }

    renderWindow_->Render();

    auto capture = vtkSmartPointer<vtkWindowToImageFilter>::New();
    capture->SetInput(renderWindow_);
    capture->SetScale(2);
    capture->SetInputBufferTypeToRGBA();
    capture->ReadFrontBufferOff();
    capture->Update();

    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(path.toLocal8Bit().constData());
    writer->SetInputConnection(capture->GetOutputPort());
    writer->Write();

    statusBar()->showMessage("Exported screenshot: " + path);
}
