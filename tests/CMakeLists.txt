include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(test_storage_full test_storage_full.cpp)
target_link_libraries(test_storage_full PRIVATE storage gtest_main)
target_include_directories(test_storage_full PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_buffer_manager test_buffer_manager.cpp)
target_link_libraries(test_buffer_manager PRIVATE buffer gtest_main)
target_include_directories(test_buffer_manager PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_bplus_tree test_bplus_tree.cpp)
target_link_libraries(test_bplus_tree PRIVATE index gtest_main)
target_include_directories(test_bplus_tree PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_buffer_metrics test_buffer_metrics.cpp)
target_link_libraries(test_buffer_metrics PRIVATE buffer gtest_main)
target_include_directories(test_buffer_metrics PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_query_basic test_query_basic.cpp)
target_link_libraries(test_query_basic PRIVATE query gtest_main)
target_include_directories(test_query_basic PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(test_storage_full)
gtest_discover_tests(test_buffer_manager)
gtest_discover_tests(test_bplus_tree)
gtest_discover_tests(test_buffer_metrics)
gtest_discover_tests(test_query_basic)
