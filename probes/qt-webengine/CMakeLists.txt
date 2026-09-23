cmake_minimum_required(VERSION 3.21)
project(QtWebEngineProbe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets)

# T005a: load the real Studio in QWebEngineView, capture the painted frame.
add_executable(studio_probe studio_probe.cpp)
target_link_libraries(studio_probe PRIVATE Qt6::Widgets Qt6::WebEngineWidgets)

# T005b: decode the exported MP4 inside QWebEngineView, report codec support.
add_executable(media_probe media_probe.cpp)
target_link_libraries(media_probe PRIVATE Qt6::Widgets Qt6::WebEngineWidgets)

# T004: Qt native HTTP against Studio; checks Host/Origin mutation gate behaviour.
add_executable(http_probe http_probe.cpp)
target_link_libraries(http_probe PRIVATE Qt6::Core Qt6::Network)
