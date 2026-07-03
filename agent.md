# Agent Development Context

## Product Background

This project is a Windows desktop simulation and visualization tool for a structural full-scale test bench.

The imported 3D model represents the physical structure or specimen being tested. The software should help the user place, inspect, and manage loading devices that apply forces to the structure during a structural mechanics test.

The long-term product direction is not a generic 3D viewer. It is an engineering test-bench application centered on:

- Importing the tested structure model, especially STEP/STP CAD models.
- Defining a fixed number of loading devices around or on the structure.
- Selecting control/load application points on the model surface.
- Visualizing applied loads, sensor/load channels, and time-varying test data.
- Supporting future connection to real loading equipment, control systems, or data acquisition systems.

## Domain Concepts

Use these concepts consistently in future development.

- Structure model: the imported test object, usually a STEP/STP CAD model. It is the target structure under test.
- Loading device: a physical actuator or load applicator. The project should assume a fixed configured count of loading devices, not arbitrary decorative points.
- Constraint face group: one or more selected STEP/OCCT CAD faces that become boundary-condition source regions during Netgen volume meshing.
- Control point: a user-selected point on the generated boundary mesh. It may become the location where a loading device applies force, a reference point for control, or a monitored point.
- Load channel: the numeric force value associated with a loading device or sensor. Time-varying channels are a future workflow, not current behavior.
- Sensor point: a measurement point. It is related to but not always the same as a loading device or control point.
- Load data: time-series data used to drive visualization. In the prototype this may be simulated CSV data; in later versions it may come from real equipment.

## Current Technical Direction

The current implementation uses:

- C++17 only.
- Qt 6 Widgets for the desktop UI.
- VTK for 3D rendering, interaction, markers, labels, color mapping, and screenshots.
- OCCT/OpenCASCADE for direct STEP/STP import through `STEPControl_Reader`.
- `BRepMesh_IncrementalMesh` to mesh OCCT shapes.
- Conversion from OCCT triangulation to `vtkPolyData`.
- Netgen as the tetrahedral volume meshing backend, invoked either as `netgen.exe` or through the official prebuilt `netgen-mesher` Python wheel bridge.
- CalculiX `ccx` as the selected structural finite element solver.
- CMake for build configuration.
- MSYS2 UCRT64 as the current fast prebuilt dependency route for Qt + VTK + OCCT.

Do not embed Python into the Qt application process unless the user explicitly requests it. The current Netgen wheel integration is an external process bridge only; the application remains a C++ Qt desktop program.

Do not use Gmsh for STEP loading. The chosen route is Qt + VTK + OCCT.

## Current Build Routes

Primary working route:

- MSYS2 UCRT64
- Ninja
- `C:\msys64\ucrt64` dependencies
- preset: `windows-msys2-ucrt64`
- executable: `build\windows-msys2-ucrt64\VisualizationPrototype.exe`

The MSYS2 route is useful for rapid development because Qt, VTK, and OCCT are available as compatible prebuilt binary packages.

Planned migration route:

- MSVC / Visual Studio 2022
- vcpkg or official MSVC-compatible prebuilt dependencies
- preset: `windows-msvc-vcpkg`

Do not mix MSYS2/MinGW libraries with MSVC builds. Qt, VTK, and OCCT must match the compiler ABI.

## Current Functional State

Implemented or recently added:

- Empty startup scene. The application should not preload sample models, sensors, or load data.
- Project management workflow starts with `New Project` or `Open Project`.
- A project folder owns `project.vpproj`, `models/`, `inputs/`, `meshes/`, `solver/`, `results/`, and `exports/`.
- Imported model and loading-device files are copied into the project before loading.
- Mesh generation and CalculiX export/solve write under the active project folder, not the application directory or arbitrary user-selected output folders.
- Manual model import for STEP/STP, STL, and OBJ.
- STEP/STP import through OCCT.
- Surface control point picking on imported models.
- `Pick Point` mode for selecting model-surface control points.
- Control point list and 3D point labels.
- Fixed-screen orientation marker instead of a large world-space XYZ axis.
- Load contour visualization using interpolation over model points.
- PNG viewport export.
- Load-point JSON workflow using `loading_points`.
- Constraint-face picking on STEP/OCCT source faces before volume meshing.
- Netgen STEP-to-volume-mesh workflow with constraint-face boundary markers.
- Netgen boundary triangles are remapped back to selected STEP/OCCT constraint faces by geometric distance; do not assume Netgen surface numbering equals local OCCT face ids.
- Boundary `.faces.vtk` export with `BoundaryMarker` cell data for visual inspection.
- Load-point picking on the generated boundary surface, mapped to mesh node ids.
- Load direction is not read from the load-point file. It is computed from the picked face normal and flipped toward the camera for visual consistency.
- CalculiX `.inp` export using generated mesh nodes and `C3D4` tetrahedral solid elements.
- CalculiX `ccx` execution through `C:\msys64\ucrt64\bin\ccx.exe` when available.
- CalculiX `.frd` displacement-result import and displacement-magnitude contour display on the generated boundary mesh.

Important solver state:

- The current CalculiX export uses the generated tetrahedral volume mesh as the solver source.
- It writes `*NODE`, `*ELEMENT, TYPE=C3D4`, `*SOLID SECTION`, `*BOUNDARY`, and `*CLOAD`.
- Constraint faces are currently full-fixed displacement groups with `UX/UY/UZ=0`.
- Loads are currently concentrated nodal `*CLOAD` entries on mapped mesh node ids.
- Export and solve require a generated tetrahedral volume mesh, at least one constraint face, and at least one mapped load node.
- Postprocessing maps `.frd` displacement node ids to generated mesh node ids and colors the generated boundary surface. Stress/strain rendering and engineering validation are still future work.

Netgen dependency note:

- The MSYS2 UCRT64 repository checked during development did not provide `mingw-w64-ucrt-x86_64-netgen`.
- Netgen discovery checks `NETGEN_EXE`, app-local `netgen.exe`, `extern\netgen\bin\netgen.exe`, `PATH`, `C:\msys64\ucrt64\bin\netgen.exe`, `NETGEN_PYTHON`, and `extern\netgen\python-env\Scripts\python.exe`.
- The preferred Windows route is `scripts\download_netgen_windows.ps1`, which downloads prebuilt `netgen-mesher` wheels into ignored `extern\netgen` and does not compile Netgen source.

## Development Priorities

Future work should move the prototype toward a structural test-bench workflow:

1. Model import and inspection
   - Keep STEP/STP import reliable.
   - Handle large CAD models without making interaction unusable.
   - Preserve clear reset-view, zoom, pan, and rotate behavior.

2. Loading device management
   - Add a first-class data model for loading devices.
   - Support a fixed configured number of devices.
   - Each loading device should have an id, name, type, position/control point, direction vector, current load, unit, and enabled/disabled state.
   - UI should make it clear which loading device corresponds to which point on the model.

3. Control point workflow
   - Constraint picking should happen on STEP/OCCT source faces before meshing.
   - Load-point picking should happen on the generated boundary mesh after meshing.
   - Picked points should be editable, removable, and assignable to loading devices.
   - Avoid confusing control points, sensors, and loading devices as the same object.

4. Load visualization
   - Load channels should map to loading devices or sensors explicitly.
   - Color contours are useful for visualization but should not be treated as engineering-grade finite element results.
   - Keep units visible, especially force units such as kN.

5. Test-bench realism
   - Prepare the architecture for real hardware integration later.
   - Future integrations may include DAQ, PLC, actuator controllers, closed-loop control, alarms, and data logging.
   - Do not hard-code sample-only assumptions into core data structures.

6. Finite element solver workflow
   - Keep CalculiX integration as a first-class engineering line, not a hidden export utility.
   - Preprocessing should expose boundary conditions, materials, load definitions, and mesh quality clearly.
   - Postprocessing should support displacement first, then stress and strain.
   - Make result visualizations visibly distinct from load interpolation visualizations.
   - Do not claim engineering accuracy until the meshing, material definition, boundary conditions, and validation cases are upgraded.

## UI/UX Direction

This is an engineering operations tool, not a marketing demo.

Prefer:

- Dense but readable panels.
- Clear tables/lists for devices, points, and channels.
- Stable toolbars and explicit modes.
- Numeric coordinates and units.
- Direct manipulation in the 3D viewport.
- Visual distinction between model, loading devices, control points, and sensors.

Avoid:

- Decorative landing pages.
- Large promotional hero sections.
- Ambiguous point types.
- Preloading demo data on startup.
- Huge world-space axes that interfere with the model view.

## Data Model Guidance

When adding new persistent configuration, prefer structured files such as JSON.

Likely future configuration shape:

```json
{
  "loading_devices": [
    {
      "id": "LD01",
      "name": "Loading Device 01",
      "position": [0.0, 0.0, 0.0],
      "direction": [0.0, 0.0, -1.0],
      "unit": "kN",
      "enabled": true
    }
  ]
}
```

Keep this separate from sensor-only configuration unless the user explicitly decides to merge them.

## Engineering Constraints

- Preserve existing user changes in the working tree.
- Keep edits scoped.
- Prefer existing Qt/VTK/OCCT patterns in the codebase.
- Verify builds after C++ changes when possible.
- If the executable is expected to run by double-clicking, deploy Qt plugins and MSYS2 runtime DLLs into the build output directory.
- Keep Windows deployment practical and explain commands clearly when the user is learning the build process.

## Known Development Risks

- Qt + VTK + OCCT deployment on Windows is DLL-heavy.
- MSYS2 UCRT64 and MSVC dependencies are not ABI-compatible.
- VTK template/link behavior on MinGW can be sensitive; prefer stable exported APIs when possible.
- STEP model scale and units may vary between files.
- Picked control points are currently mesh-surface points, not semantic CAD features.
- Future engineering accuracy requires explicit validation beyond visual interpolation.
