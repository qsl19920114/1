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
target_link_libraries(qvw_backend PUBLIC qvw_domain qvw_infra Qt6::Network)

# Config, logging and the startup self-check. Depends on nothing above it, so it
# stays usable from tests without pulling in the UI.
add_library(qvw_infra STATIC
  infrastructure/WorkspaceStore.cpp
  infrastructure/AppConfig.cpp
  infrastructure/HypitProbe.cpp
  infrastructure/LogWriter.cpp
  infrastructure/JsonProcess.cpp
  infrastructure/RuntimePaths.cpp
)
target_include_directories(qvw_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_infra PUBLIC Qt6::Core)

add_library(qvw_services STATIC services/ProjectStore.cpp services/AssetService.cpp services/SourceEditGuard.cpp services/ExportWorkspace.cpp services/MediaValidation.cpp services/ProposalService.cpp services/ProposalProvider.cpp services/SampleCatalog.cpp services/VideoAssetImporter.cpp services/VideoAssetImporter.h)
target_include_directories(qvw_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_services PUBLIC qvw_domain qvw_infra Qt6::Gui)

add_library(qvw_ui STATIC
  ui/AssetTree.cpp
  ui/HistoryPanel.cpp
  ui/SceneStrip.cpp
  ui/MainWindow.cpp
  ui/MainWindow.h
  ui/InspectorControls.cpp
  ui/StudioTransport.cpp
  ui/MediaPlayerDialog.cpp
  ui/AgentPanel.cpp
  ui/PlanReviewPanel.cpp
)
target_include_directories(qvw_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_ui PUBLIC qvw_domain Qt6::Widgets Qt6::WebEngineWidgets)

add_library(qvw_controllers STATIC controllers/ProjectController.cpp controllers/DocumentController.cpp controllers/EditorController.cpp controllers/ExportController.cpp controllers/ProposalController.cpp controllers/SampleCreationController.cpp)
target_include_directories(qvw_controllers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_controllers PUBLIC qvw_backend qvw_infra qvw_services)

add_library(qvw_agent STATIC agent/PlanService.cpp agent/ContextBuilder.cpp agent/StoryTemplate.cpp agent/ModelClient.cpp agent/AgentTaskStore.cpp agent/AgentController.cpp agent/ToolDispatcher.cpp)
target_include_directories(qvw_agent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_agent PUBLIC qvw_controllers Qt6::Gui)

qt_add_executable(qt_video_workbench main.cpp)
target_link_libraries(qt_video_workbench PRIVATE qvw_ui qvw_agent)
target_compile_definitions(qt_video_workbench PRIVATE QVW_APP_VERSION="${PROJECT_VERSION}")

set_target_properties(qt_video_workbench PROPERTIES
  MACOSX_BUNDLE ${APPLE}
  MACOSX_BUNDLE_GUI_IDENTIFIER "edu.qvw.workbench"
  MACOSX_BUNDLE_BUNDLE_NAME "FrameLab"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  OUTPUT_NAME qt-video-workbench
)
if(APPLE)
  set(qvw_resource_directory "$<TARGET_BUNDLE_DIR:qt_video_workbench>/Contents/Resources")
else()
  set(qvw_resource_directory "$<TARGET_FILE_DIR:qt_video_workbench>/resources")
endif()
add_custom_command(TARGET qt_video_workbench POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${qvw_resource_directory}/templates"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/templates" "${qvw_resource_directory}/templates"
  COMMENT "Copy original templates beside application runtime")
