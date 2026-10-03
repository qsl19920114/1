add_executable(agent_panel_test unit/AgentPanelTest.cpp)
target_link_libraries(agent_panel_test PRIVATE qvw_ui Qt6::Test)
add_test(NAME agent_panel COMMAND agent_panel_test)
set_tests_properties(agent_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
