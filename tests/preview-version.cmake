add_executable(preview_version_test unit/PreviewVersionTest.cpp)
target_link_libraries(preview_version_test PRIVATE qvw_ui qvw_backend Qt6::Test)
add_test(NAME preview_version COMMAND preview_version_test)
set_tests_properties(preview_version PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu" TIMEOUT 90)
