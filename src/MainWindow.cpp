#include "MainWindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <QVTKOpenGLNativeWidget.h>

#include <vtkAxesActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCaptionActor2D.h>
#include <vtkCellPicker.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLookupTable.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPNGWriter.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarBarActor.h>
#include <vtkTextProperty.h>
#include <vtkWindowToImageFilter.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("3D Specimen Visualization Prototype");

    buildUi();
    buildScene();
    statusBar()->showMessage("Ready. Open a model to begin.");
}
MainWindow::~MainWindow()
{
    if (solverProcess_) {
        solverProcess_->kill();
        solverProcess_->waitForFinished(1500);
    }
    if (meshWorker_) {
        meshWorker_->wait();
    }
}
void MainWindow::buildUi()
{
    auto *toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder), "New Project", this, &MainWindow::newProject);
    toolbar->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), "Open Project", this, &MainWindow::openProject);
    toolbar->addSeparator();
    toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), "Reset View", this, &MainWindow::resetView);
    toolbar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), "Screenshot", this, &MainWindow::exportPng);

    pickConstraintFaceAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogApplyButton), "Pick Constraint Faces", this);
    pickConstraintFaceAction_->setCheckable(true);
    pickConstraintFaceAction_->setToolTip("Pick STEP faces to constrain before Netgen meshing");
    pickControlPointAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), "Pick Load Points", this);
    pickControlPointAction_->setCheckable(true);
    pickControlPointAction_->setToolTip("Pick loading points on the generated boundary mesh");
    connect(pickConstraintFaceAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked && pickControlPointAction_) {
            pickControlPointAction_->setChecked(false);
        }
        statusBar()->showMessage(checked ? "Constraint face picking enabled." : "Constraint face picking disabled.");
    });
    connect(pickControlPointAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked && pickConstraintFaceAction_) {
            pickConstraintFaceAction_->setChecked(false);
        }
        statusBar()->showMessage(checked ? "Load point picking enabled." : "Load point picking disabled.");
    });

    auto *root = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *sidePanel = new QWidget(root);
    sidePanel->setObjectName("SidePanel");
    sidePanel->setFixedWidth(390);
    auto *sideOuterLayout = new QVBoxLayout(sidePanel);
    sideOuterLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(sidePanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *sideContent = new QWidget(scrollArea);
    auto *sideLayout = new QVBoxLayout(sideContent);
    sideLayout->setContentsMargins(16, 16, 16, 18);
    sideLayout->setSpacing(12);
    scrollArea->setWidget(sideContent);
    sideOuterLayout->addWidget(scrollArea);

    auto *titleLabel = new QLabel("Preprocessing Workflow", sideContent);
    titleLabel->setObjectName("PanelTitle");
    auto *subtitleLabel = new QLabel("Follow the enabled action from top to bottom. Configuration checks update after each step.", sideContent);
    subtitleLabel->setObjectName("PanelSubtitle");
    subtitleLabel->setWordWrap(true);
    sideLayout->addWidget(titleLabel);
    sideLayout->addWidget(subtitleLabel);

    projectLabel_ = new QLabel("Project: not created", sidePanel);
    projectLabel_->setWordWrap(true);
    modelLabel_ = new QLabel("Model: not loaded", sidePanel);
    modelLabel_->setWordWrap(true);
    workflowLabel_ = new QLabel(sidePanel);
    workflowLabel_->setObjectName("NextActionLabel");
    workflowLabel_->setWordWrap(true);
    workflowLabel_->setTextFormat(Qt::RichText);
    configStatusLabel_ = new QLabel(sidePanel);
    configStatusLabel_->setObjectName("ConfigStatusLabel");
    configStatusLabel_->setWordWrap(true);
    configStatusLabel_->setTextFormat(Qt::RichText);
    valueLabel_ = new QLabel("Load range: --", sidePanel);
    valueLabel_->setObjectName("MutedLabel");
    loadingDeviceList_ = new QListWidget(sidePanel);
    fixedNodeList_ = new QListWidget(sidePanel);
    connect(loadingDeviceList_, &QListWidget::currentRowChanged, this, &MainWindow::onLoadingDeviceSelectionChanged);

    openModelButton_ = new QPushButton(style()->standardIcon(QStyle::SP_FileIcon), "Open Model", sideContent);
    connect(openModelButton_, &QPushButton::clicked, this, &MainWindow::openModel);
    generateMeshButton_ = new QPushButton(style()->standardIcon(QStyle::SP_ComputerIcon), "Generate Volume Mesh", sideContent);
    connect(generateMeshButton_, &QPushButton::clicked, this, &MainWindow::generateVolumeMesh);
    loadPointsButton_ = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogDetailedView), "Load Points", sideContent);
    connect(loadPointsButton_, &QPushButton::clicked, this, &MainWindow::openLoadingDevices);
    exportInpButton_ = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), "Export INP", sideContent);
    connect(exportInpButton_, &QPushButton::clicked, this, &MainWindow::exportCalculixInput);
    runCalculixButton_ = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), "Run CalculiX", sideContent);
    connect(runCalculixButton_, &QPushButton::clicked, this, &MainWindow::runCalculixSolver);
    openResultButton_ = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogContentsView), "Open Result", sideContent);
    connect(openResultButton_, &QPushButton::clicked, this, &MainWindow::openCalculixResult);
    auto *clearPointsButton = new QPushButton("Clear Load Points", sideContent);
    connect(clearPointsButton, &QPushButton::clicked, this, &MainWindow::clearControlPoints);

    auto makeModeButton = [](QAction *action, QWidget *parent) {
        auto *button = new QToolButton(parent);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return button;
    };
    auto addStepCard = [&](const QString &number, const QString &title, QLabel **statusTarget, QWidget *actionWidget) {
        auto *card = new QFrame(sideContent);
        card->setObjectName("StepCard");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(8);
        auto *headerLayout = new QHBoxLayout();
        auto *badge = new QLabel(number, card);
        badge->setObjectName("StepBadge");
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(24, 24);
        auto *titleText = new QLabel(title, card);
        titleText->setObjectName("StepTitle");
        headerLayout->addWidget(badge);
        headerLayout->addWidget(titleText, 1);
        cardLayout->addLayout(headerLayout);
        auto *status = new QLabel(card);
        status->setObjectName("StepStatus");
        status->setWordWrap(true);
        status->setTextFormat(Qt::RichText);
        *statusTarget = status;
        cardLayout->addWidget(status);
        if (actionWidget) {
            cardLayout->addWidget(actionWidget);
        }
        sideLayout->addWidget(card);
    };

    sideLayout->addWidget(workflowLabel_);
    sideLayout->addWidget(configStatusLabel_);
    addStepCard("1", "Project", &projectStepStatusLabel_, nullptr);
    addStepCard("2", "Import STEP Model", &modelStepStatusLabel_, openModelButton_);
    addStepCard("3", "Constraint Faces", &constraintStepStatusLabel_, makeModeButton(pickConstraintFaceAction_, sideContent));
    addStepCard("4", "Netgen Volume Mesh", &meshStepStatusLabel_, generateMeshButton_);

    auto *loadActions = new QWidget(sideContent);
    auto *loadActionLayout = new QHBoxLayout(loadActions);
    loadActionLayout->setContentsMargins(0, 0, 0, 0);
    loadActionLayout->setSpacing(8);
    loadActionLayout->addWidget(loadPointsButton_);
    loadActionLayout->addWidget(makeModeButton(pickControlPointAction_, sideContent));
    addStepCard("5", "Load Points", &loadStepStatusLabel_, loadActions);
    sideLayout->addWidget(clearPointsButton);

    auto *exportActions = new QWidget(sideContent);
    auto *exportActionLayout = new QHBoxLayout(exportActions);
    exportActionLayout->setContentsMargins(0, 0, 0, 0);
    exportActionLayout->setSpacing(8);
    exportActionLayout->addWidget(exportInpButton_);
    exportActionLayout->addWidget(runCalculixButton_);
    addStepCard("6", "Export and Solve", &exportStepStatusLabel_, exportActions);
    addStepCard("7", "Visualize Result", &resultStepStatusLabel_, openResultButton_);

    auto *detailsTitle = new QLabel("Project Details", sideContent);
    detailsTitle->setObjectName("SectionTitle");
    sideLayout->addWidget(detailsTitle);
    sideLayout->addWidget(projectLabel_);
    sideLayout->addWidget(modelLabel_);
    sideLayout->addWidget(valueLabel_);
    auto *loadListTitle = new QLabel("Load Points", sideContent);
    loadListTitle->setObjectName("SectionTitle");
    sideLayout->addWidget(loadListTitle);
    sideLayout->addWidget(loadingDeviceList_, 1);
    auto *constraintListTitle = new QLabel("Constraint Faces", sideContent);
    constraintListTitle->setObjectName("SectionTitle");
    sideLayout->addWidget(constraintListTitle);
    sideLayout->addWidget(fixedNodeList_);
    sideLayout->addStretch(1);

    sidePanel->setStyleSheet(
        "QWidget#SidePanel { background: #f4f6f8; border-right: 1px solid #d7dde4; }"
        "QLabel#PanelTitle { color: #17212b; font-size: 20px; font-weight: 700; }"
        "QLabel#PanelSubtitle, QLabel#MutedLabel { color: #5d6975; }"
        "QLabel#NextActionLabel { background: #eaf2ff; border: 1px solid #c7dbff; border-radius: 6px; padding: 10px; color: #183b67; }"
        "QLabel#ConfigStatusLabel { background: #ffffff; border: 1px solid #d9e0e7; border-radius: 6px; padding: 10px; color: #26323d; }"
        "QFrame#StepCard { background: #ffffff; border: 1px solid #d9e0e7; border-radius: 8px; }"
        "QLabel#StepBadge { background: #1d4ed8; color: white; border-radius: 12px; font-weight: 700; }"
        "QLabel#StepTitle { color: #17212b; font-size: 14px; font-weight: 700; }"
        "QLabel#StepStatus { color: #4a5562; }"
        "QLabel#SectionTitle { color: #344151; font-weight: 700; margin-top: 6px; }"
        "QPushButton, QToolButton { min-height: 30px; border: 1px solid #bcc7d3; border-radius: 6px; padding: 6px 10px; background: #ffffff; color: #1f2d3a; }"
        "QPushButton:hover, QToolButton:hover { background: #eef5ff; border-color: #8fb8ff; }"
        "QPushButton:checked, QToolButton:checked { background: #dbeafe; border-color: #2563eb; color: #123c88; }"
        "QPushButton:disabled, QToolButton:disabled { background: #eef1f4; color: #98a3ae; border-color: #d7dde4; }"
        "QListWidget { background: #ffffff; border: 1px solid #d9e0e7; border-radius: 6px; padding: 4px; }"
    );

    vtkWidget_ = new QVTKOpenGLNativeWidget(root);
    rootLayout->addWidget(sidePanel);
    rootLayout->addWidget(vtkWidget_, 1);

    setCentralWidget(root);
    setStatusBar(new QStatusBar(this));
    backgroundProgressLabel_ = new QLabel(this);
    backgroundProgressLabel_->setObjectName("BackgroundProgressLabel");
    backgroundProgressLabel_->setVisible(false);
    backgroundProgressBar_ = new QProgressBar(this);
    backgroundProgressBar_->setRange(0, 0);
    backgroundProgressBar_->setFixedWidth(180);
    backgroundProgressBar_->setTextVisible(false);
    backgroundProgressBar_->setVisible(false);
    statusBar()->addPermanentWidget(backgroundProgressLabel_);
    statusBar()->addPermanentWidget(backgroundProgressBar_);
    refreshWorkflowStatus();
}

void MainWindow::startBackgroundProgress(const QString &message)
{
    if (backgroundProgressLabel_) {
        backgroundProgressLabel_->setText(message);
        backgroundProgressLabel_->setVisible(true);
    }
    if (backgroundProgressBar_) {
        backgroundProgressBar_->setRange(0, 0);
        backgroundProgressBar_->setVisible(true);
    }
    statusBar()->showMessage(message);
    refreshWorkflowStatus();
}

void MainWindow::finishBackgroundProgress(const QString &message)
{
    if (backgroundProgressLabel_) {
        backgroundProgressLabel_->setVisible(false);
    }
    if (backgroundProgressBar_) {
        backgroundProgressBar_->setVisible(false);
    }
    statusBar()->showMessage(message);
    refreshWorkflowStatus();
}

void MainWindow::refreshWorkflowStatus()
{
    const bool projectReady = !projectDir_.isEmpty();
    const bool modelReady = !modelPath_.isEmpty();
    const QString suffix = QFileInfo(modelPath_).suffix().toLower();
    const bool stepModel = modelReady && (suffix == QStringLiteral("step") || suffix == QStringLiteral("stp"));
    const int constraintCount = selectedConstraintFaceIds().size();
    const bool constraintsReady = constraintCount > 0;
    const bool meshReady = !volumeMesh_.nodes.isEmpty() && !volumeMesh_.tetrahedra.isEmpty();
    const bool loadsLoaded = !loadingDevices_.isEmpty();
    const bool backgroundBusy = meshJobRunning_ || solverJobRunning_;

    int enabledLoads = 0;
    int mappedLoads = 0;
    for (const LoadingDevice &device : loadingDevices_) {
        if (!device.enabled) {
            continue;
        }
        ++enabledLoads;
        if (device.meshNodeId >= 0 && device.meshNodeId < volumeMesh_.nodes.size()) {
            ++mappedLoads;
        }
    }
    const bool loadsReady = loadsLoaded && enabledLoads > 0 && mappedLoads == enabledLoads;
    const bool solveReady = projectReady && stepModel && constraintsReady && meshReady && loadsReady;
    const bool resultAvailable = projectReady
        && (QFile::exists(QDir(projectSubdir(QStringLiteral("solver"))).absoluteFilePath(QStringLiteral("model.frd")))
            || QFile::exists(QDir(projectSubdir(QStringLiteral("results"))).absoluteFilePath(QStringLiteral("model.frd"))));

    auto setStep = [](QLabel *label, bool ok, const QString &okText, const QString &pendingText) {
        if (!label) {
            return;
        }
        const QString state = ok
            ? QStringLiteral("<span style='color:#16794c;font-weight:700'>OK</span>")
            : QStringLiteral("<span style='color:#a15c00;font-weight:700'>Pending</span>");
        label->setText(QString("%1&nbsp;&nbsp;%2").arg(state, ok ? okText : pendingText));
    };

    setStep(projectStepStatusLabel_, projectReady,
        QString("Project folder: %1").arg(projectName_),
        QStringLiteral("Create or open a project before importing files."));
    setStep(modelStepStatusLabel_, stepModel,
        QString("STEP model loaded: %1").arg(QFileInfo(modelPath_).fileName()),
        modelReady ? QStringLiteral("Loaded file is not STEP/STP; full solver workflow requires STEP/STP.") : QStringLiteral("Import a STEP/STP CAD model."));
    setStep(constraintStepStatusLabel_, constraintsReady,
        QString("%1 constraint face(s) selected.").arg(constraintCount),
        QStringLiteral("Pick one or more STEP faces to fix UX/UY/UZ."));
    setStep(meshStepStatusLabel_, meshReady,
        QString("Netgen mesh: %1 nodes, %2 tetrahedra.").arg(volumeMesh_.nodes.size()).arg(volumeMesh_.tetrahedra.size()),
        QStringLiteral("Generate the Netgen volume mesh after constraint faces are selected."));
    setStep(loadStepStatusLabel_, loadsReady,
        QString("%1/%2 enabled load point(s) mapped to mesh nodes.").arg(mappedLoads).arg(enabledLoads),
        loadsLoaded
            ? QString("%1/%2 enabled load point(s) mapped. Pick remaining points on the boundary mesh.").arg(mappedLoads).arg(enabledLoads)
            : QStringLiteral("Load a loading-point JSON file, then pick each point on the boundary mesh."));
    setStep(exportStepStatusLabel_, solveReady,
        QStringLiteral("Ready to export INP or run CalculiX."),
        QStringLiteral("Requires STEP model, constraints, volume mesh, and mapped load points."));
    setStep(resultStepStatusLabel_, resultAvailable,
        QStringLiteral("Result file found. Open it to show displacement contour."),
        QStringLiteral("Run CalculiX or place model.frd in solver/results."));

    QString nextAction;
    if (!projectReady) {
        nextAction = QStringLiteral("Next: create or open a project from the top toolbar.");
    } else if (!modelReady || !stepModel) {
        nextAction = QStringLiteral("Next: import a STEP/STP model.");
    } else if (!constraintsReady) {
        nextAction = QStringLiteral("Next: enable Pick Constraint Faces and click fixed faces on the imported model.");
    } else if (!meshReady) {
        nextAction = QStringLiteral("Next: generate the Netgen volume mesh.");
    } else if (!loadsLoaded) {
        nextAction = QStringLiteral("Next: load the loading-point JSON file.");
    } else if (!loadsReady) {
        nextAction = QStringLiteral("Next: enable Pick Load Points and click nodes on the generated boundary mesh.");
    } else {
        nextAction = QStringLiteral("Next: export model.inp or run CalculiX, then open the result for visualization.");
    }
    if (workflowLabel_) {
        workflowLabel_->setText(QString("<b>%1</b>").arg(nextAction));
    }

    QStringList checks;
    checks << (projectReady ? QStringLiteral("Project ready") : QStringLiteral("Project missing"));
    checks << (stepModel ? QStringLiteral("STEP model ready") : QStringLiteral("STEP model missing"));
    checks << (constraintsReady ? QString("Constraint faces: %1").arg(constraintCount) : QStringLiteral("Constraint faces missing"));
    checks << (meshReady ? QString("Volume mesh: %1 nodes").arg(volumeMesh_.nodes.size()) : QStringLiteral("Volume mesh missing"));
    checks << (loadsReady ? QString("Load nodes mapped: %1").arg(mappedLoads) : QStringLiteral("Load nodes missing"));
    if (configStatusLabel_) {
        configStatusLabel_->setText(QString("<b>Configuration checks</b><br>%1").arg(checks.join(QStringLiteral("<br>"))));
    }

    if (openModelButton_) {
        openModelButton_->setEnabled(projectReady);
    }
    if (pickConstraintFaceAction_) {
        pickConstraintFaceAction_->setEnabled(stepModel);
        if (!stepModel) {
            pickConstraintFaceAction_->setChecked(false);
        }
    }
    if (generateMeshButton_) {
        generateMeshButton_->setEnabled(projectReady && stepModel && constraintsReady && !backgroundBusy);
    }
    if (loadPointsButton_) {
        loadPointsButton_->setEnabled(projectReady && meshReady);
    }
    if (pickControlPointAction_) {
        pickControlPointAction_->setEnabled(meshReady && loadsLoaded);
        if (!meshReady || !loadsLoaded) {
            pickControlPointAction_->setChecked(false);
        }
    }
    if (exportInpButton_) {
        exportInpButton_->setEnabled(solveReady && !backgroundBusy);
    }
    if (runCalculixButton_) {
        runCalculixButton_->setEnabled(solveReady && !backgroundBusy);
    }
    if (openResultButton_) {
        openResultButton_->setEnabled(meshReady);
    }
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

    modelPicker_ = vtkSmartPointer<vtkCellPicker>::New();
    modelPicker_->SetTolerance(0.0008);
    modelPicker_->PickFromListOn();

    leftButtonPressCallback_ = vtkSmartPointer<vtkCallbackCommand>::New();
    leftButtonPressCallback_->SetClientData(this);
    leftButtonPressCallback_->SetCallback(&MainWindow::onVtkLeftButtonPress);
    vtkWidget_->interactor()->AddObserver(vtkCommand::LeftButtonPressEvent, leftButtonPressCallback_, 1.0);

    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(1.0, 1.0, 1.0);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.035);
    axes->SetConeRadius(0.16);
    axes->SetSphereRadius(0.08);
    axes->GetXAxisCaptionActor2D()->GetCaptionTextProperty()->SetColor(0.95, 0.22, 0.18);
    axes->GetYAxisCaptionActor2D()->GetCaptionTextProperty()->SetColor(0.25, 0.82, 0.32);
    axes->GetZAxisCaptionActor2D()->GetCaptionTextProperty()->SetColor(0.30, 0.55, 1.0);
    axes->GetXAxisCaptionActor2D()->GetCaptionTextProperty()->SetFontSize(18);
    axes->GetYAxisCaptionActor2D()->GetCaptionTextProperty()->SetFontSize(18);
    axes->GetZAxisCaptionActor2D()->GetCaptionTextProperty()->SetFontSize(18);

    orientationMarker_ = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationMarker_->SetOrientationMarker(axes);
    orientationMarker_->SetInteractor(vtkWidget_->interactor());
    orientationMarker_->SetViewport(0.02, 0.02, 0.18, 0.18);
    orientationMarker_->SetEnabled(1);
    orientationMarker_->InteractiveOff();

    scalarBar_ = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar_->SetLookupTable(lookupTable_);
    scalarBar_->SetTitle("Load (kN)");
    scalarBar_->SetNumberOfLabels(5);
    scalarBar_->SetWidth(0.10);
    scalarBar_->SetHeight(0.46);
    scalarBar_->SetPosition(0.88, 0.10);
    renderer_->AddViewProp(scalarBar_);
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
    if (!ensureProjectReady(QStringLiteral("opening a model"))) {
        return;
    }

    QFileDialog dialog(this, "Open model", projectDir_.isEmpty() ? samplePath(QString()) : projectDir_);
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
        const QString projectModelPath = copyFileIntoProject(dialog.selectedFiles().first(), QStringLiteral("models"));
        if (projectModelPath.isEmpty()) {
            return;
        }
        loadModel(projectModelPath);
        projectModelRelativePath_ = projectRelativePath(projectModelPath);
        saveProjectFile();
        updateProjectUi();
    }
}
void MainWindow::openLoadingDevices()
{
    if (!ensureProjectReady(QStringLiteral("loading load points"))) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, "Load load points", projectSubdir(QStringLiteral("inputs")), "JSON (*.json)");
    if (!path.isEmpty()) {
        const QString projectInputPath = copyFileIntoProject(path, QStringLiteral("inputs"));
        if (projectInputPath.isEmpty()) {
            return;
        }
        loadLoadingDevices(projectInputPath);
        projectLoadingDevicesRelativePath_ = projectRelativePath(projectInputPath);
        saveProjectFile();
        updateProjectUi();
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
    if (!ensureProjectReady(QStringLiteral("exporting a screenshot"))) {
        return;
    }
    const QString path = QDir(projectSubdir(QStringLiteral("exports"))).absoluteFilePath("visualization.png");

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
