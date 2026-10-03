add_executable(model_client_test unit/ModelClientTest.cpp)
target_link_libraries(model_client_test PRIVATE qvw_agent Qt6::Test)
add_test(NAME model_client COMMAND model_client_test)
set_tests_properties(model_client PROPERTIES TIMEOUT 20)
