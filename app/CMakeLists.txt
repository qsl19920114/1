# Dependency direction from PROJECT_PLAN §6, enforced by target layering rather
# than convention: ui -> domain / backend. A lower layer never links an upper
# one, so an accidental reverse include fails the build instead of rotting.
#
# Only layers with concrete implementations are declared.
# Services persist project metadata and immutable image assets.

add_library(qvw_domain STATIC
  domain/InspectorField.cpp
  domain/Snapshot.cpp
  domain/EditProposal.cpp
)
target_include_directories(qvw_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_domain PUBLIC Qt6::Core)

# Hypit's own field names stay inside this adapter; the UI only sees domain DTOs.
add_library(qvw_backend STATIC
  backend/hypit/SnapshotMapper.cpp
  backend/hypit/StudioClient.cpp
  backend/hypit/StudioWriter.cpp
  backend/hypit/StudioProcess.cpp
)
target_include_directories(qvw_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_backend PUBLIC qvw_domain Qt6::Network)

# Config, logging and the startup self-check. Depends on nothing above it, so it
# stays usable from tests without pulling in the UI.
add_library(qvw_infra STATIC
  infrastructure/AppConfig.cpp
  infrastructure/HypitProbe.cpp
  infrastructure/LogWriter.cpp
  infrastructure/JsonProcess.cpp
)
target_include_directories(qvw_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_infra PUBLIC Qt6::Core)

add_library(qvw_services STATIC services/ProjectStore.cpp services/AssetService.cpp services/SourceEditGuard.cpp services/ExportWorkspace.cpp services/MediaValidation.cpp services/ProposalService.cpp services/ProposalProvider.cpp)
target_include_directories(qvw_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_services PUBLIC qvw_domain qvw_infra Qt6::Gui)

add_library(qvw_ui STATIC
  ui/MainWindow.cpp
  ui/MainWindow.h
  ui/InspectorControls.cpp
)
target_include_directories(qvw_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_ui PUBLIC qvw_domain Qt6::Widgets Qt6::WebEngineWidgets)

add_library(qvw_controllers STATIC controllers/ProjectController.cpp controllers/DocumentController.cpp controllers/EditorController.cpp controllers/ExportController.cpp controllers/ProposalController.cpp)
target_include_directories(qvw_controllers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_controllers PUBLIC qvw_backend qvw_infra qvw_services)

qt_add_executable(qt_video_workbench main.cpp)
target_link_libraries(qt_video_workbench PRIVATE qvw_ui qvw_controllers)
target_compile_definitions(qt_video_workbench PRIVATE QVW_TEMPLATE_DIR="${PROJECT_SOURCE_DIR}/templates")

set_target_properties(qt_video_workbench PROPERTIES
  MACOSX_BUNDLE FALSE
  OUTPUT_NAME qt-video-workbench
)
