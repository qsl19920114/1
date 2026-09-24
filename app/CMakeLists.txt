# Dependency direction from PROJECT_PLAN §6, enforced by target layering rather
# than convention: ui -> domain / backend. A lower layer never links an upper
# one, so an accidental reverse include fails the build instead of rotting.
#
# Only layers that have real code today are declared. controllers/ and services/
# arrive with T009-T011; placeholder targets would hide what is still missing.

add_library(qvw_domain STATIC
  domain/InspectorField.cpp
  domain/Snapshot.cpp
)
target_include_directories(qvw_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_domain PUBLIC Qt6::Core)

# Hypit's own field names stay inside this adapter; the UI only sees domain DTOs.
add_library(qvw_backend STATIC
  backend/hypit/SnapshotMapper.cpp
)
target_include_directories(qvw_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_backend PUBLIC qvw_domain)

add_library(qvw_ui STATIC
  ui/MainWindow.cpp
  ui/MainWindow.h
)
target_include_directories(qvw_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qvw_ui PUBLIC qvw_domain Qt6::Widgets)

qt_add_executable(qt_video_workbench main.cpp)
target_link_libraries(qt_video_workbench PRIVATE qvw_ui qvw_backend)

set_target_properties(qt_video_workbench PROPERTIES
  MACOSX_BUNDLE FALSE
  OUTPUT_NAME qt-video-workbench
)
