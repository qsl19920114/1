add_executable(history_panel_test unit/HistoryPanelTest.cpp)
target_link_libraries(history_panel_test PRIVATE qvw_ui Qt6::Test)
add_test(NAME history_panel COMMAND history_panel_test)
set_tests_properties(history_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
