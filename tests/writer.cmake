add_executable(studio_writer_test unit/StudioWriterTest.cpp)
target_link_libraries(studio_writer_test PRIVATE qvw_backend Qt6::Test)
add_test(NAME studio_writer COMMAND studio_writer_test)
