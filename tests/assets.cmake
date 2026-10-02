add_executable(asset_service_test unit/AssetServiceTest.cpp)
target_link_libraries(asset_service_test PRIVATE qvw_services Qt6::Test)
add_test(NAME asset_service COMMAND asset_service_test)
