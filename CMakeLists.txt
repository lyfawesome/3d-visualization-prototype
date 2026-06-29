cmake_minimum_required(VERSION 3.24)

project(VisualizationPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCASCADE CONFIG QUIET)
if(NOT OpenCASCADE_FOUND)
    find_package(opencascade CONFIG REQUIRED)
endif()
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersCore
    FiltersSources
    GUISupportQt
    InteractionStyle
    IOGeometry
    IOImage
    RenderingAnnotation
    RenderingCore
    RenderingOpenGL2
)

if(NOT OpenCASCADE_INCLUDE_DIR AND OpenCASCADE_INCLUDE_DIRS)
    set(OpenCASCADE_INCLUDE_DIR ${OpenCASCADE_INCLUDE_DIRS})
endif()

if(NOT OpenCASCADE_INCLUDE_DIR AND OpenCASCADE_INCLUDE_DIR)
    set(OpenCASCADE_INCLUDE_DIR ${OpenCASCADE_INCLUDE_DIR})
endif()

if(NOT OpenCASCADE_LIBRARIES)
    set(OpenCASCADE_LIBRARIES
        TKSTEP
        TKSTEPBase
        TKXSBase
        TKBRep
        TKMesh
        TKTopAlgo
        TKGeomAlgo
        TKGeomBase
        TKG3d
        TKG2d
        TKMath
        TKernel
    )
endif()

qt_add_executable(VisualizationPrototype WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/Sensor.h
)

target_include_directories(VisualizationPrototype PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCASCADE_INCLUDE_DIR}
    ${OpenCASCADE_INCLUDE_DIRS}
)

target_link_libraries(VisualizationPrototype PRIVATE
    Qt6::Widgets
    ${VTK_LIBRARIES}
    ${OpenCASCADE_LIBRARIES}
)

if(OpenCASCADE_LIBRARY_DIR)
    target_link_directories(VisualizationPrototype PRIVATE ${OpenCASCADE_LIBRARY_DIR})
elseif(OpenCASCADE_LIBRARY_DIRS)
    target_link_directories(VisualizationPrototype PRIVATE ${OpenCASCADE_LIBRARY_DIRS})
endif()

vtk_module_autoinit(
    TARGETS VisualizationPrototype
    MODULES ${VTK_LIBRARIES}
)

add_custom_command(TARGET VisualizationPrototype POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/samples
        $<TARGET_FILE_DIR:VisualizationPrototype>/samples
)
