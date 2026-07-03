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
    InteractionWidgets
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
    src/MainWindowCalculix.cpp
    src/MainWindowContour.cpp
    src/MainWindow.h
    src/MainWindowLoadingDevices.cpp
    src/MainWindowModel.cpp
    src/MainWindowPicking.cpp
    src/MainWindowProject.cpp
    src/MainWindowConstraintFaces.cpp
    src/MainWindowVolumeMesh.cpp
    src/LoadingDevice.h
    src/ConstraintFaceGroup.h
    src/VolumeMesh.h
    src/NetgenVolumeMesher.cpp
    src/NetgenVolumeMesher.h
)

qt_add_executable(NetgenStepMeshSmoke
    src/NetgenStepMeshSmoke.cpp
    src/NetgenVolumeMesher.cpp
    src/NetgenVolumeMesher.h
    src/VolumeMesh.h
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

target_include_directories(NetgenStepMeshSmoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCASCADE_INCLUDE_DIR}
    ${OpenCASCADE_INCLUDE_DIRS}
)

target_link_libraries(NetgenStepMeshSmoke PRIVATE
    Qt6::Core
    ${OpenCASCADE_LIBRARIES}
)

if(OpenCASCADE_LIBRARY_DIR)
    target_link_directories(VisualizationPrototype PRIVATE ${OpenCASCADE_LIBRARY_DIR})
    target_link_directories(NetgenStepMeshSmoke PRIVATE ${OpenCASCADE_LIBRARY_DIR})
elseif(OpenCASCADE_LIBRARY_DIRS)
    target_link_directories(VisualizationPrototype PRIVATE ${OpenCASCADE_LIBRARY_DIRS})
    target_link_directories(NetgenStepMeshSmoke PRIVATE ${OpenCASCADE_LIBRARY_DIRS})
endif()

vtk_module_autoinit(
    TARGETS VisualizationPrototype
    MODULES ${VTK_LIBRARIES}
)

if(WIN32)
    get_filename_component(QT6_PACKAGE_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    set(QT6_RUNTIME_CONF "${CMAKE_CURRENT_BINARY_DIR}/qt.conf")
    file(WRITE "${QT6_RUNTIME_CONF}" "[Paths]\nPrefix = .\nPlugins = .\n")
    set(NETGEN_EXE "${CMAKE_CURRENT_SOURCE_DIR}/extern/netgen/bin/netgen.exe")

    set(DEPLOY_RUNTIME_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/deploy_runtime_dependencies.cmake")
    file(WRITE "${DEPLOY_RUNTIME_SCRIPT}" [=[
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

get_filename_component(TARGET_NAME "${TARGET_FILE}" NAME)
set(SCAN_DIR "${TARGET_DIR}/.runtime-scan")
set(SCAN_FILE "${SCAN_DIR}/${TARGET_NAME}")

file(REMOVE_RECURSE "${SCAN_DIR}")
file(MAKE_DIRECTORY "${SCAN_DIR}")
file(COPY "${TARGET_FILE}" DESTINATION "${SCAN_DIR}")

if(EXISTS "${SDK_BIN_DIR}/objdump.exe")
    file(REMOVE_RECURSE "${SCAN_DIR}")
    return()
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${SCAN_FILE}"
    DIRECTORIES "${SDK_BIN_DIR}"
    RESOLVED_DEPENDENCIES_VAR RESOLVED_DLLS
    UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DLLS
    PRE_EXCLUDE_REGEXES
        [[api-ms-.*]]
        [[ext-ms-.*]]
        [[msvcp140.*\.dll]]
        [[vcruntime140.*\.dll]]
        [[concrt140\.dll]]
        [[vcomp140\.dll]]
        [[vcamp140\.dll]]
        [[vccorlib140\.dll]]
    POST_EXCLUDE_REGEXES
        [[.*[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/.*]]
        [[.*[Ww][Ii][Nn][Dd][Oo][Ww][Ss]\\.*]]
)

foreach(RESOLVED_DLL IN LISTS RESOLVED_DLLS)
    file(COPY "${RESOLVED_DLL}" DESTINATION "${TARGET_DIR}")
endforeach()

file(REMOVE_RECURSE "${SCAN_DIR}")

file(GLOB MSVC_RUNTIME_DLLS
    "${SDK_BIN_DIR}/msvcp140*.dll"
    "${SDK_BIN_DIR}/vcruntime140*.dll"
    "${SDK_BIN_DIR}/concrt140.dll"
    "${SDK_BIN_DIR}/vcomp140.dll"
    "${SDK_BIN_DIR}/vcamp140.dll"
    "${SDK_BIN_DIR}/vccorlib140.dll"
)

foreach(MSVC_RUNTIME_DLL IN LISTS MSVC_RUNTIME_DLLS)
    file(COPY "${MSVC_RUNTIME_DLL}" DESTINATION "${TARGET_DIR}")
endforeach()

if(UNRESOLVED_DLLS)
    message(WARNING "Unresolved runtime dependencies for ${TARGET_FILE}: ${UNRESOLVED_DLLS}")
endif()
]=])

    add_custom_command(TARGET VisualizationPrototype POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D "TARGET_FILE=$<TARGET_FILE:VisualizationPrototype>"
            -D "TARGET_DIR=$<TARGET_FILE_DIR:VisualizationPrototype>"
            -D "SDK_BIN_DIR=${QT6_PACKAGE_PREFIX}/bin"
            -P "${DEPLOY_RUNTIME_SCRIPT}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_RUNTIME_CONF}"
            "$<TARGET_FILE_DIR:VisualizationPrototype>/qt.conf"
    )

    add_custom_command(TARGET NetgenStepMeshSmoke POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D "TARGET_FILE=$<TARGET_FILE:NetgenStepMeshSmoke>"
            -D "TARGET_DIR=$<TARGET_FILE_DIR:NetgenStepMeshSmoke>"
            -D "SDK_BIN_DIR=${QT6_PACKAGE_PREFIX}/bin"
            -P "${DEPLOY_RUNTIME_SCRIPT}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT6_RUNTIME_CONF}"
            "$<TARGET_FILE_DIR:NetgenStepMeshSmoke>/qt.conf"
    )

    if(EXISTS "${NETGEN_EXE}")
        foreach(NETGEN_DEPLOY_TARGET VisualizationPrototype NetgenStepMeshSmoke)
            add_custom_command(TARGET ${NETGEN_DEPLOY_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${NETGEN_EXE}"
                    "$<TARGET_FILE_DIR:${NETGEN_DEPLOY_TARGET}>/netgen.exe"
            )
        endforeach()
    endif()

    foreach(NETGEN_BRIDGE_DEPLOY_TARGET VisualizationPrototype NetgenStepMeshSmoke)
        add_custom_command(TARGET ${NETGEN_BRIDGE_DEPLOY_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${NETGEN_BRIDGE_DEPLOY_TARGET}>/scripts"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/netgen_mesh_step.py"
                "$<TARGET_FILE_DIR:${NETGEN_BRIDGE_DEPLOY_TARGET}>/scripts/netgen_mesh_step.py"
        )
    endforeach()

    set(QT6_PLUGINS_DIR "${QT6_PACKAGE_PREFIX}/plugins")
    foreach(QT6_PLUGIN_SUBDIR platforms imageformats)
        if(EXISTS "${QT6_PLUGINS_DIR}/${QT6_PLUGIN_SUBDIR}")
            add_custom_command(TARGET VisualizationPrototype POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${QT6_PLUGINS_DIR}/${QT6_PLUGIN_SUBDIR}"
                    "$<TARGET_FILE_DIR:VisualizationPrototype>/${QT6_PLUGIN_SUBDIR}"
            )
        endif()
    endforeach()
endif()

add_custom_command(TARGET VisualizationPrototype POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/samples
        $<TARGET_FILE_DIR:VisualizationPrototype>/samples
)
