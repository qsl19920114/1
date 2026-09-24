cmake_minimum_required(VERSION 3.21)
project(QtVideoWorkbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# WebEngineWidgets is required by the plan's embedded preview, but T008 only
# builds the shell. It is linked here so a missing Kit fails at configure time
# rather than three milestones later.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets)

qt_standard_project_setup()

add_subdirectory(app)

option(QVW_BUILD_PROBES "Build the M0 verification probes" OFF)
if(QVW_BUILD_PROBES)
  add_subdirectory(probes/qt-webengine)
endif()

enable_testing()
