add_executable(project_store_test unit/ProjectStoreTest.cpp)
target_link_libraries(project_store_test PRIVATE qvw_services Qt6::Test)
add_test(NAME project_store COMMAND project_store_test)
